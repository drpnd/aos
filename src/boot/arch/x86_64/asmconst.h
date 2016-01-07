/* -*- Mode: asm -*- */
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

	.set	KERNEL_BASE,0x10000
	.set	INITRAMFS_BASE,0x30000
	.set	KERNEL_PGT,0x00079000	/* Page table */
	.set	P_DATA_SIZE,0x10000	/* Data size for each processor */
	.set	P_DATA_BASE,0x1000000	/* Data base for each processor */
	.set	P_STACK_GUARD,0x10

	.set	BOOTINFO_BASE,0x8000	/* Boot information base address */
	.set	BOOTINFO_SIZE,0x100	/* Size of boot info structure */
	/* struct bootinfo {
	 * 	u64 mm_num;		// # of entries
	 * 	u64 mm_ptr;		// pointer to the memory map table
	 * offset: --0x00ff unused space
	 * offset: 0x0100-- memory map table
	 * };
	 */
	.set	BOOTINFO_MM_NUM,BOOTINFO_BASE
	.set	BOOTINFO_MM_PTR,BOOTINFO_BASE+8
	.set	BOOTINFO_MM_TBL,BOOTINFO_BASE+BOOTINFO_SIZE

	.set	NUM_RETRIES,3		/* # of retries for disk read */
	.set	ERRCODE_TIMEOUT,0x80	/* Error code: Timeout */
	.set	SECTOR_SIZE,0x200	/* 512 bytes / sector */
	.set	BUFFER,0x6000		/* Buffer: 6000-61ff */

	.set	APIC_LAPIC_ID,0x020
	.set	MSR_APIC_BASE,0x1b
