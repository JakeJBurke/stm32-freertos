#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// RCC registers
#define RCC_AHB1ENR   (*((volatile uint32_t *)0x40023830))
#define RCC_APB1ENR   (*((volatile uint32_t *)0x40023840))

// GPIOB registers (PB6=SCL, PB7=SDA)
#define GPIOB_MODER   (*((volatile uint32_t *)0x40020400))
#define GPIOB_OTYPER  (*((volatile uint32_t *)0x40020404))
#define GPIOB_OSPEEDR (*((volatile uint32_t *)0x40020408))
#define GPIOB_PUPDR   (*((volatile uint32_t *)0x4002040C))
#define GPIOB_AFRL    (*((volatile uint32_t *)0x40020420))

// I2C1 registers
#define I2C1_CR1      (*((volatile uint32_t *)0x40005400))
#define I2C1_CR2      (*((volatile uint32_t *)0x40005404))
#define I2C1_SR1      (*((volatile uint32_t *)0x40005414))
#define I2C1_SR2      (*((volatile uint32_t *)0x40005418))
#define I2C1_CCR      (*((volatile uint32_t *)0x4000541C))
#define I2C1_TRISE    (*((volatile uint32_t *)0x40005420))
#define I2C1_DR       (*((volatile uint32_t *)0x40005410))

// USART2 registers
#define USART2_SR     (*((volatile uint32_t *)0x40004400))
#define USART2_DR     (*((volatile uint32_t *)0x40004404))
#define USART2_BRR    (*((volatile uint32_t *)0x40004408))
#define USART2_CR1    (*((volatile uint32_t *)0x4000440C))

// MPU-6050
#define MPU6050_ADDR  0x68
#define MPU_PWR_MGMT  0x6B
#define MPU_ACCEL_X_H 0x3B

// Sensor data structure
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} AccelData;

// Queue handles
QueueHandle_t sensorQueue;
QueueHandle_t filterQueue;

// UART functions
void uart_send_byte(char c) {
    while (!(USART2_SR & (1 << 7)));
    USART2_DR = c;
}

void uart_send_string(const char* s) {
    while (*s) uart_send_byte(*s++);
}

void uart_send_number(int32_t num) {
    char buf[12];
    int i = 0;
    if (num < 0) { uart_send_byte('-'); num = -num; }
    if (num == 0) { uart_send_byte('0'); return; }
    while (num > 0) { buf[i++] = '0' + (num % 10); num /= 10; }
    while (i > 0) uart_send_byte(buf[--i]);
}

// I2C functions
void i2c_init(void) {
    RCC_AHB1ENR |= (1 << 1);
    RCC_APB1ENR |= (1 << 21);

    GPIOB_MODER  &= ~(0xF << 12);
    GPIOB_MODER  |=  (0xA << 12);
    GPIOB_OTYPER |=  (0x3 << 6);
    GPIOB_OSPEEDR|=  (0xF << 12);
    GPIOB_PUPDR  &= ~(0xF << 12);
    GPIOB_AFRL   &= ~(0xFF << 24);
    GPIOB_AFRL   |=  (0x44 << 24);

    I2C1_CR1  &= ~(1 << 0);
    I2C1_CR2   =  16;
    I2C1_CCR   =  80;
    I2C1_TRISE =  17;
    I2C1_CR1  |=  (1 << 0);
}

void i2c_start(void) {
    I2C1_CR1 |= (1 << 8);
    while (!(I2C1_SR1 & (1 << 0)));
}

void i2c_stop(void) {
    I2C1_CR1 |= (1 << 9);
}

void i2c_send_addr(uint8_t addr, uint8_t rw) {
    I2C1_DR = (addr << 1) | rw;
    while (!(I2C1_SR1 & (1 << 1)));
    (void)I2C1_SR1;
    (void)I2C1_SR2;
}

void i2c_send_byte(uint8_t data) {
    while (!(I2C1_SR1 & (1 << 7)));
    I2C1_DR = data;
    while (!(I2C1_SR1 & (1 << 2)));
}

uint8_t i2c_read_byte(uint8_t ack) {
    if (ack) I2C1_CR1 |=  (1 << 10);
    else     I2C1_CR1 &= ~(1 << 10);
    while (!(I2C1_SR1 & (1 << 6)));
    return I2C1_DR;
}

void mpu6050_write(uint8_t reg, uint8_t data) {
    i2c_start();
    i2c_send_addr(MPU6050_ADDR, 0);
    i2c_send_byte(reg);
    i2c_send_byte(data);
    i2c_stop();
}

uint8_t mpu6050_read(uint8_t reg) {
    uint8_t data;
    i2c_start();
    i2c_send_addr(MPU6050_ADDR, 0);
    i2c_send_byte(reg);
    i2c_start();
    i2c_send_addr(MPU6050_ADDR, 1);
    data = i2c_read_byte(0);
    i2c_stop();
    return data;
}

// Task 1 — Sensor Task
void SensorTask(void *pvParameters) {
    AccelData data;

    mpu6050_write(MPU_PWR_MGMT, 0x00);

    while (1) {
        data.x = ((int16_t)mpu6050_read(MPU_ACCEL_X_H) << 8)
                | mpu6050_read(MPU_ACCEL_X_H + 1);
        data.y = ((int16_t)mpu6050_read(MPU_ACCEL_X_H + 2) << 8)
                | mpu6050_read(MPU_ACCEL_X_H + 3);
        data.z = ((int16_t)mpu6050_read(MPU_ACCEL_X_H + 4) << 8)
                | mpu6050_read(MPU_ACCEL_X_H + 5);

        xQueueSend(sensorQueue, &data, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Task 2 — Filter Task
void FilterTask(void *pvParameters) {
    AccelData raw;
    AccelData filtered;
    int32_t x_buf[5] = {0};
    int32_t y_buf[5] = {0};
    int32_t z_buf[5] = {0};
    int idx = 0;

    while (1) {
        if (xQueueReceive(sensorQueue, &raw, portMAX_DELAY)) {
            x_buf[idx] = raw.x;
            y_buf[idx] = raw.y;
            z_buf[idx] = raw.z;
            idx = (idx + 1) % 5;

            int32_t x_sum = 0, y_sum = 0, z_sum = 0;
            for (int i = 0; i < 5; i++) {
                x_sum += x_buf[i];
                y_sum += y_buf[i];
                z_sum += z_buf[i];
            }

            filtered.x = x_sum / 5;
            filtered.y = y_sum / 5;
            filtered.z = z_sum / 5;

            xQueueSend(filterQueue, &filtered, 0);
        }
    }
}

// Task 3 — Logger Task
void LoggerTask(void *pvParameters) {
    AccelData data;

    while (1) {
        if (xQueueReceive(filterQueue, &data, portMAX_DELAY)) {
            uart_send_string("X: ");
            uart_send_number(data.x);
            uart_send_string("  Y: ");
            uart_send_number(data.y);
            uart_send_string("  Z: ");
            uart_send_number(data.z);
            uart_send_string("\r\n");
        }
    }
}

int main(void) {
    // UART init
    RCC_AHB1ENR |= (1 << 0);
    RCC_APB1ENR |= (1 << 17);

    (*((volatile uint32_t *)0x40020000)) &= ~(3 << 4);
    (*((volatile uint32_t *)0x40020000)) |=  (2 << 4);
    (*((volatile uint32_t *)0x40020020)) &= ~(0xF << 8);
    (*((volatile uint32_t *)0x40020020)) |=  (7 << 8);

    USART2_BRR = 0x683;
    USART2_CR1 = (1 << 3) | (1 << 13);

    // I2C init
    i2c_init();

    // Create queues
    sensorQueue = xQueueCreate(5, sizeof(AccelData));
    filterQueue = xQueueCreate(5, sizeof(AccelData));

    // Create tasks
    xTaskCreate(SensorTask,  "Sensor",  256, NULL, 3, NULL);
    xTaskCreate(FilterTask,  "Filter",  256, NULL, 2, NULL);
    xTaskCreate(LoggerTask,  "Logger",  256, NULL, 1, NULL);

    // Start FreeRTOS scheduler
    vTaskStartScheduler();

    while (1);

    return 0;
}