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
	.globl	bootmon		/* Entry point */

/*
 * Boot monitor (from BIOS)
 *   %cs:%ip=0x0900:0x0000 (=0x9000)
 *   %dl: drive
 */
bootmon:
	/* Save parameters from IPL */
	movb	%dl,drive

	call	poweroff

	/* Halt */
	cli
	hlt


/* Power off the machine using APM */
poweroff:
	/* Disable PIC */
	call	disable_pic

	/* Power off with APM */
	movw	$0x5301,%ax	/* Connect APM interface */
	movw	$0x0,%bx	/* Specify system BIOS */
	int	$0x15		/* Return error code in %ah with CF */
	jc	1f		/* Error */

	movw	$0x530e,%ax	/* Set APM version */
	movw	$0x0,%bx	/* Specify system BIOS */
	movw	$0x102,%cx	/* Version 1.2 */
	int	$0x15		/* Return error code in %ah with CF */
	jc	1f		/* Error */

	movw	$0x5308,%ax	/* Enable power management */
	movw	$0x1,%bx	/* All devices managed by the system BIOS */
	movw	$0x1,%cx	/* Enable */
	int	$0x15		/* Ignore errors */

	movw	$0x5307,%ax	/* Set power state */
	movw	$0x1,%bx	/* All devices managed by the system BIOS */
	movw	$0x3,%cx	/* Off */
	int	$0x15
1:
	ret			/* Return on error */


/* Disable i8259 PIC */
disable_pic:
	pushw	%ax
	movb	$0xff,%al
	outb	%al,$0xa1
	movb	$0xff,%al
	outb	%al,$0x21
	popw	%ax
	ret


/* Display the error message (%ah = error code) */
int_error:
	movb	%ah,%al
	movw	$error_code,%di
	xorw	%bx,%bx
	movw	%bx,%es
	call	hex8
	movw	$msg_error,%si	/* %ds:(%si) -> error message */
	call	putstr		/* Display error message at %si and then halt */

/* Halt */
halt:
	hlt
	jmp	halt

/* Display a null-terminated string */
putstr:
putstr.load:
	lodsb			/* Load %al from %ds:(%si), then incl %si */
	testb	%al,%al		/* Stop at null */
	jnz	putstr.putc	/* Call the function to output %al */
	ret			/* Return if null is reached */
putstr.putc:
	call	putc		/* Output a character %al */
	jmp	putstr		/* Go to next character */
putc:
	pushw	%bx		/* Save %bx */
	movw	$0x7,%bx	/* %bh: Page number for text mode */
				/* %bl: Color code for graphics mode */
	movb	$0xe,%ah	/* BIOS: Put char in tty mode */
	int	$0x10		/* Call BIOS, print a character in %al */
	popw	%bx		/* Restore %bx */
	ret

/* Convert %al to hex characters, saving the result to [%di] */
hex8:
	pushw	%ax		/* Save %ax */
	shrb	$0x4,%al	/* Get most significant 4 bits in %al */
	call	hex8.allsb4	/* Convert the least significant 4 bits in */
				/*  %al to a hex character */
	popw	%ax		/* Restore %ax */
hex8.allsb4:
	andb	$0xf,%al	/* Get least significant 4 bits in %al */
	cmpb	$0xa,%al	/* CF=1 if %al < $0xa (0..9) */
	sbbb	$0x69,%al	/* %al <= %al - ($0x69 + CF) */
	das			/* BCD (N.B., %al - 0x60 if AF is not set) */
	orb	$0x20,%al	/* To lower case */
	stosb			/* Save char to %es:[%di] and inc %di */
	ret


	.data

msg_welcome:
	.ascii	"Welcome to Academic Operating System!\r\n\n"
	.asciz	"Let's get it started.\r\n\n"

msg_error:
	.ascii  "Error: 0x"
error_code:
        .asciz  "00\r\r"

drive:
	.byte	0
