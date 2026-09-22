# FreeRTOS Sensor Pipeline — STM32F411

Bare-metal FreeRTOS sensor pipeline written in C for the STM32F411CEU6
microcontroller. MPU-6050 accelerometer data is read over I2C, passed
between three FreeRTOS tasks using queues, filtered with a moving
average, and transmitted over UART. No STM32 HAL used — direct
register configuration only.

## What This Demonstrates

- FreeRTOS task creation and priority scheduling
- Inter-task communication using FreeRTOS queues
- Blocking task synchronization with xQueueReceive()
- Bare-metal I2C1 driver for MPU-6050 communication
- 3-axis accelerometer data acquisition
- 5-sample moving average filtering
- Bare-metal USART2 serial output
- Direct STM32 peripheral register configuration

## Why FreeRTOS

A traditional superloop handles sensor acquisition, processing, and
output sequentially. FreeRTOS separates these operations into
independent tasks. SensorTask reads the MPU-6050, FilterTask processes
the measurements, and LoggerTask sends the results over UART. Queues
transfer data safely between each stage while blocking tasks that are
waiting for new data.

## Build and Flash

make

Flash with ST-Link V2 and OpenOCD.

## Hardware

- WeAct STM32F411CEU6 Black Pill
- MPU-6050 accelerometer/gyroscope
- ST-Link V2 programmer
- CP2102 USB-UART adapter
- MPU-6050 SCL connected to PB6
- MPU-6050 SDA connected to PB7
- CP2102 RXD connected to PA2
- Common ground between devices

## Tools

- arm-none-eabi-gcc
- GNU Make
- OpenOCD
- FreeRTOS
- STM32F411 reference manual
- MPU-6050 register map

## Status

Firmware implementation complete. Hardware verification of the
MPU-6050 sensor pipeline and UART output is pending.
