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

	.code32
	.globl	entry32

/* Entry point */
entry32:
	cli

	/* Mask all interrupts (i8259) */
	movb	$0xff,%al
	outb	%al,$0x21
	movb	$0xff,%al
	outb	%al,$0xa1

	/* %cs is automatically set after the long jump operation */
	movl	$0x20,%eax
	movl	%eax,%ss
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%fs
	movl	%eax,%gs

	/* Obtain APIC ID */
	movl	$0xfee00000,%edx
	movl	0x20(%edx),%eax		/* APIC ID */
	shrl	$24,%eax
	/* P6 family and Pentium processors: [27:24] */
	/* Pentium 4 processors, Xeon processors, and later processors: [31:24] */

	/* Setup stack with 16 byte guard */
	addl	$1,%eax
	movl	$P_DATA_SIZE,%ebx
	mull	%ebx			/* [%edx|%eax] = %eax * %ebx */
	addl	$P_DATA_BASE,%eax
	subl	$P_STACK_GUARD,%eax
	movl	%eax,%esp

	/* Enable PAE and SSE */
	movl	$0x00000220,%eax	/* CR4[bit 5] = PAE */
	movl	%eax,%cr4		/* CR4[bit 9] = OSFXSR */

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
pg_setup.1a:
	movl	%eax,(%edi)
	addl	$8,%edi
	addl	$0x1000,%eax
	loop	pg_setup.1a
	movl	$508,%ecx
	/* PDPE: 4G-512G */
	movl	$0x183,%eax
pg_setup.1b:
	movl	%eax,(%edi)
	addl	$8,%edi
	loop	pg_setup.1b
	/* Page directories (PDE) */
	leal	0x2000(%ebx),%edi
	movl	$0x183,%eax
	movl	$(512*4),%ecx
pg_setup.2:
	movl	%eax,(%edi)
	addl	$8,%edi
	addl	$0x00200000,%eax
	loop	pg_setup.2

	/* Set page table register */
	movl	%ebx,%cr3

	/* Enable long mode */
	movl	$0xc0000080,%ecx	/* EFER MSR number */
	rdmsr				/* Read from 64 bit-specific register */
	btsl	$8,%eax			/* LME bit = 1 */
	wrmsr

	/* Activate page translation and long mode */
	movl	$0x80000001,%eax
	movl	%eax,%cr0

	/* Load code64 descriptor */
	pushl	$0x08
	pushl	$entry64
	lret
