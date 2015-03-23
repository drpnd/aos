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

	.include	"asmconst.h"
	.text

	.code64
	.globl	kstart64
	.globl	apstart64
	.globl	_halt
	.globl	_pause
	.globl	_lgdt
	.globl	_lidt
	.globl	_lldt
	.globl	_ltr
	.globl	_sti
	.globl	_cli
	.globl	_inb
	.globl	_inw
	.globl	_inl
	.globl	_outb
	.globl	_outw
	.globl	_outl
	.globl	_mfread32
	.globl	_mfwrite32
	.globl	_kmemset
	.globl	_kmemcmp
	.globl	_asm_ioapic_map_intr
	.globl	_intr_null
	.globl	_intr_apic_loc_tmr

	.set	APIC_LAPIC_ID,0x020
	.set	APIC_EOI,0x0b0
	.set	MSR_APIC_BASE,0x1b

/* Entry point to the 64-bit kernel */
kstart64:
	/* Initialize the bootstrap processor */
	call	_bsp_init
	/* Start the kernel code */
	call	_kmain
	jmp	_halt

/* Entry point for the application processors */
apstart64:
	/* Initialize the application processor */
	call	_ap_init
	/* Start the kernel code */
	call	_kmain
	jmp	_halt

/* void halt(void) */
_halt:
	nop
	hlt
	nop
	nop
	ret

/* void pause(void) */
_pause:
	pause
	ret

/* void lgdt(void *gdtr, u64 selector) */
_lgdt:
	lgdt	(%rdi)
	/* Reload GDT */
	pushq	%rsi
	pushq	$1f	/* Just to do ret */
	lretq
1:
	/* Set data selector */
	movq	%rsi,%rax
	addq	$8,%rax
	movq	%rax,%ds
	movq	%rax,%es
	movq	%rax,%ss
	ret

/* void lidt(void *idtr) */
_lidt:
	lidt	(%rdi)
	ret

/* void lldt(u16) */
_lldt:
	lldt	%di
	ret

/* void ltr(u16) */
_ltr:
	ltr	%di
	ret

/* void sti(void) */
_sti:
	sti
	ret

/* void cli(void) */
_cli:
	cli
	ret

/* u8 inb(u16 port) */
_inb:
	movw	%di,%dx
	xorq	%rax,%rax
	inb	%dx,%al
	ret

/* u16 inw(u16 port) */
_inw:
	movw	%di,%dx
	xorq	%rax,%rax
	inw	%dx,%ax
	ret

/* u32 inw(u16 port) */
_inl:
	movw	%di,%dx
	xorq	%rax,%rax
	inl	%dx,%eax
	ret

/* void outb(u16 port, u8 value) */
_outb:
	movw	%di,%dx
	movw	%di,%ax
	outb	%al,%dx
	ret

/* void outw(u16 port, u16 value) */
_outw:
	movw	%di,%dx
	movw	%di,%ax
	outw	%ax,%dx
	ret

/* void outl(u16 port, u32 value) */
_outl:
	movw	%di,%dx
	movl	%edi,%eax
	outl	%eax,%dx
	ret

/* u32 mfread32(u64 addr) */
_mfread32:
	mfence			/* Prevent out-of-order execution */
	movl	(%rdi),%eax
	ret

/* void mfwrite32(u64 addr, u32 val) */
_mfwrite32:
	mfence			/* Prevent out-of-order execution */
	movl	%esi,(%rdi)
	ret

/* void * kmemset(void *b, int c, size_t len) */
_kmemset:
	pushq	%rdi
	pushq	%rsi
	movl	%esi,%eax	/* c */
	movq	%rdx,%rcx	/* len */
	cld			/* Ensure the DF cleared */
	rep	stosb		/* Set %al to (%rdi)-(%rdi+%rcx) */
	popq	%rsi
	popq	%rdi
	movq	%rdi,%rax	/* Restore for the return value */
	ret

/* int kmemcmp(void *s1, void *s2, size_t n) */
_kmemcmp:
	xorq	%rax,%rax
	movq	%rdx,%rcx	/* n */
	cld			/* Ensure the DF cleared */
	repe	cmpsb		/* Compare byte at (%rsi) with byte at (%rdi) */
	jz	1f
	decq	%rdi		/* rollback one */
	decq	%rsi		/* rollback one */
	movb	(%rdi),%al	/* *s1 */
	subb	(%rsi),%al	/* *s1 - *s2 */
1:
	ret


/* void asm_ioapic_map_intr(u64 val, u64 tbldst, u64 ioapic_base) */
_asm_ioapic_map_intr:
	/* Copy arguments */
	movq	%rdi,%rax       /* src */
	movq	%rsi,%rcx       /* tbldst */
	/* rdx = ioapic_base */

	/* *(u32 *)(ioapic_base + 0x00) = tbldst * 2 + 0x10 */
	shlq	$1,%rcx         /* tbldst * 2 */
	addq	$0x10,%rcx      /* tbldst * 2 + 0x10 */
	sfence
	movl	%ecx,0x00(%rdx) /* IOREGSEL (0x00) */
	/* *(u32 *)(ioapic_base + 0x10) = (u32)src */
	sfence
	movl	%eax,0x10(%rdx) /* IOWIN (0x10) */
	shrq	$32,%rax
	/* *(u32 *)(ioapic_base + 0x00) = tbldst * 2 + 0x10 + 1 */
	addq	$1,%rcx         /* tbldst * 2 + 0x10 + 1 */
	sfence
	movl	%ecx,0x00(%rdx)
	/* *(u32 *)(ioapic_base + 0x10) = (u32)(src >> 32) */
	sfence
	movl	%eax,0x10(%rdx)
	ret


/* Null function for interrupt handler */
_intr_null:
	/* APIC EOI */
	movq	$MSR_APIC_BASE,%rcx
	rdmsr			/* Read APIC info to [%edx:%eax]; N.B., higer */
				/*  32 bits of %rax and %rdx are cleared */
				/*  bit [35:12]: APIC Base, [11]: EN */
				/*  [10]: EXTD, and [8]:BSP*/
	shlq	$32,%rdx
	addq	%rax,%rdx
	andq	$0xfffffffffffff000,%rdx	/* APIC Base */
	movl	$0,APIC_EOI(%rdx)	/* EOI */
	iretq


/* macro to save registers to the stackframe and call the interrupt handler */
.macro	intr_lapic_isr vec
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
	/* EOI for the local APIC */
	movq	$MSR_APIC_BASE,%rcx
	rdmsr			/* Read APIC info to [%edx:%eax]; N.B., higer */
				/*  32 bits of %rax and %rdx are cleared */
				/*  bit [35:12]: APIC Base, [11]: EN */
				/*  [10]: EXTD, and [8]:BSP*/
	shlq	$32,%rdx
	addq	%rax,%rdx
	andq	$0xfffffffffffff000,%rdx	/* APIC Base */
	movl	$0,APIC_EOI(%rdx)	/* EOI */
.endm

/* macro to restore from the stackframe */
.macro	intr_lapic_isr_done
	/* Pop all registers from the stackframe */
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

/* Interrupt handler for local APIC timer */
_intr_apic_loc_tmr:
	intr_lapic_isr 0x50
	jmp	_task_restart

/* Task restart */
_task_restart:
	/* Get the APIC ID */
	movq	$MSR_APIC_BASE,%rcx
	rdmsr
	shlq	$32,%rdx
	addq	%rax,%rdx
	andq	$0xfffffffffffff000,%rdx	/* APIC Base */
	xorq	%rax,%rax
	movl	APIC_LAPIC_ID(%rdx),%eax
	/* Calculate the processor data space from the APIC ID */
	movq	$P_DATA_SIZE,%rbx
	mulq	%rbx		/* [%rdx:%rax] = %rax * %rbx */
	movq	%rax,%rbp
	/* If the next task is not scheduled, immediately restart this task */
	cmpq	$0,P_NEXT_TASK_OFFSET(%rbp)
	jz	2f
	/* If the current task is null, then do not need to save anything */
	cmpq	$0,P_CUR_TASK_OFFSET(%rbp)
	jz	1f
	/* Save the stack pointer (restart point) */
	movq	P_CUR_TASK_OFFSET(%rbp),%rax
	movq	%rsp,TASK_RP(%rax)
1:
	/* Notify that the current task is switched (to the kernel) */
	movq	P_CUR_TASK_OFFSET(%rbp),%rdi
	movq	P_NEXT_TASK_OFFSET(%rbp),%rsi
	//callq
	/* Task switch (set the stack frame of the new task) */
	movq	P_NEXT_TASK_OFFSET(%rbp),%rax
	movq	%rax,P_CUR_TASK_OFFSET(%rbp)
	movq	TASK_RP(%rax),%rsp
	movq	$0,P_NEXT_TASK_OFFSET(%rbp)
	/* ToDo: Load LDT if necessary */
	/* Setup sp0 in TSS */
	movq	P_CUR_TASK_OFFSET(%rbp),%rax
	movq	TASK_SP0(%rax),%rdx
	leaq	P_TSS_OFFSET(%rbp),%rax
	movq	%rdx,TSS_SP0(%rax)
2:
	intr_lapic_isr_done
	iretq
