# OS plan

## Video
- [x] VGA text
- [x] VBE support
- [ ] PSF font support
- ... (maybe)
## MM
- [x] PMM
- [x] VMM
- [x] heap
- [x] VMA (Virtual Memory Areas)
- [ ] VMO (Virtual Memory Objects)
- [x] slab allocator
- [ ] demand paging
- [ ] memory debugging
- [ ] copy-on-write
- [ ] swap/paging device
- [ ] memory hotplug
- ...
## Arch
- [x] serial
- [x] GDT
- [x] IDT
- [x] ISR
- [x] IRQ
- [x] ACPI tables parsing (FADT, MADT) (WIP)
- [x] RTC
- [x] CPUID
- [ ] Memory maps
    - [x] e820
    - [ ] UEFI memory map - for modern bootloaders (maybe)
    - [ ] Device tree support (DTB)
    - [ ] /proc-like memory info
- [ ] TSS
- [ ] syscalls
- [ ] multiboot2 support
- [ ] safe reboot/shutdown
- ...
## Drivers
- [x] PIC
- [ ] PS2
    - [x] keyboard
    - [ ] mouse
- [ ] PCI enumeration and device discovery
- [ ] IDE/ATA
- [ ] CMOS
- [ ] AHCI/SATA
- [ ] USB basics
- [ ] USB XHCI / EHCI stacked drivers
- [ ] PCIe
- [ ] virtio (for virtualization & QEMU)
- [ ] network drivers (RTL8139, e1000, virtio-net)
- ...
## Filesystem
- [ ] VFS
- [ ] FATFS
- [ ] EXT2
- [x] initrd
    - [x] tar
    - [ ] cpio (maybe)
- [ ] block device abstraction
- [ ] initrd mounting
- [ ] ramdisk persistence
- [ ] filesystem caching / page cache
- [ ] journaling support
- ... (maybe)
## Kernel core
- [x] logger
- [ ] module loader
- [x] symbol export table / ksym (TOOD: export symbols)
- [ ] dynamic kernel modules
- [ ] kernel configuration (Kconfig-like)
- [x] kernel panic
## Networking
- [ ] TCP/IP stack
- [ ] ethernet driver (RTL8139, e.g.)
- [ ] DHCP client
- [ ] socket API
- [ ] UDP
- [ ] ARP, ICMP
- [ ] IPv6 support
- [ ] basic userspace net tools (ping, ifconfig)
## userland & UX
- [ ] multitasking
    - [ ] process management (creation, termination)
    - [ ] scheduler (preemptive, O(1)/CFS)
    - [ ] threads (maybe)
- [ ] user mode support
- [ ] ELF loader
- [x] shell
- [ ] basic libc / posix subset
- [ ] init/systemd-like init
- [ ] package manager / packaging format
- [ ] login/TTY manager
- [ ] GUI/windowing / compositor
- [ ] sound support
- [ ] SMP
- [ ] containers / namespaces
- ...
## build / CI / tests
- [ ] reproducible build system
- [ ] cross-compile toolchain docs
- [ ] unit tests for core components
- [ ] integration tests (QEMU boot tests)
- [ ] CI pipelines (build + QEMU smoke tests)
- [ ] fuzzing for syscall / drivers
- [ ] static analysis / clang-tidy
## security / stability
- [ ] KASLR / ASLR
- [ ] secureboot / signatures
- [ ] mitigations (SMEP/SMAP, NX)
- [ ] address sanitizers for kernel
- [ ] exploit mitigations and hardening
## tooling / developer experience
- [ ] code coverage tooling
- [ ] profiling hooks
- [ ] tracing (ftrace-like)
- [ ] documentation generation (Doxygen / mdbook)
## documentation
- [ ] contributor guide
- [ ] design docs (scheduler, memory model, VFS)
- [ ] API docs for kernel subsystems
- [ ] how to build / boot with QEMU / real hardware
## nice to have
- [ ] basic scripting
- [ ] installer
- [ ] live-upgrade flow
- [ ] QEMU user-mode emulation helpers
