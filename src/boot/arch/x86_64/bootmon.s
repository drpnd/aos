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

	.set	IVT_IRQ0,0x08	/* IRQ0 = 0x08 (BIOS default) */
	.set	IVT_IRQ8,0x70	/* IRQ0 = 0x70 (BIOS default) */
	.set	KBD_LCTRL,0x1d	/* Left ctrl */
	.set	KBD_LSHIFT,0x2a	/* Left shift */
	.set	KBD_RSHIFT,0x36	/* Right shift */
	.set	KBD_CAPS,0x3a	/* Caps lock */
	.set	KBD_RCTRL,0x5a	/* Right ctrl */
	.set	KBD_UP,0x48	/* Up */
	.set	KBD_LEFT,0x4b	/* Left */
	.set	KBD_RIGHT,0x4d	/* Right */
	.set	KBD_DOWN,0x50	/* Down */
	.set	VGA_TEXT_COLOR_80x25,0x03

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

	/* Set video mode to 16bit color text mode */
	movb	VGA_TEXT_COLOR_80x25,%al
	movb	$0x00,%ah
	int	$0x10

	/* Setup the timer interrupt handler */
	xorw	%ax,%ax
	movw	%ax,%es
	movw	$intr_irq0,%ax
	movw	$(IVT_IRQ0+0),%bx
	call	setup_intvec

	/* Setup the keyboard interrupt handler */
	xorw	%ax,%ax
	movw	%ax,%es
	movw	$intr_irq1,%ax
	movw	$(IVT_IRQ0+1),%bx
	call	setup_intvec

	/* Initialize the counter */
	movw	$99*100,(counter)	/* 99 seconds (in centisecond) */

	/* Start the timer */
	call	init_pit

	/* Wait for keyboard interrupts */
1:
	sti
	hlt
	cli
	jmp	1b

/* Initialize programmable interval timer */
init_pit:
	pushw	%ax
	movb	$(0x00|0x30|0x06),%al
	outb	%al,$0x43
	movw	$0x2e9b,%ax	/* Frequency=100Hz: 1193181.67/100 */
	outb	%al,$0x40	/* Counter 0 (least significant 8 bits) */
	movb	%ah,%al		/* Get most significant 8 bits */
	outb	%al,$0x40	/* Counter 0 (most significant 8 bits) */
	popw	%ax
	ret

/*
 * Setup interrupt vector
 *   %es: code segment
 *   %ax: instruction pointer
 *   %bx: interrupt vector number
 */
setup_intvec:
	pushw	%bx
	shlw	$2,%bx
	movw	%ax,(%bx)
	addw	$2,%bx
	movw	%es,(%bx)
	popw	%bx
	ret

/*
 * Timer interrupt handler
 */
intr_irq0:
	/* Save registers to the stack */
	pushw	%ax
	pushw	%bx
	pushw	%cx
	pushw	%dx
	pushw	%ds
	pushw	%si

	movw	(counter),%ax	/* Get the previous counter value */
	testw	%ax,%ax
	jz	1f		/* Jump if the counter reaches zero */
	decw	%ax		/* Decrease the counter by one */
	movw	%ax,(counter)	/* Save the counter */
1:
	movb	$100,%dl	/* Convert centisecond to second */
	divb	%dl		/*  Q=%al, R=%ah */
	xorb	%ah,%ah
	movb	$10,%dl
	divb	%dl		/* Q(%al) = tens digit, R(%ah) = unit digit */
	addb	$'0',%al	/* To ascii */
	addb	$'0',%ah	/* To ascii */
	movw	%ax,%dx
	call	putc
	movb	%dh,%al
	call	putc
	movb	$0x08,%al
	call	putc
	movb	$0x08,%al
	call	putc

	/* EOI for PIC1 */
	movb	$0x20,%al
	outb	%al,$0x20

	/* Restore registers */
	popw	%si
	popw	%dx
	popw	%dx
	popw	%cx
	popw	%bx
	popw	%ax
	iret

/*
 * Keyboard interrupt handler
 */
intr_irq1:
	pushw	%ax
	pushw	%bx
	xorw	%ax,%ax		/* Zero */
	inb	$0x60,%al	/* Scan code from the keyboard controller */
	cmpb	$1,%al		/* If `ESC' is pressed */
	jne	1f
	call	poweroff	/*  then power off */
1:
	movb	%al,%bl		/* Ignore the flag */
	and	$0x7f,%bl	/*  indicating released in %bl */
	cmpb	$KBD_LSHIFT,%bl	/* Left shift */
	je	4f		/* Jump if left shift */
	cmpb	$KBD_RSHIFT,%bl	/* Right shift */
	je	4f		/* Jump if right shift */
	/* Otherwise */
	testb	$0x80,%al	/* Released? */
	jnz	6f		/*  Yes, then ignore the key */
	cmpb	$0,(keyboard_shift)	/* Shift key is released? */
	je	2f		/*  Yes, then use base keymap */
	movw	$keymap_shift,%bx	/*  Otherwise, use shifted keymap */
	jmp	3f
2:
	movw	$keymap_base,%bx	/* Use base keymap */
3:
	addw	%ax,%bx
	movb	(%bx),%al	/* Get ascii code from the keyboard code */
	call	putc		/* Print the character */
	jmp	6f
4:
	testb	$0x80,%al	/* Released? */
	jnz	5f		/*  Yes, then clear shift key */
	movb	$1,(keyboard_shift)	/* Set shift key */
	jmp	6f
5:
	movb	$0,(keyboard_shift)	/* Clear shift key */
6:
	movb	$0x20,%al	/* Notify */
	outb	%al,$0x20	/*  End-Of-Interrupt (EOI) */
	popw	%bx
	popw	%ax
	iret



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

drive:
	.byte	0

/* Counter */
counter:
	.word	0

/* Keymap (US) */
keymap_base:
	.ascii	"  1234567890-=\x08\tqwertyuiop[]\r as"
	.ascii	"dfghjkl;'` \\zxcvbnm,./          "
	.ascii	"                                "
	.ascii	"                                "
keymap_shift:
	.ascii	"  !@#$%^&*()_+\x08\tQWERTYUIOP{}\r AS"
	.ascii	"DFGHJKL:\"~ |ZXCVBNM<>?          "
	.ascii	"                                "
	.ascii	"                                "

/* Keybaord status */
keyboard_shift:
	.byte	0
