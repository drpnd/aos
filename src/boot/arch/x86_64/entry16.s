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

	.code16
	.globl	entry16

/* Entry point */
entry16:
	cli

	/* Setup temporary IDT and GDT */
	lidt	(idtr)
	lgdt	(gdtr)

	/* Enter the protected mode */
	movl	%cr0,%eax
	orb	$0x1,%al		/* Enable protected mode */
	movl	%eax,%cr0

	/* Go into protected mode and flush the pipeline */
	ljmpl	$0x10,$entry32

	.data

/* Pseudo interrupt descriptor table */
idtr:
	.word	0x0
	.long	0x0

gdt:
	.word	0x0,0x0,0x0,0x0		/* Null descriptor */
	.word	0xffff,0x0,0x9a00,0xaf	/* Code64 descriptor */
	.word	0xffff,0x0,0x9a00,0xcf	/* Code32 descriptor */
	.word	0xffff,0x0,0x9a00,0x8f	/* Code16 descriptor */
	.word	0xffff,0x0,0x9200,0xaf	/* Data64 */
	.word	0xffff,0x0,0x9200,0xcf	/* Data32 */
gdt.1:
/* Pseudo global descriptor table */
gdtr:
	.word	gdt.1 - gdt - 1		/* Limit */
	.long	gdt			/* Address */
