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

#include "const.h"

	.text

	.code32
	.globl	entry32

/* Entry point */
entry32:
	cli

	/* Mask all interrupts (i8259) */
	movb	$0xff,%al
	outb	%al,$IO_PIC1_DATA
	movb	$0xff,%al
	outb	%al,$IO_PIC2_DATA

	/* %cs is automatically set after the long jump operation */
	movl	$GDT_DATA32,%eax
	movl	%eax,%ss
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%fs
	movl	%eax,%gs

	/* Obtain APIC ID */
	movl	$MSR_APIC_BASE,%ecx
	rdmsr
	andl	$APIC_BASE_MASK,%eax	/* APIC Base */
	movl	LAPIC_ID_OFF(%eax),%eax
	shrl	$LAPIC_ID_SHR,%eax
	/* P6 family and Pentium processors: [27:24] */
	/* Pentium 4 processors, Xeon processors, and later processors: [31:24] */

	/* Setup stack with 16 byte guard */
	addl	$1,%eax
	movl	$CPU_DATA_SIZE,%ebx
	mull	%ebx			/* [%edx|%eax] = %eax * %ebx */
	addl	$CPU_DATA_BASE,%eax
	subl	$CPU_STACK_GUARD,%eax
	movl	%eax,%esp

	/* CPU feature check */
	pushfl
	popl	%eax
	btl	$EFLAGS_CPUID,%eax	/* CPUID supported? */
	jz	1f
	movl	$1,%eax
	cpuid
	btl	$CPUID1H_EDX_SSE,%edx	/* SSE supported? */
	jz	1f
	btl	$CPUID1H_EDX_SSE2,%edx	/* SSE2 supported? */
	jz	1f
	btl	$CPUID1H_EDX_PAE,%edx	/* PAE supported? */
	jz	1f

	/* Valid CPU */
	jmp	3f
1:
	/* Invalid CPU */
	movl	$error_msg,%esi
	call	display_error
2:
	hlt
	jmp	2b

3:
	/* Enable PAE and SSE */
	movl	$((1 << CR4_PAE) | (1 << CR4_OSFXSR)),%eax
	movl	%eax,%cr4
/* Create 64bit page table */
pg_setup:
	movl	$KERNEL_PGT,%ebx	/* Low 12 bit must be zero */
	movl	%ebx,%edi
	xorl	%eax,%eax
	movl	$(512*8*6/4),%ecx
	rep	stosl			/* Initialize %ecx*4 bytes from %edi */
					/*  with %eax */
	/* Level 4 page map */
	leal	0x1007(%ebx),%eax
	movl	%eax,(%ebx)
	/* Page directory pointers (PDPE) */
	leal	0x1000(%ebx),%edi
	leal	0x2007(%ebx),%eax
	movl	$4,%ecx
pg_setup.1:
	movl	%eax,(%edi)
	addl	$8,%edi
	addl	$0x1000,%eax
	loop	pg_setup.1
	/* Page directories (PDE) */
	leal	0x2000(%ebx),%edi
	movl	$0x083,%eax
	movl	$(512*4),%ecx
pg_setup.2:
	movl	%eax,(%edi)
	addl	$8,%edi
	addl	$0x00200000,%eax
	loop	pg_setup.2

	/* Set page table register */
	movl	%ebx,%cr3

	/* Enable long mode */
	movl	$MSR_IA32_EFER,%ecx	/* EFER MSR number */
	rdmsr				/* Read from 64 bit-specific register */
	btsl	$IA32_EFER_LME,%eax	/* LME bit = 1 */
	wrmsr

	/* Activate page translation and long mode */
	movl	%cr0,%eax
	orl	$((1 << CR0_PG) | (1 << CR0_PE)),%eax
	movl	%eax,%cr0

	/* Load code64 descriptor */
	pushl	$GDT_CODE64
	pushl	$entry64
	lret

/* Display error message */
display_error:
	/* Clear screen */
	movl	$0xb8000,%edi
	movw	$0x2f20,%ax
	movl	$80*25,%ecx
	rep	stosw

	movl	$0xb8000,%edi
	movb	$0x2f,%ah
	xorl	%ecx,%ecx
1:
	movb	(%esi),%al
	testb	%al,%al
	jz	2f
	movw	%ax,(%edi)
	incl	%esi
	addl	$2,%edi
	incl	%ecx
	jmp	1b
2:
	movw	%cx,%ax
	andw	$0xff,%ax
	shlw	$8,%ax
	orw	$0xf,%ax
	movw	$0x3d4,%dx
	outw	%ax,%dx
	movw	%cx,%ax
	andw	$0xff00,%ax
	orw	$0xe,%ax
	movw	$0x3d4,%dx
	outw	%ax,%dx
	ret

	.align	16
	.data
error_msg:
	.asciz	"Unsupported CPU"
