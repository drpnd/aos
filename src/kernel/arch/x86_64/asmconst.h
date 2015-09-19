/* -*- Mode: asm -*- */
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

	/* GDT selectors */
	.set	GDT_RING0_CODE_SEL,0x08
	.set	GDT_RING0_DATA_SEL,0x10
	.set	GDT_RING3_CODE32_SEL,0x18
	.set	GDT_RING3_DATA_SEL,0x20
	.set	GDT_RING3_CODE64_SEL,0x28

	/* Temporary GDT for application processors */
	.set	AP_GDT_CODE64_SEL,0x08	/* Code64 selector */
	.set	AP_GDT_CODE32_SEL,0x10	/* Code32 selector */
	.set	AP_GDT_CODE16_SEL,0x18	/* Code16 selector */
	.set	AP_GDT_DATA64_SEL,0x20	/* Data selector */
	.set	AP_GDT_DATA32_SEL,0x28	/* Data selector */

	.set	APIC_BASE,0xfee00000

	/* # of interrupts */
	.set	IDT_NR,256
	/* Kernel page table */
	.set	KERNEL_PGT,0x00079000
	/* Per-processor information (struct p_data) */
	.set	P_DATA_BASE,0x01000000
	.set	P_DATA_SIZE,0x10000
	.set	P_STACK_GUARD,0x10
	.set	P_TSS_SIZE,104	/* sizeof(struct tss) */
	.set	P_TSS_OFFSET,(0x20 + IDT_NR * 8)	/* struct tss */
	.set	P_CUR_TASK_OFFSET,(P_TSS_OFFSET + P_TSS_SIZE)	/* cur_task */
        .set	P_NEXT_TASK_OFFSET,(P_CUR_TASK_OFFSET + 8)	/* next_task */
	/* Task information (struct arch_task) */
	.set	TASK_RP,0
	.set	TASK_SP0,8
	.set	TASK_CR3,16
	.set	TASK_KTASK,40
	/* TSS */
	.set	TSS_SP0,4
	/* Trampoline */
	.set	TRAMPOLINE_VEC,0x70

	/* Syscall */
	.set	SYSCALL_MAX_NR,0x10
