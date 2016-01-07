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

	.include	"asmconst.h"
	.file		"pxeboot.s"

	.set	PXENV_SIGNATURE0,0x4e455850	/* PXEN */
	.set	PXENV_SIGNATURE1,0x2b56		/* V+ */
	.set	PXE_SIGNATURE,0x45585021	/* !PXE */
	.set	KERNEL_ADDR,0x10000
	.set	INITRD_ADDR,0x00030000
	.set	MME_SIZE,24		/* Memory map entry size */
	.set	MME_SIGN,0x534d4150	/* MME signature (ascii "SMAP")  */
	.set	GDT_CODE64_SEL,0x08
	.set	GDT_CODE32_SEL,0x10
	.set	GDT_CODE16_SEL,0x18
	.set	GDT_DATA64_SEL,0x20
	.set	GDT_DATA32_SEL,0x28

	.text

	.code16
	.globl	pxeboot

/* Entry point for the PXE boot
 *  %cs:%ip is 0x0000:0x7c000
 *  %es:%bx points to the PXENV+ structure
 *  %ss:%sp points to a valid stack (at least 1.5 KiB available)
 *  %ss:(%sp+4) points to the !PXE structure, if available
 */
pxeboot:
	cld
	cli

	/* Reset %ds */
	xorw	%ax,%ax
	movw	%ax,%ds

	/* Store PXENV+ segment:offset */
	movw	%es,(pxenv_seg)
	movw	%bx,(pxenv_off)

	/* Obtain !PXE structure pointed by %ss:(%sp+4) */
	movw	%ss,%ax
	movw	%ax,%es
	movw	%sp,%bx
	addw	$4,%bx
	movl	%es:(%bx),%eax
	movw	%ax,(pxe_off)
	shrl	$16,%eax
	movw	%ax,(pxe_seg)

	sti

verify_pxenv:
	/* Verify the PXENV+ structure */
	movw	(pxenv_seg),%ax
	movw	%ax,%es
	movw	(pxenv_off),%bp
	movl	%es:(%bp),%eax
	cmpl	$PXENV_SIGNATURE0,%eax
	jne	error
	movw	%es:4(%bp),%ax
	cmpw	$PXENV_SIGNATURE1,%ax
	jne	error
	/* Check the signature */
	movw	%bp,%si
	xorw	%cx,%cx
	movb	%es:8(%bp),%cl
	call	checksum
	testb	%al,%al
	jnz	error
	/* Check the version.  This only supports >=2.1. */
	movw	%es:6(%bp),%ax
	cmpw	$0x21,%ax
	jl	error

parse_pxe:
	/* Parse !PXE structure */
	movw	(pxe_seg),%ax
	movw	%ax,%es
	movw	(pxe_off),%bx

	/* !PXE signature */
	movl	%es:(%bx),%eax
	cmpl	$PXE_SIGNATURE,%eax
	jne	error			/* Error on invalid signature */

	/* Obtain PMEntry from the !PXE structure */
	movl	%es:16(%bx),%eax
	movl	%eax,pm_entry
	movw	%ax,pm_entry_off
	shrl	$16,%eax
	movw	%ax,pm_entry_seg

	/* Get the cache information (buffer information) */
	movw	$t_PXENV_GET_CACHED,%di
	movw	$0x0071,%bx		/* PXENV_GET_CACHED_INFO opcode */
	call	pxeapi
	movw	t_PXENV_GET_CACHED.Status,%ax
	testw	%ax,%ax
	jnz	error

/* Load kernel and initrd */
	movw	$kernel,%si
	movl	$KERNEL_ADDR,%edi
	call	load_tftp_file
	testw	%ax,%ax
	jnz	error

	movw	$initrd,%si
	movl	$INITRD_ADDR,%edi
	call	load_tftp_file
	testw	%ax,%ax
	jnz	error

bootmon:
	/* Reset the boot information structure */
	xorl	%eax,%eax
	movl	$BOOTINFO_BASE,%ebx
	movl	$BOOTINFO_SIZE,%ecx
	shrl	$2,%ecx
1:
	movl	%eax,(%ebx)
	addl	$4,%ebx
	loop	1b

	xorl	%eax,%eax
	movl	%eax,(BOOTINFO_MM_NUM)
	movl	%eax,(BOOTINFO_MM_NUM+4)
	movl	%eax,(BOOTINFO_MM_PTR)
	movl	%eax,(BOOTINFO_MM_PTR+4)
	/* Load memory map entries */
	movw	%ax,%es
	movw	$BOOTINFO_MM_TBL,%ax
	movw	%ax,(BOOTINFO_MM_PTR)
	movw	%ax,%di
	call	load_mm		/* Load system address map to %es:%di */
	movw	%ax,(BOOTINFO_MM_NUM)
	jmp	boot16

/* Load memory map entries from BIOS
 *   Input:
 *     %es:%di: Destination
 *   Return:
 *     %ax: The number of entries
 *     CF: set if an error occurs
 */
load_mm:
	pushl	%ebx
	pushl	%ecx
	pushw	%di
	pushw	%bp
	xorl	%ebx,%ebx		/* Continuation value for int 0x15 */
	xorw	%bp,%bp			/* Counter */
load_mm.1:
	movl	$0x1,%ecx		/* Write 1 once */
	movl	%ecx,%es:20(%di)	/*  to check support ACPI >= 3.x? */
	/* Read the system address map */
	movl	$0xe820,%eax
	movl	$MME_SIGN,%edx		/* Set the signature */
	movl	$MME_SIZE,%ecx		/* Set the buffer size */
	int	$0x15			/* Query system address map */
	jc	load_mm.error		/* Could not load the address map */
	cmpl	$MME_SIGN,%eax		/* Check the signature SMAP */
	jne	load_mm.error

	cmpl	$24,%ecx		/* Check the read buffer size */
	je	load_mm.2		/*  %ecx == 24 */
	cmpl	$20,%ecx
	je	load_mm.3		/*  %ecx == 20 */
	jmp	load_mm.error		/*  otherwise raise an error */
load_mm.2:
	/* 24-byte entry */
	testl	$0x1,%es:20(%di)	/* 1 must be present in the attribute */
	jz	load_mm.error		/*  error if it's overwritten */
load_mm.3:
	/* 20-byte entry or 24-byte entry coming from above */
	incw	%bp			/* Increment the number of entries  */
	testl	%ebx,%ebx		/* %ebx=0: No remaining info */
	jz	load_mm.done
load_mm.4:
	addw	$MME_SIZE,%di		/* Next entry */
	jmp	load_mm.1		/* Load remaining entries */
load_mm.error:
	stc				/* Set CF */
load_mm.done:
	movw	%bp,%ax			/* Return value */
	popw	%bp
	popw	%di
	popl	%ecx
	popl	%ebx
	ret

boot16:
	cli
	/* Setup temporary IDT and GDT */
	lidt	(idtr)
	lgdt	(gdtr)

	/* Enable protected mode */
	movl	%cr0,%eax
	orb	$0x1,%al
	movl	%eax,%cr0

	/* Go into protected mode and flush the pipeline */
	ljmpl	$GDT_CODE32_SEL,$boot32


/* Call PXE API specified by the opcode %bx with the input buffer pointed by
 * %ds:%di.  Note that the arguments of this function should not be changed, so
 * that we do not need to add some lines of code for the legacy PXE API because
 * the legacy PXE API calling convension uses the registers of %ds:%di and %bx
 * as its arguments instead of the stack pushed in this code.
 */
pxeapi:
	push	%ds
	push	%di
	push	%bx
	lcall	*(pm_entry)
	addw	$6,%sp
	ret

/* Load the content of a file specified by a null-terminated string starting
 * from (%si) to (%edi). */
load_tftp_file:
	pushw	%bx
	pushw	%cx
	pushl	%edx
	pushw	%fs
	pushw	%si
	pushl	%edi

	/* Reset the file name first */
	movw	$t_PXENV_TFTP_OPEN.Filename,%di
	movw	$128,%cx
	xorb	%al,%al
	rep	stosb

	/* Get the cached info to build a bootph structure */
	movw	t_PXENV_GET_CACHED.BufferSize,%cx
	movw	t_PXENV_GET_CACHED.BufferOff,%bx
	movw	t_PXENV_GET_CACHED.BufferSeg,%dx

	/* Open a file */
	movw	%dx,%fs
	movl	%fs:20(%bx),%eax
	movl	%eax,t_PXENV_TFTP_OPEN.SIP	/* Server IP address */
	movl	%fs:24(%bx),%eax
	movl	%eax,t_PXENV_TFTP_OPEN.GIP	/* Relay agent IP address */
	movw	$69,%ax
	xchgb	%al,%ah
	movw	%ax,t_PXENV_TFTP_OPEN.Port	/* TFTP port (69) */
	movw	$512,%ax
	movw	%ax,t_PXENV_TFTP_OPEN.PacketSize
	/* Copy the null-terminated string */
	movw	$t_PXENV_TFTP_OPEN.Filename,%di
1:
	movb	(%si),%al
	testb	%al,%al
	jz	2f
	movb	%al,(%di)
	incw	%si
	incw	%di
	jmp	1b
2:

	/* Open the TFTP session */
	movw	$t_PXENV_TFTP_OPEN,%di
	movw	$0x0020,%bx			/* PXENV_TFTP_OPEN opcode */
	call	pxeapi
	movw	t_PXENV_TFTP_OPEN.Status,%ax
	testw	%ax,%ax
	jnz	load_tftp_file.error

/* Read the file specified by the PXENV_TFTP_READ structure */
	movw	$t_PXENV_TFTP_READ,%di
	movw	$0x0022,%bx
	movl	%ss:0(%esp),%edx
	xorw	%cx,%cx
1:
	movw	%dx,%ax
	andw	$0xf,%ax
	movw	%ax,t_PXENV_TFTP_READ.BufferOff
	movl	%edx,%eax
	shrl	$4,%eax
	movw	%ax,t_PXENV_TFTP_READ.BufferSeg
	call	pxeapi
	movw	t_PXENV_TFTP_READ.Status,%ax
	testw	%ax,%ax
	jnz	load_tftp_file.error
	/* Check the packet number */
	incw	%cx
	movw	$0xffff,%ax			/* to raise an error */
	cmpw	t_PXENV_TFTP_READ.PacketNumber,%cx
	jne	load_tftp_file.error
	/* Check the size */
	xorl	%eax,%eax
	movw	t_PXENV_TFTP_READ.BufferSize,%ax
	addl	%eax,%edx
	cmpl	$0x100000,%edx
	jge	load_tftp_file.error
	/* Check if it is the last packet */
	cmpw	t_PXENV_TFTP_OPEN.PacketSize,%ax
	jge	1b

	/* Close */
	movw	$t_PXENV_TFTP_CLOSE,%di
	movw	$0x0021,%bx
	call	pxeapi
	movw	t_PXENV_TFTP_CLOSE.Status,%ax
	testw	%ax,%ax
	jnz	load_tftp_file.error

load_tftp_file.success:
	xorw	%ax,%ax
load_tftp_file.error:
	popl	%edi
	popw	%si
	popw	%fs
	popl	%edx
	popw	%cx
	popw	%bx

	ret



/* Calculate the checksum of the input byte-string starting from %es:%si for the
 * length of %cx bytes.  The checksum is returned through %al. */
checksum:
	push	%cx
	push	%si
	xorb	%al,%al
1:
	addb	%es:(%si),%al
	incw	%si
	loop	1b
	pop	%si
	pop	%cx
	ret

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


/* Error handler */
error:
	hlt
	jmp	error


/* Entry point for 32-bit protected mode */
	.align	16
	.code32
boot32:
	cli

	/* Mask all interrupts (i8259) */
	movb	$0xff,%al
	outb	%al,$0x21
	movb	$0xff,%al
	outb	%al,$0xa1

	/* Initialize stack and data segments.  Note that %cs is */
	/* automatically set after the long jump operation. */
	movl	$GDT_DATA32_SEL,%eax
	movl	%eax,%ss
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%fs
	movl	%eax,%gs

	/* Obtain APIC ID (assuming it is 0) */
	movl	$0xfee00000,%edx
	movl	0x20(%edx),%eax
	shrl	$24,%eax

	/* Setup stack with 16-byte guard */
	addl	$1,%eax
	movl	$P_DATA_SIZE,%ebx
	mull	%ebx			/* [%edx|eax] = %eax * P_DATA_SIZE */
	addl	$P_DATA_BASE,%eax
	subl	$P_STACK_GUARD,%eax
	movl	%eax,%esp

	/* Enable PAE */
	movl	$0x220,%eax		/* CR4[bit 5] = PAE */
	movl	%eax,%cr4		/* CR4[bit 9] = OSFXSR */

/* Create 64-bit page table */
pg_setup:
	movl	$KERNEL_PGT,%ebx	/* LS 12 bits must be 0. */
	movl	%ebx,%edi
	xorl	%eax,%eax
	movl	$(512*8*6/4),%ecx
	rep	stosl			/* Initialize %ecx*4 bytes from %edi */
					/*  with %eax (zero) */
	/* Level 4 page map */
	leal	0x1007(%ebx),%eax
	movl	%eax,(%ebx)
	/* Page directory pointers (PDPE) */
	leal	0x1000(%ebx),%edi
	leal	0x2007(%ebx),%eax
	movl	$4,%ecx
pg_setup.1:
	movl	%eax,(%edi)
	addl	$8,%edi
	addl	$0x1000,%eax
	loop	pg_setup.1
	/* Page directories (PDE) */
	leal	0x2000(%ebx),%edi
	movl	$0x183,%eax
	movl	$(512*4),%ecx
pg_setup.2:
	movl	%eax,(%edi)
	addl	$8,%edi
	addl	$0x00200000,%eax
	loop	pg_setup.2

	/* Setup page table regsiter */
	movl	%ebx,%cr3

	/* Enable long mode */
	movl	$0xc0000080,%ecx	/* EFER MSR number */
	rdmsr				/* Read from 64-bit-specific register */
	btsl	$8,%eax			/* LME bit = 1 */
	wrmsr

	/* Activate page translation and long mode */
	movl	$0x80000001,%eax
	movl	%eax,%cr0

	/* Load code64 descriptor */
	pushl	$GDT_CODE64_SEL
	pushl	$boot64
	lret

/* Entry point for 64-bit long mode code */
	.align	16
	.code64
boot64:
	cli
	xorl	%eax,%eax
	movl	%eax,%fs
	movl	%eax,%gs
	movl	%eax,%ds
	movl	%eax,%es
	movl	$GDT_DATA64_SEL,%eax
	movl	%eax,%ss

	/* Clear screen */
	movl	$0xb8000,%edi
	movw	$0x0f20,%ax
	movl	$80*25,%ecx
	rep	stosw

	/* Reset cursor */
	movw	$0x000f,%ax	/* %al=0xf: cursor location low, %ah: xx00 */
	movw	$0x3d4,%dx
	outw	%ax,%dx
	movw	$0x000e,%ax	/* %al=0xf: cursor location high, %ah: 00xx */
	movw	$0x3d4,%dx
	outw	%ax,%dx

	/* Jump to the kernel main function */
	movq	$KERNEL_ADDR,%rdi
	call	ljmp64
	jmp	halt64

	/* Halt (64-bit mode) */
halt64:
	hlt
	jmp	halt64

ljmp64:
	/* Load code64 descriptor */
	pushq	$GDT_CODE64_SEL
	pushq	%rdi
	lretq


/* Data section */
	.align	16
	.data

/* Pseudo interrupt descriptor table */
idtr:
	.word	0x0
	.long	0x0

/* Global descriptor table */
gdt:
	.word	0x0,0x0,0x0,0x0		/* Null entry */
	.word	0xffff,0x0,0x9a00,0xaf	/* Code64 */
	.word	0xffff,0x0,0x9a00,0xcf	/* Code32 */
	.word	0xffff,0x0,0x9a00,0x8f	/* Code16 */
	.word	0xffff,0x0,0x9200,0xaf	/* Data64 */
	.word	0xffff,0x0,0x9200,0xcf	/* Data32 */
gdt.1:
	/* Global descriptor table register */
gdtr:
	.word	gdt.1 - gdt - 1		/* Limit */
	.long	gdt			/* Address */

/* Second stage filename */
kernel:
	.asciz	"kernel"
initrd:
	.asciz	"initrd"

/* Stored PXE information */
pxenv_seg:
	.word	0x0
pxenv_off:
	.word	0x0
pxe_seg:
	.word	0x0
pxe_off:
	.word	0x0

pm_entry:
	.long	0x0
pm_entry_seg:
	.word	0x0
pm_entry_off:
	.word	0x0

t_PXENV_GET_CACHED:
t_PXENV_GET_CACHED.Status:
	.word	0
t_PXENV_GET_CACHED.PacketType:
	.word	2
t_PXENV_GET_CACHED.BufferSize:
	.word	0
t_PXENV_GET_CACHED.BufferOff:
	.word	0
t_PXENV_GET_CACHED.BufferSeg:
	.word	0
t_PXENV_GET_CACHED.BufferLimit:
	.word	0

t_PXENV_TFTP_OPEN:
t_PXENV_TFTP_OPEN.Status:
	.word	0
t_PXENV_TFTP_OPEN.SIP:
	.long	0
t_PXENV_TFTP_OPEN.GIP:
	.long	0
t_PXENV_TFTP_OPEN.Filename:
	.rept 128
	.byte	0
	.endr
t_PXENV_TFTP_OPEN.Port:
	.word	0
t_PXENV_TFTP_OPEN.PacketSize:
	.word	0

t_PXENV_TFTP_READ:
t_PXENV_TFTP_READ.Status:
	.word	0
t_PXENV_TFTP_READ.PacketNumber:
	.word	0
t_PXENV_TFTP_READ.BufferSize:
	.word	0
t_PXENV_TFTP_READ.BufferOff:
	.word	0
t_PXENV_TFTP_READ.BufferSeg:
	.word	0

t_PXENV_TFTP_CLOSE:
t_PXENV_TFTP_CLOSE.Status:
	.word	0
