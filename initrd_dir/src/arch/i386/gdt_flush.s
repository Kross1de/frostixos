section .text
global gdt_flush

; gdt_flush: load GDTR and update segment registers
; arg: pointer to gdt_ptr on stack
gdt_flush:
	mov	eax, [esp + 4]
	lgdt	[eax]

	mov	ax, 0x10
	mov	ds, ax
	mov	es, ax
	mov	fs, ax
	mov	gs, ax
	mov	ss, ax

	jmp	0x08:.gdt_flush_complete

.gdt_flush_complete:
	ret