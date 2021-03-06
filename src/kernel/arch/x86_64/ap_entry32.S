/*_
 * Copyright (c) 2015-2016 Hirochika Asai <asai@jar.jp>
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
	.globl	ap_entry32

/* Entry point */
ap_entry32:
	cli

	/* %cs is automatically set after the long jump operation */
	/* Setup other segment registers */
	movl	$AP_GDT_DATA64_SEL,%eax
	movl	%eax,%ss
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%fs
	movl	%eax,%gs

	/* Obtain APIC ID */
	movl	$APIC_BASE,%edx
	movl	0x20(%edx),%eax
	shrl	$24,%eax

	/* Setup stack with 16 byte guard */
	addl	$1,%eax
	movl	$CPU_DATA_SIZE,%ebx
	mull	%ebx		/* [%edx:%eax] = %eax * CPU_DATA_SIZE */
	addl	$CPU_DATA_BASE,%eax
	subl	$CPU_STACK_GUARD,%eax
	movl	%eax,%esp

	/* Enable PAE */
	movl	$((1 << CR4_PAE) | (1 << CR4_OSFXSR)),%eax
	movl	%eax,%cr4

	/* Setup page table register */
	movl	$KERNEL_PGT,%ebx
	movl	%ebx,%cr3

	/* Enable long mode */
	movl	$MSR_IA32_EFER,%ecx	/* EFER MSR number */
	rdmsr			/* Read from 64bit-specific register */
	btsl	$IA32_EFER_LME,%eax	/* LME bit = 1 */
	wrmsr			/* Write to 64bit-specific register */

	/* Activate page translation and long mode */
	movl	$((1 << CR0_PG) | (1 << CR0_PE)),%eax
	movl	%eax,%cr0

	/* Load code64 descriptor */
	pushl	$AP_GDT_CODE64_SEL
	pushl	$ap_entry64
	lret
