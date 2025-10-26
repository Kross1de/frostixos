section .text
global idt_flush

; idt_flush: load IDTR
; arg: pointer to idt_ptr on stack
idt_flush:
	mov	eax, [esp + 4]
	lidt	[eax]
	ret