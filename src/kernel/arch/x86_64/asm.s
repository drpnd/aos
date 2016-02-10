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
	.text

	.code64
	.globl	kstart64
	.globl	apstart64
	.globl	_halt
	.globl	_pause
	.globl	_lgdt
	.globl	_sgdt
	.globl	_lidt
	.globl	_sidt
	.globl	_lldt
	.globl	_ltr
	.globl	_sti
	.globl	_cli
	.globl	_rdtsc
	.globl	_inb
	.globl	_inw
	.globl	_inl
	.globl	_outb
	.globl	_outw
	.globl	_outl
	.globl	_mfread32
	.globl	_mfwrite32
	.globl	_cpuid
	.globl	_rdmsr
	.globl	_wrmsr
	.globl	_kmemset
	.globl	_kmemcmp
	.globl	_kmemcpy
	.globl	_bitwidth
	.globl	_spin_lock_intr
	.globl	_spin_unlock_intr
	.globl	_spin_lock
	.globl	_spin_unlock
	.globl	_syscall_setup
	.globl	_asm_ioapic_map_intr
	.globl	_get_cr0
	.globl	_set_cr0
	.globl	_get_cr3
	//.globl	_set_cr3
	.globl	_get_cr4
	.globl	_set_cr4
	.globl	_invlpg
	.globl	_vmxon
	.globl	_vmclear
	.globl	_vmptrld
	.globl	_vmwrite
	.globl	_vmread
	.globl	_vmlaunch
	.globl	_vmresume
	.globl	_task_restart
	.globl	_task_replace
	.globl	_intr_null
	.globl	_intr_dze
	.globl	_intr_debug
	.globl	_intr_nmi
	.globl	_intr_breakpoint
	.globl	_intr_overflow
	.globl	_intr_bre
	.globl	_intr_iof
	.globl	_intr_dna
	.globl	_intr_df
	.globl	_intr_itf
	.globl	_intr_snpf
	.globl	_intr_ssf
	.globl	_intr_gpf
	.globl	_intr_pf
	.globl	_intr_x87_fpe
	.globl	_intr_simd_fpe
	.globl	_intr_apic_loc_tmr
	.globl	_intr_crash
	.globl	_sys_fork

	.set	APIC_LAPIC_ID,0x020
	.set	APIC_EOI,0x0b0
	.set	MSR_APIC_BASE,0x1b

/* Entry point to the 64-bit kernel */
kstart64:
	/* Disable interrupts */
	cli

	/* Re-configure the stack pointer (for alignment) */
	/* Obtain APIC ID */
	movq	$MSR_APIC_BASE,%rcx
	rdmsr
	shlq	$32,%rdx
	addq	%rax,%rdx
	andq	$0xfffffffffffff000,%rdx	/* APIC Base */
	xorq	%rax,%rax
	movl	APIC_LAPIC_ID(%rdx),%eax
	shrl	$24,%eax
	/* P6 family and Pentium processors: [27:24] */
	/* Pentium 4 processors, Xeon processors, and later processors: [31:24] */

	/* Setup stack with 16 byte guard */
	addl	$1,%eax
	movl	$P_DATA_SIZE,%ebx
	mull	%ebx			/* [%edx|%eax] = %eax * %ebx */
	addq	$P_DATA_BASE,%rax
	subq	$P_STACK_GUARD,%rax
	movq	%rax,%rsp

	/* Initialize the bootstrap processor */
	call	_bsp_init
	/* Start the kernel code */
	call	_kmain
	jmp	_halt

/* Entry point for the application processors */
apstart64:
	/* Disable interrupts */
	cli

	/* Re-configure the stack pointer (for alignment) */
	/* Obtain APIC ID */
	movq	$MSR_APIC_BASE,%rcx
	rdmsr
	shlq	$32,%rdx
	addq	%rax,%rdx
	andq	$0xfffffffffffff000,%rdx	/* APIC Base */
	xorq	%rax,%rax
	movl	APIC_LAPIC_ID(%rdx),%eax
	shrl	$24,%eax
	/* P6 family and Pentium processors: [27:24] */
	/* Pentium 4 processors, Xeon processors, and later processors: [31:24] */

	/* Setup stack with 16 byte guard */
	addl	$1,%eax
	movl	$P_DATA_SIZE,%ebx
	mull	%ebx			/* [%edx|%eax] = %eax * %ebx */
	addq	$P_DATA_BASE,%rax
	subq	$P_STACK_GUARD,%rax
	movq	%rax,%rsp

	/* Initialize the application processor */
	call	_ap_init
	/* Start the kernel code */
	call	_kmain
	jmp	_halt

/* void halt(void) */
_halt:
	hlt
	ret

/* void pause(void) */
_pause:
	pause
	ret

/* void lgdt(void *gdtr, u64 selector) */
_lgdt:
	lgdt	(%rdi)
	/* Reload GDT */
	pushq	%rsi
	pushq	$1f	/* Just to do ret */
	lretq
1:
	/* Set data selector */
	movq	%rsi,%rax
	addq	$8,%rax
	movq	%rax,%ds
	movq	%rax,%es
	movq	%rax,%ss
	ret

/* void sgdt(void *gdtr) */
_sgdt:
	sgdt	(%rdi)
	ret

/* void lidt(void *idtr) */
_lidt:
	lidt	(%rdi)
	ret

/* void sidt(void *idtr) */
_sidt:
	sidt	(%rdi)
	ret

/* void lldt(u16) */
_lldt:
	lldt	%di
	ret

/* void ltr(u16) */
_ltr:
	ltr	%di
	ret

/* void sti(void) */
_sti:
	sti
	ret

/* void cli(void) */
_cli:
	cli
	ret

/* u64 rdtsc(void) */
_rdtsc:
	xorq	%rax,%rax
	movq	%rax,%rdx
	rdtscp
	shlq	$32,%rdx
	addq	%rdx,%rax
	ret

/* u8 inb(u16 port) */
_inb:
	movw	%di,%dx
	xorq	%rax,%rax
	inb	%dx,%al
	ret

/* u16 inw(u16 port) */
_inw:
	movw	%di,%dx
	xorq	%rax,%rax
	inw	%dx,%ax
	ret

/* u32 inw(u16 port) */
_inl:
	movw	%di,%dx
	xorq	%rax,%rax
	inl	%dx,%eax
	ret

/* void outb(u16 port, u8 value) */
_outb:
	movw	%di,%dx
	movw	%si,%ax
	outb	%al,%dx
	ret

/* void outw(u16 port, u16 value) */
_outw:
	movw	%di,%dx
	movw	%si,%ax
	outw	%ax,%dx
	ret

/* void outl(u16 port, u32 value) */
_outl:
	movw	%di,%dx
	movl	%esi,%eax
	outl	%eax,%dx
	ret

/* u32 mfread32(u64 addr) */
_mfread32:
	mfence			/* Prevent out-of-order execution */
	movl	(%rdi),%eax
	ret

/* void mfwrite32(u64 addr, u32 val) */
_mfwrite32:
	mfence			/* Prevent out-of-order execution */
	movl	%esi,(%rdi)
	ret

/* u64 cpuid(u64 rax, u64 *rcx, u64 *rdx) */
_cpuid:
	movq	%rdi,%rax
	movq	%rdx,%rdi
	cpuid
	movq	%rcx,(%rsi)
	movq	%rdx,(%rdi)
	ret

/* u64 rdmsr(u64 reg) */
_rdmsr:
	movq	%rdi,%rcx
	rdmsr
	shlq	$32,%rdx
	addq	%rdx,%rax
	ret

/* void wrmsr(u64 reg, u64 data) */
_wrmsr:
	movq	%rdi,%rcx
	movq	%rsi,%rax
	movq	%rax,%rdx
	shrq	$32,%rdx
	wrmsr
	ret

/* void * kmemset(void *b, int c, size_t len) */
_kmemset:
	pushq	%rdi
	pushq	%rsi
	movl	%esi,%eax	/* c */
	movq	%rdx,%rcx	/* len */
	cld			/* Ensure the DF cleared */
	rep	stosb		/* Set %al to (%rdi)-(%rdi+%rcx) */
	popq	%rsi
	popq	%rdi
	movq	%rdi,%rax	/* Restore for the return value */
	ret

/* int kmemcmp(void *s1, void *s2, size_t n) */
_kmemcmp:
	xorq	%rax,%rax
	movq	%rdx,%rcx	/* n */
	cld			/* Ensure the DF cleared */
	repe	cmpsb		/* Compare byte at (%rsi) with byte at (%rdi) */
	jz	1f
	decq	%rdi		/* rollback one */
	decq	%rsi		/* rollback one */
	movb	(%rdi),%al	/* *s1 */
	subb	(%rsi),%al	/* *s1 - *s2 */
1:
	ret

/* int kmemcpy(void *__restrict dst, void *__restrict src, size_t n) */
_kmemcpy:
	movq	%rdi,%rax	/* Return value */
	movq	%rdx,%rcx	/* n */
	cld			/* Ensure the DF cleared */
	rep	movsb		/* Copy byte at (%rsi) to (%rdi) */
	ret

/* u64 bitwidth(u64) */
_bitwidth:
	decq	%rdi
	xorq	%rax,%rax
	testq	%rdi,%rdi
	jz	1f
	bsrq	%rdi,%rax
	incq	%rax
1:
	ret

/* void spin_lock_intr(u32 *) */
_spin_lock_intr:
	cli
/* void spin_lock(u32 *) */
_spin_lock:
	xorl	%ecx,%ecx
	incl	%ecx
1:
	xorl	%eax,%eax
	lock cmpxchgl	%ecx,(%rdi)
	jnz	1b
	ret

/* void spin_unlock(u32 *) */
_spin_unlock:
	xorl	%eax,%eax
	lock xchgl	(%rdi),%eax
	ret

/* void spin_unlock_intr(u32 *) */
_spin_unlock_intr:
	xorl	%eax,%eax
	lock xchgl	(%rdi),%eax
	sti
	ret


/* void syscall_setup(void *, u64 nr) */
_syscall_setup:
	movq	%rdi,(syscall_table)
	movq	%rsi,(syscall_nr)
	/* Write syscall entry point */
	movq	$0xc0000082,%rcx	/* IA32_LSTAR */
	movq	$syscall_entry,%rax
	movq	%rax,%rdx
	shrq	$32,%rdx
	wrmsr
	/* Segment register */
	movq	$0xc0000081,%rcx
	movq	$0x0,%rax
	movq	$(GDT_RING0_CODE_SEL | ((GDT_RING3_CODE32_SEL + 3) << 16)),%rdx
	wrmsr
	/* Enable syscall */
	movl	$0xc0000080,%ecx	/* EFER MSR number */
	rdmsr
	btsl	$0,%eax		/* SYSCALL ENABLE bit [bit 0] = 1 */
	wrmsr
	ret


/* Entry point to a syscall */
syscall_entry:
	cli
	/* rip and rflags are stored in rcx and r11, respectively. */
	pushq	%rbp
	movq	%rsp,%rbp
	pushq	%rcx		/* -8(%rbp): rip */
	pushq	%r11		/* -16(%rbp): rflags */
	pushq	%rbx

	/* Check the number */
	cmpq	(syscall_nr),%rax
	jge	1f

	/* Lookup the system call table and call one corresponding to %rax */
	movq	(syscall_table),%rbx
	cmpq	$0,%rbx
	je	1f
	shlq	$3,%rax		/* 8 byte for each function pointer */
	addq	%rax,%rbx
	movq	%r10,%rcx	/* Replace the 4th argument with %r10 */
	callq	*(%rbx)		/* Call the function */
1:
	popq	%rbx
	popq	%r11
	popq	%rcx
	movq	%rbp,%rsp
	popq	%rbp
	sysretq

/* pid_t sys_fork(void) */
_sys_fork:
	pushq	%rbp
	movq	%rsp,%rbp
	subq	$8,%rsp
	movq	%rsp,%rdi
	subq	$8,%rsp
	movq	%rsp,%rsi
	subq	$8,%rsp
	movq	%rsp,%rdx
	call	_sys_fork_c
	addq	$24,%rsp
	cmpl	$0,%eax
	jne	1f
	movq	-8(%rsp),%rdi
	movq	-16(%rsp),%rsi
	movq	-24(%rsp),%rdx
	call	sys_fork_restart
1:
	/* Return upon error */
	leaveq
	ret

/* void sys_fork_restart(u64 task, u64 ret0, u64, ret1) */
sys_fork_restart:
	movq	%rdx,%rax
	leaveq			/* Restore the stack */
	addq	$8,%rsp		/* Pop the return point */
	pushq	%rdi
	pushq	%rsi

	/* Setup the stackframe for the forked task */
	movq	TASK_RP(%rdi),%rdx
	addq	$164,%rdx
	movq	$GDT_RING0_DATA_SEL,%rcx
	movq	%rcx,-8(%rdx)	/* ss */
	movq	%rbp,%rcx
	movq	%rcx,-16(%rdx)	/* rsp */
	pushfq
	popq	%rcx
	movq	%rcx,-24(%rdx)	/* rflags */
	movq	$GDT_RING0_CODE_SEL,%rcx
	movq	%rcx,-32(%rdx)	/* cs */
	movq	$1f,%rcx
	movq	%rcx,-40(%rdx)	/* rip */
	movq	%rsi,-48(%rdx)	/* rax */
	movq	-24(%rbp),%rcx
	movq	%rcx,-56(%rdx)	/* rbx */
	movq	-8(%rbp),%rcx
	movq	%rcx,-64(%rdx)	/* rcx */
	movq	-16(%rbp),%rcx
	movq	%rcx,-104(%rdx)	/* r11 */
	movq	%r12,-112(%rdx)	/* r12 */
	movq	%r13,-120(%rdx)	/* r13 */
	movq	%r14,-128(%rdx)	/* r14 */
	movq	%r15,-136(%rdx)	/* r15 */
	movq	-32(%rbp),%rcx
	movq	%rcx,-144(%rdx)	/* rsi */
	movq	-40(%rbp),%rcx
	movq	%rcx,-152(%rdx)	/* rdi */
	movq	0(%rbp),%rcx
	movq	$0xabcd,%rcx
	movq	%rcx,-160(%rdx)	/* rbp */
	movw	$(GDT_RING3_DATA_SEL+3),%cx
	movw	%cx,-162(%rdx)	/* fs */
	movw	%cx,-164(%rdx)	/* gs */

	/* Restore */
	popq	%rsi
	popq	%rdi
	popq	%rbx
	popq	%r11
	popq	%rcx
	movq	%rbp,%rsp
	popq	%rbp
	sysretq
1:
	popq	%rbp
	sysretq


/* void asm_ioapic_map_intr(u64 val, u64 tbldst, u64 ioapic_base) */
_asm_ioapic_map_intr:
	/* Copy arguments */
	movq	%rdi,%rax       /* src */
	movq	%rsi,%rcx       /* tbldst */
	/* rdx = ioapic_base */

	/* *(u32 *)(ioapic_base + 0x00) = tbldst * 2 + 0x10 */
	shlq	$1,%rcx         /* tbldst * 2 */
	addq	$0x10,%rcx      /* tbldst * 2 + 0x10 */
	sfence
	movl	%ecx,0x00(%rdx) /* IOREGSEL (0x00) */
	/* *(u32 *)(ioapic_base + 0x10) = (u32)src */
	sfence
	movl	%eax,0x10(%rdx) /* IOWIN (0x10) */
	shrq	$32,%rax
	/* *(u32 *)(ioapic_base + 0x00) = tbldst * 2 + 0x10 + 1 */
	addq	$1,%rcx         /* tbldst * 2 + 0x10 + 1 */
	sfence
	movl	%ecx,0x00(%rdx)
	/* *(u32 *)(ioapic_base + 0x10) = (u32)(src >> 32) */
	sfence
	movl	%eax,0x10(%rdx)
	ret

/* u64 get_cr0(void) */
_get_cr0:
	movq	%cr0,%rax
	ret

/* void set_cr0(u64 cr0) */
_set_cr0:
	movq	%rdi,%cr0
	ret

/* void * get_cr3(void) */
_get_cr3:
	movq	%cr3,%rax
	ret

/* void set_cr3(void *) */
//_set_cr3:
//	movq	%rdi,%cr3
//	ret

/* u64 get_cr4(void) */
_get_cr4:
	movq	%cr4,%rax
	ret

/* void set_cr4(u64) */
_set_cr4:
	movq	%rdi,%cr4
	ret

/* void invlpg(void *) */
_invlpg:
	invlpg	(%rdi)
	ret

/* int vmxon(void *) */
_vmxon:
	vmxon	(%rdi)
	jz	1f
	sbbq	%rax,%rax
	ret
1:
	movq	$-1,%rax
	ret

/* int vmclear(void *) */
_vmclear:
	vmclear	(%rdi)
	jz	1f
	sbbq	%rax,%rax
	ret
1:
	movq	$-1,%rax
	ret

/* int vmptrld(void *) */
_vmptrld:
	vmptrld	(%rdi)
	jz	1f
	sbbq	%rax,%rax
	ret
1:
	movq	$-1,%rax
	ret

/* int vmwrite(u64, u64) */
_vmwrite:
	vmwrite	%rsi,%rdi
	jz	1f
	sbbq	%rax,%rax
	ret
1:
	movq	$-1,%rax
	ret

/* u64 vmread(u64) */
_vmread:
	vmread	%rdi,%rax
	ret


/* int vmlaunch(void) */
_vmlaunch:
	vmlaunch
	jz	1f
	sbbq	%rax,%rax
	ret
1:
	movq	$-1,%rax
	ret

/* int vmresume(void) */
_vmresume:
	vmresume
	jz	1f
	sbbq	%rax,%rax
	ret
1:
	movq	$-1,%rax
	ret

/* Null function for interrupt handler */
_intr_null:
	pushq	%rax
	pushq	%rcx
	pushq	%rdx
	/* APIC EOI */
	movq	$MSR_APIC_BASE,%rcx
	rdmsr			/* Read APIC info to [%edx:%eax]; N.B., higer */
				/*  32 bits of %rax and %rdx are cleared */
				/*  bit [35:12]: APIC Base, [11]: EN */
				/*  [10]: EXTD, and [8]:BSP */
	shlq	$32,%rdx
	addq	%rax,%rdx
	andq	$0xfffffffffffff000,%rdx	/* APIC Base */
	movl	$0,APIC_EOI(%rdx)	/* EOI */
	popq	%rdx
	popq	%rcx
	popq	%rax
	iretq

/* Debug fault or trap */
_intr_debug:
	pushq	%rbp
	movq	%rsp,%rbp
	pushq	%rbx
	movq	16(%rbp),%rbx
	movq	%rbx,%dr0	/* cs */
	movq	8(%rbp),%rbx
	movq	%rbx,%dr1	/* rip */
	movq	32(%rbp),%rbx
	movq	%rbx,%dr2	/* rsp */
	call	_isr_debug
	popq	%rbx
	popq	%rbp
	iretq

_intr_dze:
_intr_nmi:
_intr_breakpoint:
_intr_overflow:
_intr_bre:
_intr_dna:
_intr_df:
_intr_snpf:
_intr_ssf:
	iretq

/* Interrupt handler for invalid opcode exception */
_intr_iof:
	pushq	%rbp
	movq	%rsp,%rbp
	pushq	%rdi
	pushq	%rbx
	movq	16(%rbp),%rbx
	//movq	%rbx,%dr0	/* cs */
	movq	8(%rbp),%rdi
	//movq	%rbx,%dr1	/* rip */
	movq	32(%rbp),%rbx
	//movq	%rbx,%dr2	/* rsp */
	call	_isr_io_fault
	popq	%rbx
	popq	%rdi
	popq	%rbp
	iretq

/* Interrupt handler for general protection fault
 * Error code, RIP, CS, RFLAGS, (RSP, SS) */
_intr_gpf:
	pushq	%rbp
	movq	%rsp,%rbp
	pushq	%rbx
	movq	16(%rbp),%rbx	/* rip */
	movq	8(%rbp),%rbx	/* error code */
	movq	40(%rbp),%rbx	/* rsp */
	movq	48(%rbp),%rbx	/* ss */
	//movq	(gpf_reentry),%rbx
	//cmpq	$0,%rbx
	//jz	1f
	//movq	%rbx,16(%rbp)   /* Overwrite the reentry point (%rip) */
	pushq	%rdi
	pushq	%rsi
	movq	16(%rbp),%rdi	/* rip */
	movq	8(%rbp),%rsi	/* error code */
	call	_isr_general_protection_fault
	popq	%rsi
	popq	%rdi
1:	popq	%rbx
	popq	%rbp
	addq	$0x8,%rsp
	iretq


/* Interrupt handler for page fault
 * Error code, RIP, CS, RFLAGS, (RSP, SS) */
_intr_pf:
	pushq	%rbp
	movq	%rsp,%rbp
	pushq	%rdi
	pushq	%rsi
	pushq	%rdx
	movq	16(%rbp),%rdi	/* rip */
	movq	%cr2,%rsi	/* virtual address */
	movq	8(%rbp),%rdx	/* error code */
	//movq	%rdi,%dr0
	//movq	%rsi,%dr1
	//movq	%rdx,%dr2
	call	_isr_page_fault
	popq	%rdx
	popq	%rsi
	popq	%rdi
	popq	%rbp
	addq	$0x8,%rsp
	iretq

/* x87 floating point exception
 * RIP, CS, RFLAGS, (RSP, SS): without error code */
_intr_x87_fpe:
	iretq

/* SIMD floating point exception
 * RIP, CS, RFLAGS, (RSP, SS): without error code */
_intr_simd_fpe:
	iretq


/* macro to save registers to the stackframe and call the interrupt handler */
.macro	intr_lapic_isr vec
	pushq	%rax
	pushq	%rbx
	pushq	%rcx
	pushq	%rdx
	pushq	%r8
	pushq	%r9
	pushq	%r10
	pushq	%r11
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15
	pushq	%rsi
	pushq	%rdi
	pushq	%rbp
	pushw	%fs
	pushw	%gs
	/* Call kernel function for ISR */
	movq	$\vec,%rdi
	callq	_kintr_isr
	/* EOI for the local APIC */
	movq	$MSR_APIC_BASE,%rcx
	rdmsr			/* Read APIC info to [%edx:%eax]; N.B., higer */
				/*  32 bits of %rax and %rdx are cleared */
				/*  bit [35:12]: APIC Base, [11]: EN */
				/*  [10]: EXTD, and [8]:BSP*/
	shlq	$32,%rdx
	addq	%rax,%rdx
	andq	$0xfffffffffffff000,%rdx	/* APIC Base */
	movl	$0,APIC_EOI(%rdx)	/* EOI */
.endm

/* macro to restore from the stackframe */
.macro	intr_lapic_isr_done
	/* Pop all registers from the stackframe */
	popw	%gs
	popw	%fs
	popq	%rbp
	popq	%rdi
	popq	%rsi
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%r11
	popq	%r10
	popq	%r9
	popq	%r8
	popq	%rdx
	popq	%rcx
	popq	%rbx
	popq	%rax
.endm


/* Interrupt handler for local APIC timer */
_intr_apic_loc_tmr:
	intr_lapic_isr 0x50
	jmp	_task_restart


/* Crash interrupt */
_intr_crash:
	cli
1:
	hlt
	jmp	1b


/* Task restart */
_task_restart:
	/* Get the APIC ID */
	movq	$MSR_APIC_BASE,%rcx
	rdmsr
	shlq	$32,%rdx
	addq	%rax,%rdx
	andq	$0xfffffffffffff000,%rdx	/* APIC Base */
	xorq	%rax,%rax
	movl	APIC_LAPIC_ID(%rdx),%eax
	/* Calculate the processor data space from the APIC ID */
	movq	$P_DATA_SIZE,%rbx
	mulq	%rbx		/* [%rdx:%rax] = %rax * %rbx */
	addq	$P_DATA_BASE,%rax
	movq	%rax,%rbp
	/* If the next task is not scheduled, immediately restart this task */
	cmpq	$0,P_NEXT_TASK_OFFSET(%rbp)
	jz	2f
	movq	P_NEXT_TASK_OFFSET(%rbp),%rax
	/* If the current task is null, then do not need to save anything */
	cmpq	$0,P_CUR_TASK_OFFSET(%rbp)
	jz	1f
	/* Save the stack pointer (restart point) */
	movq	P_CUR_TASK_OFFSET(%rbp),%rax
	movq	%rsp,TASK_RP(%rax)
1:
	/* Notify that the current task is switched (to the kernel) */
	movq	P_CUR_TASK_OFFSET(%rbp),%rdi
	movq	P_NEXT_TASK_OFFSET(%rbp),%rsi
	callq	_arch_task_switched
	/* Task switch (set the stack frame of the new task) */
	movq	P_NEXT_TASK_OFFSET(%rbp),%rax
	movq	%rax,P_CUR_TASK_OFFSET(%rbp)
	movq	TASK_RP(%rax),%rsp
	movq	$0,P_NEXT_TASK_OFFSET(%rbp)
	/* Change page table */
	movq	TASK_CR3(%rax),%rax
	movq	%rax,%cr3
	/* Setup sp0 in TSS */
	movq	P_CUR_TASK_OFFSET(%rbp),%rax
	movq	TASK_SP0(%rax),%rdx
	leaq	P_TSS_OFFSET(%rbp),%rax
	movq	%rdx,TSS_SP0(%rax)
2:
	intr_lapic_isr_done
	iretq

/* Replace the current task with the task pointed  by %rdi */
_task_replace:
	movq	TASK_RP(%rdi),%rsp
	/* Change page table */
	movq	TASK_CR3(%rdi),%rax
	movq	%rax,%cr3
	/* Get the APIC ID */
	movq	$MSR_APIC_BASE,%rcx
	rdmsr
	shlq	$32,%rdx
	addq	%rax,%rdx
	andq	$0xfffffffffffff000,%rdx	/* APIC Base */
	xorq	%rax,%rax
	movl	APIC_LAPIC_ID(%rdx),%eax
	/* Calculate the processor data space from the APIC ID */
	movq	$P_DATA_SIZE,%rbx
	mulq	%rbx		/* [%rdx:%rax] = %rax * %rbx */
	addq	$P_DATA_BASE,%rax
	movq	%rax,%rbp
	/* Setup sp0 in TSS */
	movq	TASK_SP0(%rdi),%rdx
	leaq	P_TSS_OFFSET(%rbp),%rax
	movq	%rdx,TSS_SP0(%rax)
	intr_lapic_isr_done
	iretq


	.data
syscall_table:
	.quad	0
syscall_nr:
	.quad	0
