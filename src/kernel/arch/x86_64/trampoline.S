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

/* Note that .data section cannot be used in the trampoline file because we
   need to determine the contiguous space of the trampoline code within a
   section from two labels; between _trampoline and _trampoline_end. */
	.text

	.globl	_trampoline
	.globl	_trampoline_end

	.code16

/* Trampoline code starts.  Note that the trampoline code is loaded into a
 * 4 KiB (aligned) page in the lower 1 MiB of memory.  The %cs is automatically
 * set after the SIPI.  %ip is expected to be zero but not sure.  So, we first
 * calculate the offsets of idtr and gdtr.
 */
_trampoline:
	cli

	/* Calculate the base address */
	xorl	%eax,%eax
	movw	$(TRAMPOLINE_VEC << 8),%ax
	movw	%ax,%ds

	/* Setup GDT and IDT */
	lidt	%ds:(idtr - _trampoline)	/* N.B., %ds: can be omitted */
	lgdt	%ds:(gdtr - _trampoline)	/* N.B., %ds: can be omitted */

	/* Turn on protected mode */
	movl	%cr0,%eax
	orl	$0x1,%eax	/* Enable protected mode */
	movl	%eax,%cr0
	ljmpl	$AP_GDT_CODE32_SEL,$(ap_entry32)

	/*.data: Trampoline code cannot have separate sections */
	.align	16

/* Pseudo interrupt descriptor table */
idtr:
	.word	0x0		/* Limit */
	.long	0x0		/* Base address */

/* Pseudo global descriptor table */
gdt:
	.word	0x0,0x0,0x0,0x0	/* Null entry */
	.word	0xffff,0x0,0x9a00,0xaf	/* Code64 */
	.word	0xffff,0x0,0x9a00,0xcf	/* Code32 */
	.word	0xffff,0x0,0x9a00,0x8f	/* Code16 */
	.word	0xffff,0x0,0x9200,0xaf	/* Data64 */
	.word	0xffff,0x0,0x9200,0xcf	/* Data32 */
gdt.1:
gdtr:
	.word	gdt.1 - gdt - 1	/* Limit */
	.long	gdt		/* Base address */

_trampoline_end:
