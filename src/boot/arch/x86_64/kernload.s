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

	.set	NUM_RETRIES,3		/* # of retries for disk read */
	.set	ERRCODE_TIMEOUT,0x80	/* Error code: Timeout */
	.set	SECTOR_SIZE,0x200	/* 512 bytes / sector */

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
	/* Save registers */
	pushw	%ax
	pushw	%bx
	pushw	%cx
	pushw	%dx

	movb	%dl,(drive)	/* Save the drive */
	call	get_drive_params

	/* Restore registers */
	popw	%dx
	popw	%cx
	popw	%bx
	popw	%ax
	ret


/* Get the drive parameters of drive %dl.  %ax returns the status code. */
get_drive_params:
	pushw	%ax
	pushw	%cx
	pushw	%dx
	pushw	%di
	pushw	%es
	xorw	%ax,%ax
	movw	%ax,%es		/* Set %es:%di */
	movw	%ax,%di		/*  to 0x0000:0x0000 */
	movb	$0x08,%ah	/* Function: Read drive parameter */
	int	$0x13
	jc	1f		/* Error on read */
	/* Save the sector information */
	incb	%dh		/* Get # of heads (%dh: last index of heads) */
	movb	%dh,heads	/* Store */
	movb	%cl,%al		/* %cl[5:0]: last index of sectors per track */
	andb	$0x3f,%al	/*  N.B., sector starting with 1 */
	movb	%al,sectors	/* Store */
	movb	%ch,%al		/* %cx[7:6]%cx[15:8]: last index of cylinders */
				/*  then copy %cx[15:8] to %al */
	movb	%cl,%ah		/* Lower byte to higher byte */
	shrb	$6,%ah		/* Pick most significant two bits */
	incw	%ax		/*  N.B., cylinder starting with 0 */
	movw	%ax,cylinders
	clc			/* Ensure that no error occurred */
1:
	popw	%es
	popw	%di
	popw	%dx
	popw	%cx
	popw	%ax
	ret


/* Read %cx sectors starting at LBA %ax on drive %dl into %es:[%bx] */
read:
	pushw	%bp		/* Save the base pointer*/
	movw	%sp,%bp		/* Copy the stack pointer to the base pointer */
	/* Save general purpose registers */
	movw	%ax,-2(%bp)
	movw	%bx,-4(%bp)
	movw	%cx,-6(%bp)
	movw	%dx,-8(%bp)
	/* Prepare space for local variables */
	/* u16 cx -10(%bp) */
	/* u16 counter -12(%bp) */
	subw	$12,%sp

	/* Reset counter */
	xorw	%ax,%ax
	movw	%ax,-12(%bp)

	/* Set number of sectors to be read */
	movw	%cx,-10(%bp)
1:
	movw	-2(%bp),%ax	/* Restore %ax */
	addw	-12(%bp),%ax	/* Current LBA */
	call	lba2chs		/* Convert LBA (%ax) to CHS (%cx,%dh) */
	call	read_sector	/* Read a sector */
	addw	$SECTOR_SIZE,%bx
	movw	-12(%bp),%ax	/* Get */
	incw	%ax		/*  and increase the current LBA %ax */
	movw	%ax,-12(%bp)	/*  then write back */
	cmpw	-10(%bp),%ax
	jb	1b		/* Need to read more sectors */

	/* Restore the saved registers */
	movw	-8(%bp),%dx
	movw	-6(%bp),%cx
	movw	-4(%bp),%bx
	movw	-2(%bp),%ax
	movw	%bp,%sp		/* Restore the stack pointer and base pointer */
	popw	%bp
	ret


/* Read one sector from CHS (specified by %dh and %cx) specified on drive %dl to
 * %es:[%bx]
 */
read_sector:
	pushw	%bp		/* Save the base pointer*/
	movw	%sp,%bp		/* Copy the stack pointer to the base pointer */
	/* Save registers */
	movw	%ax,-2(%bp)
	/* Prepare space for local variables */
	/* u16 retries -4(%bp) */
	/* u16 error -6(%bp) */
	subw	$6,%sp
	/* Reset retry counter */
	xorw	%ax,%ax
	movw	%ax,-4(%bp)
1:
	/* Read a sector from the drive */
	movb	$0x02,%ah	/* Function: Read sectors from drive */
	movb	$0x01,%al	/* # of sectors to be read */
	int	$0x13
	jnc	3f		/* Jump if success */
	movw	%ax,-6(%bp)	/* Save the return code */
	movw	-4(%bp),%ax	/* Get */
	incw	%ax		/*  and increase the number of retries */
	movw	%ax,-4(%bp)	/*  then write back */
	cmpw	$NUM_RETRIES,%ax
	movw	-6(%bp),%ax	/* Restore the return code */
	ja	2f		/* Exceeding the maximum number of retries */
	cmpb	$ERRCODE_TIMEOUT,%ah
	je	2f		/* Timeout */
	clc			/* Ensure that no error occurred */
	jmp	3f
2:
	stc			/* Set carry flag to notify the error */
3:
	/* Restore the saved registers */
	movw	-2(%bp),%ax
	movw	%bp,%sp		/* Restore the stack pointer and base pointer */
	popw	%bp
	ret

	/* Restore the saved registers */
	movw	-2(%bp),%ax
	movw	%bp,%sp		/* Restore the stack pointer and base pointer */
	popw	%bp
	ret


/* Calculate CHS (%cx[7:6]%cx[15:8] ,%dh, %cx[5:0]) from LBA (%ax) */
lba2chs:
	/* Save registers */
	pushw	%ax
	pushw	%bx
	pushw	%dx
	/* Compute sector number */
	xorw	%bx,%bx
	movw	%bx,%dx
	movw	%bx,%cx
	movb	sectors,%bl
	divw	%bx		/* %dx:%ax / %bx; %ax:quotient, %dx:remainder */
	incb	%dl		/* Sector number is one-based numbering */
	movb	%dl,%cl		/* Sector: %cx[5:0] */
	/* Compute head and track (cylinder) numbers */
	xorw	%bx,%bx
	movw	%bx,%dx
	movb	heads,%bl
	divw	%bx		/* %dx:%ax / %bx; %ax:cylinder, %dx:head */
	movb	%al,%ch		/* Cylinder[7:0]: %cx[15:8] */
	movb	%ah,%bl		/* Take the least significant two bits */
	shlb	$6,%bl		/*  from %ah, and copy to %bl[7:6] */
	orb	%bl,%cl		/* Cylinder[9:8]: %cx[7:6]*/
	movw	%dx,%bx		/* Save the remainer to %bx */
	popw	%dx		/* Restore %dx */
	movb	%bl,%dh		/* Head */
	/* Restore registers */
	popw	%bx
	popw	%ax
	ret


	.data

/* Partition information */
lba:
	.long	0
nsec:
	.long	0

/* Drive information */
drive:
	.byte	0
heads:
	.byte	0
cylinders:
	.word	0
sectors:
	.byte	0
