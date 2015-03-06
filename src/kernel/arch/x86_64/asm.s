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
	.globl	kstart64
	.globl	_halt
	.globl	_lgdt
	.globl	_lidt
	.globl	_intr_null

	.set	APIC_BASE,0xfee00000
	.set	APIC_EOI,0x0b0

/* Entry point to the 64-bit kernel */
kstart64:
	call	_kmain

/* Halt */
_halt:
	sti
	hlt
	cli
	ret

/* void lgdt(void *gdtr, u64 selector) */
_lgdt:
	lgdt	(%rdi)
	/* Reload GDT */
	pushq	%rsi
	pushq	$1f	/* Just to do ret */
	lretq
1:
	ret

/* void lidt(void *idtr) */
_lidt:
	lidt	(%rdi)
	ret

/* Null function for interrupt handler */
_intr_null:
	pushq	%rdx
	/* APIC EOI */
	movq	$APIC_BASE,%rdx
	movq	$0,APIC_EOI(%rdx)
	popq	%rdx
	iretq
