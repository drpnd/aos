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

	/* Temporary GDT for application processors */
	.set	AP_GDT_CODE64_SEL,0x08	/* Code64 selector */
	.set	AP_GDT_CODE32_SEL,0x10	/* Code32 selector */
	.set	AP_GDT_CODE16_SEL,0x18	/* Code16 selector */
	.set	AP_GDT_DATA_SEL,0x20	/* Data selector */

	/* Kernel page table */
	.set	KERNEL_PGT	0x00079000
	/* Per-processor information */
	.set	P_DATA_BASE	0x01000000
	.set	P_DATA_SIZE	0x10000
	.set	P_STACK_GUARD	0x10
