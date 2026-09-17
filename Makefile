TARGET = main
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

FREERTOS = FreeRTOS-Kernel

CFLAGS = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
         -O0 -g -Wall -nostdlib \
         -I$(FREERTOS)/include \
         -I$(FREERTOS)/portable/GCC/ARM_CM4F \
         -Isrc

LDFLAGS = -T linker.ld -nostdlib -lgcc

FREERTOS_SRCS = \
    $(FREERTOS)/tasks.c \
    $(FREERTOS)/queue.c \
    $(FREERTOS)/list.c \
    $(FREERTOS)/timers.c \
    $(FREERTOS)/portable/GCC/ARM_CM4F/port.c \
    $(FREERTOS)/portable/MemMang/heap_4.c

SRCS = src/main.c src/memfuncs.c $(FREERTOS_SRCS)

OBJS = startup.o $(SRCS:.c=.o)

all: $(TARGET).elf $(TARGET).bin

startup.o: startup.s
	$(CC) $(CFLAGS) -c -o startup.o startup.s

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET).elf $(OBJS)

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).bin

flash: $(TARGET).bin
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
	-c "adapter speed 480; init; halt; program $(TARGET).bin 0x08000000 verify reset exit"

clean:
	rm -f startup.o $(TARGET).elf $(TARGET).bin
	find . -name "*.o" -delete
