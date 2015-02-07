/*_
 * Copyright (c) 2015 Hirochika Asai
 * All rights reserved.
 *
 * Authors:
 *      Hirochika Asai  <asai@jar.jp>
 */

	.set	BOOTMON_SEG,0x0900      /* Memory where to load boot monitor */
	.set	BOOTMON_OFF,0x0000      /*  segment and offset [0900:0000] */

	.text

	.code16
	.globl	start		/* Entry point */

start:
	/* Save drive information */
	movb	%dl,drive

	/* Read a sector from the drive */
	movw	$BOOTMON_SEG,%bx
	movw	%bx,%es		/* Buffer address pointer (Segment) */
	movw	$BOOTMON_OFF,%bx	/* Buffer address pointer (Offset) */
	movb	$0x02,%ah	/* Function: Read sectors from drive */
	movb	$0x01,%al	/* # of sectors to be read */
	movw	$0x0002,%cx	/* Cylinder[6:15] | Sector[0:5] */
	movb	$0x00,%dh	/* Head */
	movb	drive,%dl	/* Drive */
	int	$0x13

	/* Check the status code */
	testb	%ah,%ah
	jnz	1f
	/* Jump to boot monitro*/
	ljmp	$BOOTMON_SEG,$BOOTMON_OFF

1:
	/* Error */
	movw	$msg_error,%si	/* %ds:(%si) -> error message */
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

msg_error:
	.asciz	"Disk read error!\r\n"

drive:
	.byte	0
