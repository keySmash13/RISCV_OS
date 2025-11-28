qemu-system-riscv64 \
    -machine virt \
    -m 128M \
    -bios /usr/share/qemu/opensbi-riscv64-generic-fw_dynamic.bin \
    -kernel kernel.elf \
    -serial mon:stdio 
