# adjust CROSS if your toolchain prefix differs
CROSS ?= riscv64-unknown-elf-
CC      := $(CROSS)gcc
OBJCOPY := $(CROSS)objcopy

CFLAGS  := -march=rv64gc -mabi=lp64d -nostdinc -nostdlib -ffreestanding \
           -fno-builtin -O2 -Wall -Wextra -mcmodel=medany
LDFLAGS := -T linker.ld -nostdlib -static

SRCS    := boot.S libstr.c io.c fs.c cmd.c kernel.c
OBJS    := $(SRCS:.c=.o)
OBJS    := $(OBJS:.S=.o)

TARGET  := kernel.elf

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) linker.ld
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

clean:
	rm -f *.o $(TARGET)

