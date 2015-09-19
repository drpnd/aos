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

/* Entry point */
entry64:
	cli

	movl	$0x20,%eax
	movl	%eax,%ss
	movl	%eax,%ds
	xorl	%eax,%eax
	movl	%eax,%es
	movl	%eax,%fs
	movl	%eax,%gs

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

	sti

	/* Get into the C code */
	call	_centry		/* Won't return */

1:
	hlt
	jmp	1b

/* void ljmp(u64 selector, u64 address); */
_ljmp:
	pushq	%rdi
	pushq	%rsi
	lretq

/* void hlt(void); */
_hlt:
	hlt
	ret
