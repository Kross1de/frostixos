# OS plan

## Video
- [x] VGA text
- [x] VBE support
- ... (maybe)
## MM
- [x] PMM
- [x] VMM
- [x] heap
- [ ] VMA (Virtual Memory Areas)
- [ ] VMO (Virtual Memory Objects)
- [ ] slab allocator
- ...
## Arch
- [x] serial
- [x] GDT
- [x] IDT
- [x] ISR
- [x] IRQ
- [x] ACPI tables parsing (FADT, MADT)
- [x] RTC
- [ ] CPUID
- [ ] TSS
- [ ] syscalls
- ...
## Drivers
- [x] PIC
- [ ] PS2
- [ ] PCI enumeration
- [ ] IDE/ATA
- [ ] CMOS
- ...
## Filesystem
- [ ] VFS
- [ ] FATFS
- [ ] EXT2
- [ ] initrd
- [ ] block device abstraction
- ... (maybe)
## Kernel core
- [ ] c standard libs
- [x] logger
## Networking
- [ ] TCP/IP stack
- [ ] ethernet driver (RTL8139, e.g.)
## nice to have
- [ ] multitasking
    - [ ] process management (creation, termination)
    - [ ] scheduler
    - [ ] threads (maybe)
- [ ] sound support
- [ ] UI
- [ ] SMP
- [ ] user mode support
- [ ] shell
- [ ] elf loader
- [ ] USB support
