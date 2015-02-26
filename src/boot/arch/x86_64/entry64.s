/*_
 * Copyright (c) 2015 Hirochika Asai <asai@jar.jp>
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

	.text

	.code64
	.globl	entry64
	.globl	_ljmp
	.globl	_hlt
	.globl	_intr_null
	.globl	_intr_irq6
	.globl	_outb
	.globl	_inb

/* Entry point */
entry64:
	cli

	xorl	%eax,%eax
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%ss
	movl	%eax,%fs
	movl	%eax,%gs

	call	remap_pic

	/* Clear screen */
	movl	$0xb8000,%edi
	movw	$0x0f20,%ax
	movl	$80*25,%ecx
	rep	stosw

	/* Reset cursor */
	movw	$0x000f,%ax	/* %al=0xf: cursor location low, %ah: xx00 */
	movw	$0x3d4,%dx
	outw	%ax,%dx
	movw	$0x000e,%ax	/* %al=0xe: cursor location high, %ah: 00xx */
	movw	$0x3d4,%dx
	outw	%ax,%dx

	/* Unmask all */
	movb	$0x00,%al
	outb	%al,$0x21
	movb	$0x00,%al
	outb	%al,$0xa1

	/* Get into the C code */
	call	_centry

1:
	hlt
	jmp	1b

/* Remap PIC */
remap_pic:
	pushw	%ax
	pushw	%bx
	/* Remap PIC */
	inb	$0x21,%al	/* Save the current mask of the master PIC */
	movb	%al,%bl
	inb	$0xa1,%al	/* Save the current mask of the slave PIC */
	movb	%al,%bh
	movb	$0x11,%al	/* Init w/ ICW4 */
	outb	%al,$0x20	/*  Restart the master PIC */
	outb	%al,$0xa0	/*  Restart the slave PIC */
	movb	$0x20,%al
	outb	%al,$0x21	/* ICW2: Set to route IRQ0-7 to #0x20-0x27 */
	movb	$0x28,%al
	outb	%al,$0xa1	/* ICW2: Set to route IRQ8-15 to #0x28-0x2f */
	movb	$0x04,%al
	outb	%al,$0x21	/* ICW3: Cascade setting */
	movb	$0x02,%al
	outb	%al,$0xa1	/* ICW3 */
	movb	$0x01,%al
	outb	%al,$0x21	/* ICW4 */
	outb	%al,$0xa1	/* ICW4 */
	movb	%bl,%al
	outb	%al,$0x21	/* Restore the mask */
	movb	%bh,%al
	outb	%al,$0xa1	/* Restore the mask */
	popw	%bx
	popw	%ax
	ret


/* void ljmp(u64 selector, u64 address); */
_ljmp:
	pushq	%rdi
	pushq	%rsi
	lretq

/* void hlt(void); */
_hlt:
	hlt
	ret

/* u8 inb(u16 port); */
_inb:
	movw	%di,%dx
	inb	%dx,%al
	ret

/* void outb(u16 port, u8 value); */
_outb:
	movw	%di,%dx
	movw	%si,%ax
	outb	%al,%dx
	ret


/* Interrupt handlers */
_intr_null:
	pushq	%rax
	pushq	%rdx
	movb	$0x20,%al	/* Notify */
	outb	%al,$0x20	/*  End-Of-Interrupt (EOI) */
	popq	%rdx
	popq	%rax
	iretq

	/* Beginning of interrupt handler */
	.macro  intr_pic_isr vec
	pushq	%rax
	pushq	%rbx
	pushq	%rcx
	pushq	%rdx
	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	pushq	%rsi
	pushq	%rdi
	pushq	%rbp
	pushw	%fs
	pushw	%gs
	movq	$\vec,%rdi
	call	_isr
	movb	$0x20,%al	/* Notify */
	outb	%al,$0x20	/*  End-Of-Interrupt (EOI) */
	.endm

	.macro  intr_pic_isr_done
	/* Pop all registers from stackframe */
	popw	%gs
	popw	%fs
	popq	%rbp
	popq	%rdi
	popq	%rsi
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rdx
	popq	%rcx
	popq	%rbx
	popq	%rax
	.endm

_intr_irq0:
	intr_pic_isr 32
	intr_pic_isr_done
	iretq

_intr_irq1:
	intr_pic_isr 33
	intr_pic_isr_done
	iretq

_intr_irq2:
	intr_pic_isr 34
	intr_pic_isr_done
	iretq

_intr_irq3:
	intr_pic_isr 35
	intr_pic_isr_done
	iretq

_intr_irq4:
	intr_pic_isr 36
	intr_pic_isr_done
	iretq

_intr_irq5:
	intr_pic_isr 37
	intr_pic_isr_done
	iretq

_intr_irq6:
	intr_pic_isr 38
	intr_pic_isr_done
	iretq

_intr_irq7:
	intr_pic_isr 39
	intr_pic_isr_done
	iretq

_intr_irq8:
	intr_pic_isr 40
	intr_pic_isr_done
	iretq

_intr_irq9:
	intr_pic_isr 41
	intr_pic_isr_done
	iretq

_intr_irq10:
	intr_pic_isr 42
	intr_pic_isr_done
	iretq

_intr_irq11:
	intr_pic_isr 43
	intr_pic_isr_done
	iretq

_intr_irq12:
	intr_pic_isr 44
	intr_pic_isr_done
	iretq

_intr_irq13:
	intr_pic_isr 45
	intr_pic_isr_done
	iretq

_intr_irq14:
	intr_pic_isr 46
	intr_pic_isr_done
	iretq

_intr_irq15:
	intr_pic_isr 47
	intr_pic_isr_done
	iretq
