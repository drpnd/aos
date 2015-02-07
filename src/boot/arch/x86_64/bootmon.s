/*_
 * Copyright (c) 2015 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
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
	movw	$msg_welcome,%si	/* %ds:(%si) -> welcome message */
	call	putstr

	/* Halt */
	cli
	hlt

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

	.data

msg_welcome:
	.ascii	"Welcome to Academic Operating System!\r\n\n"
	.asciz	"Let's get it started.\r\n\n"

/*
 * Boot monitor (from BIOS)
 *   %cs:%ip=0x0900:0x0000 (=0x9000)
 *   %ss:%sp=0x0000:0x7c00 (=0x7c00)
 *   %dl: drive
 */
