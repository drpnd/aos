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

	.include	"asmconst.h"

	.text

	.code16
	.globl	kernload

	/* struct fat_bpb_common */
	.set	bs_jmp_boot,0		/* u8 [3] */
	.set	bs_oem_name,3		/* u8 [8] */
	.set	bpb_bytes_per_sec,11	/* u16 */
	.set	bpb_sec_per_clus,13	/* u8 */
	.set	bpb_rsvd_sec_cnt,14	/* u16 */
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
	.set	bs32_reserved1,65	/* u8 */
	.set	bs32_boot_sig,66	/* u8 */
	.set	bs32_vol_id,67		/* u32 */
	.set	bs32_vol_lab,71		/* u8 [11] */
	.set	bs32_file_sys_type,82	/* u8 [8] */

/* Load kernel from the drive %dl */
kernload:
	pushw	%bp
	movw	%sp,%bp
	/* Save registers */
	movl	%eax,-4(%bp)
	movl	%ebx,-8(%bp)
	movl	%ecx,-12(%bp)
	movl	%edx,-16(%bp)
	movw	%ds,-18(%bp)
	movw	%es,-20(%bp)
	movw	%fs,-22(%bp)
	movw	%gs,-24(%bp)
	/* u32 fat_start_bytes -28(%bp) */
	/* u32 root_dir_start_sec -32(%bp) */
	/* u32 root_dir_sectors -36(%bp) */
	/* u32 data_start_sec -40(%bp) */
	/* u32 data_sectors -44(%bp) */
	/* u32 num_clus -48(%bp) */
	/* u32 root_dir_bytes -52(%bp) */
	/* u32 root_dir_start_bytes -56(%bp) */
	/* u32 data_start_bytes -60(%bp) */
	/* u32 sec512_per_cluster -64(%bp) */
	/* u32 kernel_cluster -68(%bp) */
	/* u32 kernel_size -72(%bp) */
	/* u32 kernel_base -76(%bp) */
	subw	$76,%sp

	/* Zero %ds and %es */
	xorw	%ax,%ax
	movw	%ax,%ds
	movw	%ax,%es

	/* Save the drive number and get the parameter */
	movb	%dl,(drive)
	call	get_drive_params

	/* Read the first sector of the drive */
	movl	$0,%eax
	call	read_to_buf

	/* Check the partition table in the MBR */
	movw	$(BUFFER >> 4),%ax
	movw	%ax,%fs		/* %fs = segment for the BUFFER*/
	movw	$(BUFFER & 0xf),%bx	/* %bx = offset */
	testb	$0x80,%fs:0x1be+0x0(%bx)	/* Bootable? */
	jz	kload_error	/* No, then print an error message */
	movl	%fs:0x1be+0x8(%bx),%eax	/* LBA of first absolute sector */
	movl	%fs:0x1be+0xc(%bx),%ecx	/* Size in sectors */
	movl	%eax,(part_lba)	/* Save the starting LBA */
	movl	%ecx,(part_nsec)	/* Save the number of sectors */

	/* Read the first sector in the partition */
	movl	(part_lba),%eax
	call	read_to_buf

	/* Parse the BIOS Parameter Block (BPB) in the volume boot record */
	movw	%fs:bpb_fat_sz16(%bx),%ax
	cmpw	$0,%fs:bpb_fat_sz16(%bx)
	je	kload_error		/* Not support FAT32 */
	xorl	%ecx,%ecx
	movw	%fs:bpb_rsvd_sec_cnt(%bx),%cx	/* start_sec (=1) */
	movw	%fs:bpb_fat_sz16(%bx),%ax	/* FAT size */
	movw	%fs:bpb_num_fats(%bx),%dx	/* # of FAT regions (=2) */
	mulw	%dx		/* %dx:%ax = %ax * %dx */
	shll	$16,%edx
	andl	$0xffff,%eax
	addl	%edx,%eax
	addl	%eax,%ecx	/* root_dir_start_sec = start_sec + sectors */
	movl	%ecx,-32(%bp)	/* root_dir_start_sec */

	movw	%fs:bpb_root_ent_cnt(%bx),%ax
	andl	$0xffff,%eax
	shll	$5,%eax
	movw	%fs:bpb_bytes_per_sec(%bx),%dx
	andl	$0xffff,%edx
	addl	%edx,%eax
	decl	%eax
	movl	%eax,%edx
	shrl	$16,%edx
	divw	%fs:bpb_bytes_per_sec(%bx)	/* %dx:%ax/m Q=%ax, R=%dx */
	andl	$0xffff,%eax
	movl	%eax,-36(%bp)	/* root_dir_sectors */
	addl	%ecx,%eax	/* data_start_sec */
				/*  = root_dir_start_sec + root_dir_sectors */
	movl	%eax,-40(%bp)	/* data_start_sec */

	movw	%fs:bpb_rsvd_sec_cnt(%bx),%ax
	movw	%fs:bpb_bytes_per_sec(%bx),%dx
	mulw	%dx
	shll	$16,%edx
	andl	$0xffff,%eax
	addl	%edx,%eax
	movl	%eax,-28(%bp)	/* fat_start_bytes */

	cmpw	$0,%fs:bpb_total_sec16(%bx)
	jne	1f
	/* The number of sectors is no less than 0x10000 */
	movl	%fs:bpb_total_sec32(%bx),%edx
	subl	%eax,%edx
	jmp	2f
1:
	/* The number of sectors is less than 0x10000 */
	movw	%fs:bpb_total_sec16(%bx),%dx
	andl	$0xffff,%edx
	subl	%eax,%edx
2:
	movl	%edx,-44(%bp)	/* data_sectors */

	movl	%edx,%eax
	shrl	$16,%edx
	divw	%fs:bpb_sec_per_clus(%bx)	/* %dx:%ax/m Q=%ax, R=%dx */
	andl	$0xffff,%eax
	movl	%eax,-48(%bp)	/* num_clus */

	movl	-36(%bp),%eax	/* root_dir_sectors */
	movw	%fs:bpb_bytes_per_sec(%bx),%dx
	andl	$0xffff,%edx
	mull	%edx		/* %edx:%eax = %eax * %edx */
	movl	%eax,-52(%bp)	/* root_dir_bytes */
	movl	-32(%bp),%eax	/* root_dir_start_sec */
	movw	%fs:bpb_bytes_per_sec(%bx),%dx
	mull	%edx		/* %edx:%eax = %eax * %edx */
	movl	%eax,-56(%bp)	/* root_dir_start_bytes */

	movl	-40(%bp),%eax
	xorl	%edx,%edx
	movw	%fs:bpb_bytes_per_sec(%bx),%dx
	mull	%edx
	movl	%eax,-60(%bp)	/* data_start_bytes */

	xorl	%edx,%edx
	movl	%edx,%eax
	movl	%edx,%ecx
	movw	%fs:bpb_bytes_per_sec(%bx),%ax
	movb	%fs:bpb_sec_per_clus(%bx),%cl
	mull	%ecx
	movl	$SECTOR_SIZE,%ecx
	divl	%ecx
	testl	%edx,%edx
	jnz	kload_error
	movl	%eax,-64(%bp)	/* sec512_per_cluster */

	/* In root directory */
	movl	-56(%bp),%eax
	xorl	%ecx,%ecx
	movw	%fs:bpb_root_ent_cnt(%bx),%cx
	/* Find the kernel */
	movw	$fname_kernel,%si
	call	find_file
	/* Read the file */
	movl	%eax,%esi
	movl	-28(%bp),%ebx	/* fat_start_bytes */
	movl	-60(%bp),%edx	/* data_start_bytes */
	movl	$KERNEL_BASE,%edi
	/* Check the FAT type */
	movl	-48(%bp),%eax	/* num_clus */
	cmpl	$4085,%eax
	jle	3f		/* FAT12 */
	cmpl	$65525,%eax
	jle	4f		/* FAT16 */
	jmp	kload_error	/* Invalid filesystem */
3:
	/* FAT12 */
	movl	-64(%bp),%eax	/* sec512_per_cluster */
	call	read_file_fat12
	jmp	5f
4:
	/* FAT16 */
	movl	-64(%bp),%eax	/* sec512_per_cluster */
	call	read_file_fat16
5:

	/* In root directory */
	movl	-56(%bp),%eax
	xorl	%ecx,%ecx
	movw	%fs:bpb_root_ent_cnt(%bx),%cx
	/* Find the initramfs */
	movw	$fname_initramfs,%si
	call	find_file
	/* Read the file */
	movl	%eax,%esi
	movl	-28(%bp),%ebx	/* fat_start_bytes */
	movl	-60(%bp),%edx	/* data_start_bytes */
	movl	$INITRAMFS_BASE,%edi
	/* Check the FAT type */
	movl	-48(%bp),%eax	/* num_clus */
	cmpl	$4085,%eax
	jle	6f		/* FAT12 */
	cmpl	$65525,%eax
	jle	7f		/* FAT16 */
	jmp	kload_error	/* Invalid filesystem */
6:
	/* FAT12 */
	movl	-64(%bp),%eax	/* sec512_per_cluster */
	call	read_file_fat12
	jmp	8f
7:
	/* FAT16 */
	movl	-64(%bp),%eax	/* sec512_per_cluster */
	call	read_file_fat16
8:

	/* Restore registers */
	movw	-24(%bp),%gs
	movw	-22(%bp),%fs
	movw	-20(%bp),%es
	movw	-18(%bp),%ds
	movl	-16(%bp),%edx
	movl	-12(%bp),%ecx
	movl	-8(%bp),%ebx
	movl	-4(%bp),%eax
	movw	%bp,%sp
	popw	%bp
	ret


/*
 * Find a file
 *  Arguments
 *   %eax: base address
 *   %ecx: # of etries
 *   %si: pointer to the file name
 *  Return values
 *   %eax: first cluster of the kernel
 *   %ecx: kernel size
 */
find_file:
	pushw	%bp
	movw	%sp,%bp
	movl	%eax,-4(%bp)
	movl	%ebx,-8(%bp)
	movl	%ecx,-12(%bp)
	movl	%edx,-16(%bp)
	movw	%ds,-18(%bp)
	movw	%es,-20(%bp)
	movw	%fs,-22(%bp)
	movw	%gs,-24(%bp)
	movw	%si,-26(%bp)
	/* u32 current base -32(%bp) */
	/* u32 counter -36(%bp) */
	subw	$32,%sp
1:
	movl	%eax,-32(%bp)	/* current base address */
	movl	%ecx,-36(%bp)	/* the remaining entries */

	xorl	%edx,%edx
	movl	$SECTOR_SIZE,%ecx
	divl	%ecx		/* %edx:%eax/512 Q=%eax, R=%edx */
				/*  N.B., expecting R=0 due to the alignment */
	addl	(part_lba),%eax	/* LBA of the current base address */
	cmpl	(buf_lba),%eax	/* Check the current buffer */
	je	2f		/* If %eax is not equal to (buf_lba), */
	call	read_to_buf	/*  then read a sector at LBA %eax */
2:
	/* Search kernel file */
	movw	$(BUFFER >> 4),%bx
	movw	%bx,%es
	movw	$(BUFFER & 0xf),%bx
	movw	%bx,%di		/* Base */
	addw	%dx,%di		/* Offset */
	movw	-26(%bp),%si
	movl	$11,%ecx	/* Compare 11 bytes */
	call	memcmp
	je	3f		/* Found then resolve the file information */
	movl	-32(%bp),%eax	/* Calculate the base address */
	addl	$32,%eax	/*  of the next entry */
	movl	-36(%bp),%ecx	/* Get the remaining entries */
	loop	1b		/* decl. %ecx until %ecx == 0 */

	/* Kernel file is not found in the root directory */
	jmp	kload_error	/* Raise an error */
3:
	/* Found the kernel file, then look at the attributes */
	movw	%bx,%di
	addw	%dx,%di
	testb	$0x10,%es:11(%di)	/* Attributes: bit 11 = directory */
	jnz	kload_error	/* Must not directory */
	movw	%es:20(%di),%ax	/* First cluster (hi) */
	shll	$16,%eax
	movw	%es:26(%di),%ax	/* First cluster (lo); movw keeps MSW of %eax */
	movl	%es:28(%di),%ecx	/* File size */

	/* Restore registers */
	movw	-26(%bp),%si
	movw	-24(%bp),%gs
	movw	-22(%bp),%fs
	movw	-20(%bp),%es
	movw	-18(%bp),%ds
	movl	-16(%bp),%edx
	movl	-8(%bp),%ebx
	movw	%bp,%sp
	popw	%bp
	ret

/*
 * Read a file (FAT12)
 *  Arguments
 *   %eax: # of sectors per cluster
 *   %ebx: fat region base address
 *   %esi: first cluster of the file
 *   %ecx: file size (not used...)
 *   %edx: data start bytes
 *   %edi: destination address
 */
read_file_fat12:
	pushw	%bp
	movw	%sp,%bp
	/* Save registers */
	movl	%eax,-4(%bp)
	movl	%ebx,-8(%bp)
	movl	%ecx,-12(%bp)
	movl	%edx,-16(%bp)
	movl	%edi,-20(%bp)
	movl	%esi,-24(%bp)
	movw	%ds,-26(%bp)
	movw	%es,-28(%bp)
	/* u32 current cluster -32(%bp) */
	/* u32 current offset -36(%bp) */
	subw	$36,%sp

	movl	%esi,-32(%bp)	/* First cluster */
	movl	%edi,-36(%bp)	/* Offset */
1:
	xorl	%edx,%edx
	movl	$SECTOR_SIZE,%ecx
	movl	-16(%bp),%eax	/* data_start_bytes */
	divl	%ecx
	movl	%eax,%ecx
	movl	-32(%bp),%eax	/* Current cluster */
	subl	$2,%eax		/* -2 */
	mull	-4(%bp)
	addl	%ecx,%eax
	addl	(part_lba),%eax
	movl	-4(%bp),%ecx	/* # of sectors to read */
	movl	-36(%bp),%edx
	call	read_file

	xorl	%edx,%edx
	movl	-4(%bp),%eax
	movl	$SECTOR_SIZE,%ecx
	mull	%ecx
	addl	-36(%bp),%eax	/* Next offset */
	movl	%eax,-36(%bp)

	movl	-32(%bp),%ecx	/* Current cluster */
	movl	-8(%bp),%eax
	call	fat12_next_cluster
	movl	%ecx,-32(%bp)
	cmpl	$0x0ff8,%ecx
	jl	1b

	/* Restore registers */
	movl	-4(%bp),%eax
	movl	-8(%bp),%ebx
	movl	-12(%bp),%ecx
	movl	-16(%bp),%edx
	movl	-20(%bp),%edi
	movl	-24(%bp),%esi
	movw	-26(%bp),%ds
	movw	-28(%bp),%es
	movw	%bp,%sp
	popw	%bp
	ret


/*
 * Read a file (FAT16)
 *  Arguments
 *   %eax: # of sectors per cluster
 *   %ebx: fat region base address
 *   %esi: first cluster of the kernel
 *   %ecx: kernel size
 *   %edx: data start bytes
 *   %edi: destination address
 */
read_file_fat16:
	pushw	%bp
	movw	%sp,%bp
	/* Save registers */
	movl	%eax,-4(%bp)
	movl	%ebx,-8(%bp)
	movl	%ecx,-12(%bp)
	movl	%edx,-16(%bp)
	movl	%edi,-20(%bp)
	movl	%esi,-24(%bp)
	movw	%ds,-26(%bp)
	movw	%es,-28(%bp)
	/* u32 current cluster -32(%bp) */
	/* u32 current offset -36(%bp) */
	subw	$36,%sp

	movl	%esi,-32(%bp)	/* First cluster */
	movl	%edi,-36(%bp)	/* Offset */
1:
	xorl	%edx,%edx
	movl	$SECTOR_SIZE,%ecx
	movl	-16(%bp),%eax	/* data_start_bytes */
	divl	%ecx
	movl	%eax,%ecx
	movl	-32(%bp),%eax	/* Current cluster */
	subl	$2,%eax		/* -2 */
	mull	-4(%bp)
	addl	%ecx,%eax
	addl	(part_lba),%eax
	movl	-4(%bp),%ecx	/* # of sectors to read */
	movl	-36(%bp),%edx
	call	read_file

	xorl	%edx,%edx
	movl	-4(%bp),%eax
	movl	$SECTOR_SIZE,%ecx
	mull	%ecx
	addl	-36(%bp),%eax	/* Next offset */
	movl	%eax,-36(%bp)

	movl	-32(%bp),%ecx	/* Current cluster */
	movl	-8(%bp),%eax
	call	fat16_next_cluster
	movl	%ecx,-32(%bp)
	cmpl	$0xfff8,%ecx
	jl	1b

	/* Restore registers */
	movl	-4(%bp),%eax
	movl	-8(%bp),%ebx
	movl	-12(%bp),%ecx
	movl	-16(%bp),%edx
	movl	-20(%bp),%edi
	movw	%bp,%sp
	popw	%bp
	ret


/*
 * Get the next cluster (FAT12)
 *  Arguments
 *   %eax: fat region base address
 *   %ecx: cluster number
 *  Return values
 *   %ecx: next cluster
 */
fat12_next_cluster:
	pushw	%bp
	movw	%sp,%bp
	/* Save registers */
	movl	%eax,-4(%bp)
	movl	%ebx,-8(%bp)
	movl	%ecx,-12(%bp)
	movl	%edx,-16(%bp)
	movw	%ds,-18(%bp)
	movw	%es,-20(%bp)
	subw	$20,%sp

	/* Get the position of the next cluster value */
	movl	$3,%eax
	mull	%ecx		/* %edx:%eax = %eax * %ecx */
	shrl	$1,%eax
	addl	-4(%bp),%eax	/* base + cluster * 3 / 2 */

	xorl	%edx,%edx
	movl	$SECTOR_SIZE,%ecx
	divl	%ecx		/* %edx:%eax/512 Q=%eax, R=%edx */
	addl	(part_lba),%eax
	cmpl	(buf_lba),%eax	/* Check the current buffer */
	je	1f		/* If %eax is not equal to (buf_lba), */
	call	read_to_buf	/*  then read a sector at LBA %eax */
1:
	movw	$(BUFFER >> 4),%bx
	movw	%bx,%es
	movw	$(BUFFER & 0xf),%bx
	movw	%dx,%bx
	movw	%es:(%bx),%cx	/* Get two bytes */
	andl	$0xffff,%ecx	/*  (24 bits of them will be retrieved below) */

	testl	$1,-12(%bp)	/* Check the cluster number is odd or even */
	jz	2f	/* Jump if even */
	/* Odd, then use the most significant 24 bits */
	shrl	$4,%ecx
2:
	/* Even, then use the least significant 24 bits */
	andl	$0x0fff,%ecx

	/* Restore registers */
	movw	-20(%bp),%es
	movw	-18(%bp),%ds
	movl	-16(%bp),%edx
	movl	-8(%bp),%ebx
	movl	-4(%bp),%eax
	movw	%bp,%sp
	popw	%bp
	ret


/*
 * Get the next cluster (FAT16)
 *  Arguments
 *   %eax: fat region base address
 *   %ecx: cluster number
 *  Return values
 *   %ecx: next cluster
 */
fat16_next_cluster:
	pushw	%bp
	movw	%sp,%bp
	/* Save registers */
	movl	%eax,-4(%bp)
	movl	%ebx,-8(%bp)
	movl	%ecx,-12(%bp)
	movl	%edx,-16(%bp)
	movw	%ds,-18(%bp)
	movw	%es,-20(%bp)
	subw	$20,%sp

	/* Get the position of the next cluster value */
	shll	$1,%ecx		/* The next cluster is specified by the value */
	addl	%ecx,%eax	/*  at base + cluster * 2 */

	xorl	%edx,%edx
	movl	$SECTOR_SIZE,%ecx
	divl	%ecx		/* %edx:%eax/512 Q=%eax, R=%edx */
	addl	(part_lba),%eax
	cmpl	(buf_lba),%eax	/* Check the current buffer */
	je	1f		/* If %eax is not equal to (buf_lba), */
	call	read_to_buf	/*  then read a sector at LBA %eax */
1:
	movw	$(BUFFER >> 4),%bx
	movw	%bx,%es
	movw	$(BUFFER & 0xf),%bx
	movw	%dx,%bx
	movw	%es:(%bx),%cx
	andl	$0xffff,%ecx

	/* Restore registers */
	movw	-20(%bp),%es
	movw	-18(%bp),%ds
	movl	-16(%bp),%edx
	movl	-8(%bp),%ebx
	movl	-4(%bp),%eax
	movw	%bp,%sp
	popw	%bp
	ret


/*
 * Compare %ds:%si and %es:%di for %ecx length
 *  Arguments
 *   %ds:%si: string 1
 *   %es:%di: string 2
 *   %ecx: the length to be compared
 *  Return values
 *   ZF: set if two strings are identical
 */
memcmp:
1:
	cmpsb
	jne	2f	/* Jump if ZF=0 */
	loop	1b
2:
	ret



/* Display the kernel loader error message */
kload_error:
	movw	$msg_error,%si	/* %ds:(%si) -> error message */
	call	putstr		/* Display error message at %si and then halt */

/* Halt */
halt:
	hlt
	jmp	halt


/*
 * Get the drive parameters of drive %dl and save them to the memory in the
 * .data section.
 *  Return values
 *   CF: set when an error occurs
 */
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


/* Read one sector starting at LBA %eax to the buffer */
read_to_buf:
	pushw	%bx
	pushw	%cx
	pushw	%dx
	pushw	%es
	movw	$1,%cx		/* Read 1 sector */
	movb	(drive),%dl	/*  from the saved drive */
	movw	$(BUFFER >> 4),%bx	/* to the BUFFER memory */
	movw	%bx,%es
	movw	$(BUFFER & 0xf),%bx
	call	read
	movl	%eax,(buf_lba)	/* Save the last buffered sector's LBA */
	popw	%es
	popw	%dx
	popw	%cx
	popw	%bx
	ret

/* Read %cx sector starting at LBA %eax to the kernel location (%edx offset) */
read_file:
	pushl	%ebx
	pushl	%edx
	pushw	%es
	movl	%edx,%ebx
	shrl	$4,%ebx
	movw	%bx,%es		/* The kernel is loaded to */
	movw	%dx,%bx	
	andw	$0xf,%bx	/*  (%edx>>4):[%edx&0xf] */
	movb	(drive),%dl	/*  from the saved drive */
	call	read		/* Read %cx sectors of the kernel */
	popw	%es
	popl	%edx
	popl	%ebx
	ret


/* Read %cx sectors starting at LBA %eax on drive %dl into %es:[%bx] */
read:
	pushw	%bp		/* Save the base pointer*/
	movw	%sp,%bp		/* Copy the stack pointer to the base pointer */
	/* Save general purpose registers */
	movl	%eax,-4(%bp)
	movw	%bx,-6(%bp)
	movw	%cx,-8(%bp)
	movw	%dx,-10(%bp)
	/* Prepare space for local variables */
	/* u16 cx -12(%bp) */
	/* u16 counter -14(%bp) */
	subw	$14,%sp

	/* Reset counter */
	xorw	%ax,%ax
	movw	%ax,-14(%bp)

	/* Set number of sectors to be read */
	movw	%cx,-12(%bp)
1:
	movl	-4(%bp),%eax	/* Restore %eax */
	addw	-14(%bp),%ax	/* Current LBA */
	call	lba2chs		/* Convert LBA (%eax) to CHS (%cx,%dh) */
	call	read_sector	/* Read a sector */
	addw	$SECTOR_SIZE,%bx
	movw	-14(%bp),%ax	/* Get */
	incw	%ax		/*  and increase the current LBA %ax */
	movw	%ax,-14(%bp)	/*  then write back */
	cmpw	-12(%bp),%ax
	jb	1b		/* Need to read more sectors */

	/* Restore the saved registers */
	movw	-10(%bp),%dx
	movw	-8(%bp),%cx
	movw	-6(%bp),%bx
	movl	-4(%bp),%eax
	movw	%bp,%sp		/* Restore the stack pointer and base pointer */
	popw	%bp
	ret


/* Read one sector from CHS (specified by %dh and %cx) specified on drive %dl to
 * %es:[%bx]
 */
read_sector:
	pushw	%bp		/* Save the base pointer */
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


/* Calculate CHS (%cx[7:6]%cx[15:8] ,%dh, %cx[5:0]) from LBA (%eax) */
lba2chs:
	/* Save registers */
	pushl	%eax
	pushw	%bx
	pushl	%edx
	/* Compute sector number */
	xorw	%bx,%bx
	movw	%bx,%cx
	movl	%eax,%edx	/* Prepare for the following divw */
	shrl	$16,%edx	/*  by converting %eax to %dx:%ax */
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
	popl	%edx		/* Restore %dx */
	movb	%bl,%dh		/* Head */
	/* Restore registers */
	popw	%bx
	popl	%eax
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


	.data

/* Messages */
msg_error:
	.asciz	"\r\n\nError occurs in the kernel loader.\r\n"

/* Partition information */
part_lba:
	.long	0		/* Starting at */
part_nsec:
	.long	0		/* # of sectors in the partition */

/* Drive information */
drive:
	.byte	0
heads:
	.byte	0
cylinders:
	.word	0
sectors:
	.byte	0

/* Buffer information */
buf_lba:
	.long	0

/* The file name of the kernel */
fname_kernel:
	.ascii	"KERNEL     "

/* The file name of the initramfs */
fname_initramfs:
	.ascii	"INITRD     "
