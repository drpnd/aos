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
	.globl	kernload

	/* struct fat_bpb_common */
	.set	bs_jmp_boot,0		/* u8 [3] */
	.set	bs_oem_name,3		/* u8 [8] */
	.set	bpb_bytes_per_sec,11	/* u16 */
	.set	bpb_sec_per_clus,13	/* u8 */
	.set	bpb_rsvd_sec_cnt,14	/* u8 */
	.set	bpb_num_fats,15		/* u8 */
	.set	bpb_num_fats,16		/* u8 */
	.set	bpb_root_ent_cnt,17	/* u16 */
	.set	bpb_total_sec16,19	/* u16 */
	.set	bpb_media,21		/* u8 */
	.set	bpb_fat_sz16,22		/* u16 */
	.set	bpb_sec_per_trk,24	/* u16 */
	.set	bpb_num_heads,26	/* u16 */
	.set	bpb_hidden_sec,28	/* u32 */
	.set	bpb_total_sec32,32	/* u32 */

	/* struct fat_bpb_fat12_16 */
	.set	bs_drv_num,36		/* u8 */
	.set	bs_reserved1,37		/* u8 */
	.set	bs_boot_sig,38		/* u8 */
	.set	bs_vol_id,39		/* u32 */
	.set	bs_vol_lab,43		/* u8 [11] */
	.set	bs_file_sys_type,54	/* u8 [8] */

	/* struct fat_bpb_fat32 */
	.set	bpb_fat_sz32,36		/* u32 */
	.set	bpb_ext_flags,40	/* u16 */
	.set	bpb_fs_ver,42		/* u16 */
	.set	bpb_root_clus,44	/* u32 */
	.set	bpb_fs_info,48		/* u16 */
	.set	bpb_bk_boot_sec,50	/* u16 */
	.set	bpb_reserved,52		/* u8 [12] */
	.set	bs32_drv_num,64		/* u8 */
	.set	bs32_reserved1,65		/* u8 */
	.set	bs32_boot_sig,66		/* u8 */
	.set	bs32_vol_id,67		/* u32 */
	.set	bs32_vol_lab,71		/* u8 [11] */
	.set	bs32_file_sys_type,82	/* u8 [8] */

/* Load kernel */
kernload:
	pushw	%ax
	pushw	%bx
	pushw	%cx
	pushw	%dx

	popw	%dx
	popw	%cx
	popw	%bx
	popw	%ax
	ret

	.data

/* Partition information */
lba:
	.long	0
nsec:
	.long	0
