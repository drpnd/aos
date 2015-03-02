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

/* Entry point to the 64-bit kernel */
kstart64:
	movq	$msg,%rsi
	call	putbstr
1:
	sti
	hlt
	cli
	jmp	1b

/*
 * Display a null-terminated string at the bottom-line
  */
putbstr:
	/* Save registers */
	pushq	%rax
	pushq	%rdi
	movq	$0xb8000,%rdi	/* Memory 0xb8000 */
	addq	$(80*24*2),%rdi	/* 24th (zero-numbering) line */
putbstr.load:
	lodsb			/* Load %al from (%rsi) , then incl %rsi */
	testb	%al,%al		/* Stop at null */
	jnz	putbstr.putc	/* Call the function to output %al */
	/* Restore registers */
	popq	%rdi
	popq	%ax
	ret
putbstr.putc:
	movb	$0x7,%ah
	stosw			/* Write %rax to (%rdi), then add 2 to %di */
	jmp     putbstr.load

	.data

msg:
	.asciz	"KERNEL loaded..."
