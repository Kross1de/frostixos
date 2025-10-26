%define MULTIBOOT_HEADER_MAGIC    0x1BADB002
%define MULTIBOOT_PAGE_ALIGN      0x00000001  ; align loaded modules on page boundaries
%define MULTIBOOT_MEMORY_INFO     0x00000002  ; provide memory map
%define MULTIBOOT_VIDEO_MODE      0x00000004  ; request video mode information
%define MULTIBOOT_FLAGS           (MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_VIDEO_MODE)
%define MULTIBOOT_CHECKSUM        -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_FLAGS)

section .multiboot
align 4
    dd MULTIBOOT_HEADER_MAGIC     ; magic number
    dd MULTIBOOT_FLAGS            ; flags
    dd MULTIBOOT_CHECKSUM         ; checksum
    
    dd 0, 0, 0, 0, 0              ; header_addr, load_addr, load_end_addr, bss_end_addr, entry_addr
    
    dd 0                          ; mode_type (0 = linear graphics mode, 1 = text mode)
    dd 1024                       ; width (preferred)
    dd 768                        ; height (preferred)  
    dd 32                         ; depth (bits per pixel)

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start:function (_start.end - _start)
extern kernel_main

_start:
    mov esp, stack_top

    push ebx                      ; multiboot info pointer
    push eax                      ; multiboot magic value

    call kernel_main

    cli                           ; disable interrupts
.hang:
    hlt                           ; halt the CPU
    jmp .hang                     ; jump back to .hang if NMI occurs
.end:


global halt_cpu:function
halt_cpu:
    cli
    hlt
    jmp halt_cpu