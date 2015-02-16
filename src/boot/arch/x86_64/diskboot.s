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

	.set	BOOTMON_SEG,0x0900      /* Memory where to load boot monitor */
	.set	BOOTMON_OFF,0x0000      /*  segment and offset [0900:0000] */
	.set	VGA_TEXT_COLOR_80x25,0x03

	.text

	.code16
	.globl	start		/* Entry point */

start:
	/* Save drive information */
	movb	%dl,drive

	cld			/* Clear direction flag */
				/*  (inc di/si for str ops) */

	/* Setup stack */
	cli			/* Disable interrupts */
	xorw	%ax,%ax
	movw	%ax,%ss		/* Stack segment (%ss=0) */
	movw	%ax,%ds		/* Data segment (%ds=0) */
	movw	%ax,%es		/* Data segment (%es=0) */
	sti			/* Disable interrupts */

	/* Set video mode to 16bit color text mode */
	movb	VGA_TEXT_COLOR_80x25,%al
	movb	$0x00,%ah
	int	$0x10

	/* Read a sector from the drive */
	movw	$BOOTMON_SEG,%bx
	movw	%bx,%es		/* Buffer address pointer (Segment) */
	movw	$BOOTMON_OFF,%bx	/* Buffer address pointer (Offset) */
	movb	$0x02,%ah	/* Function: Read sectors from drive */
	movb	$0x01,%al	/* # of sectors to be read */
	movw	$0x0002,%cx	/* Cylinder[7:6]Cylinder[15:8] | Sector[5:0] */
	movb	$0x00,%dh	/* Head */
	movb	drive,%dl	/* Drive */
	int	$0x13
	jc	1f		/* Jump if an error occurs */
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
