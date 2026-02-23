# byteOS <img width="1248" height="960" alt="image" src="https://github.com/user-attachments/assets/cda470b4-01d5-41c0-b06d-894fccc8aa09" />

<p>Independent OS in C++. Also try Linux!</p>

MIT/byteOS is an independent operating system written by one person (me), it currently has working keyboard, a command handling function (currently there are commands called help, neofetch, clear, reboot, whoami, touch, ls, echo, cat, write, rm) a RAM file system, and a bootloader in ASM

Here are the compilation instructions:

BOOTLOADER

`i686-elf-as boot.s -o boot.o`

KERNEL

`gcc -m32 -ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -nostdlib -c kernel.cpp -o kernel.o`

LINKING

`ld -m elf_i386 -T linker.ld -o byte.bin boot.o kernel.o`

BOOTING (QEMU)

`qemu-system-i386 -kernel byte.bin`

And if you are lazy, a pre-compiled version will always be available in the [releases](https://github.com/nebulingular/byteOS/releases) tab


The current version is version 0.2 (codename and kernel name nbla)

![byteos 0.2](https://github.com/user-attachments/assets/2ff8a43e-4afb-49f8-a51a-30e9737e6c70)
