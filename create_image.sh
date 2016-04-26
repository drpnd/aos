#
# Copyright (c) 2016 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

## This script takes four arguments.  The options are described below in the error
## message
if [ -z "$1" -o -z "$2" -o -z "$3" -o -z "$4" ];
then
    echo "Usage: $0 <boot-loader-stage1> <boot-loader-stage2> <kernel> <initrd>" >& 2
    exit -1
fi

## Arguments to named variables
bootloader1=$1
bootloader2=$2
kernel=$3
initrd=$4
outfile="aos.img"

## Resolve the size of each file
bootloader1_size=`stat -f "%z" $bootloader1`
bootloader2_size=`stat -f "%z" $bootloader2`
kernel_size=`stat -f "%z" $kernel`
initrd_size=`stat -f "%z" $initrd`

# Check the file size first
if [ $bootloader1_size -ge 446 ];
then
    echo "Error: $bootloader1 is too large (must be <= 446 bytes)" >& 2
    exit 1
fi
if [ $bootloader2_size -ge 28672 ];
then
    echo "Error: $bootloader2 is too large (must be <= 28672 bytes)" >& 2
    exit 1
fi
if [ $kernel_size -ge 131072 ];
then
    echo "Error: $kernel is too large (must be <= 131072 bytes)" >& 2
    exit 1
fi
if [ $initrd_size -ge 262144 ];
then
    echo "Error: $initrd is too large (must be <= 262144 bytes)" >& 2
    exit 1
fi


## Create an image file
cp src/diskboot $outfile

## Write partition table (#1: start: cyl=0, hd=2, sec=3)
## N.B., # of cyl, hd, and sec in the entry are different from drives
printf '\200\002\003\000\013\055\055\000\200\000\000\000\300\012' \
    | dd of=$outfile bs=1 seek=446 conv=notrunc > /dev/null 2>&1

## Write the magic number
printf '\125\252' | dd of=$outfile bs=1 seek=510 conv=notrunc > /dev/null 2>&1

## Write bootmon
dd if=src/bootmon of=$outfile bs=1 seek=512 conv=notrunc > /dev/null 2>&1

## Write initramfs (FAT12)
printf '\353\076\220AOS  1.0\000\002\010\001\000\002\000\002\300\012\370\010\000\040\000\040\000\000\010\000\000\000\000\000\000\200\000\051\000\000\000\000NO NAME    FAT12   ' \
    | dd of=$outfile bs=1 seek=65536 conv=notrunc > /dev/null 2>&1
printf '\364\353\375' \
    | dd of=$outfile bs=1 seek=65600 conv=notrunc > /dev/null 2>&1 # 1: hlt; jmp 1b
printf '\125\252' | dd of=$outfile bs=1 seek=66046 conv=notrunc > /dev/null 2>&1
printf '\370\377\377' \
    | dd of=$outfile bs=1 seek=66048 conv=notrunc > /dev/null 2>&1


s=2
c=66051
## Calculate the number of clusters for kernel/initramfs
kcls=`expr \( $kernel_size + 4095 \) / 4096`
icls=`expr \( ${initrd_size} + 4095 \) / 4096`


if [ $kcls -gt 0 ];
then
    i=0
    while [ $i -lt $kcls ];
    do
	if [ `expr $i + 1` -eq $kcls ];
	then
	    b=4095;
	elif [ `expr $i + 2` -eq $kcls ];
	then
	    b=`expr \( $s + 1 \) + 4095 '*' 4096`
	else
	    b=`expr \( $s + 1 \) + \( $s + 2 \) '*' 4096`
	fi
	printf "0: %.6x" $b | sed -E 's/0: (..)(..)(..)/0: \3\2\1/'| xxd -r \
	    | dd of=$outfile bs=1 seek=$c conv=notrunc > /dev/null 2>&1
	i=`expr $i + 2`
	s=`expr $s + 2`
	c=`expr $c + 3`
    done
fi

if [ $icls -gt 0 ];
then
    i=0
    while [ $i -lt $icls ];
    do
	if [ `expr $i + 1` -eq $icls ];
	then
 	    b=4095
 	elif [ `expr $i + 2` -eq $icls ];
	then
 	    b=`expr \( $s + 1 \) + 4095 '*' 4096`
	else
 	    b=`expr \( $s + 1 \) + \( $s + 2 \) '*' 4096`
 	fi
 	printf "0: %.6x" $b | sed -E 's/0: (..)(..)(..)/0: \3\2\1/'| xxd -r \
	    | dd of=$outfile bs=1 seek=$c conv=notrunc > /dev/null 2>&1
 	i=`expr $i + 2`
	s=`expr $s + 2`
	c=`expr $c + 3`
    done
fi



printf '\370\377\377' | dd of=$outfile bs=1 seek=70144 conv=notrunc > /dev/null 2>&1

s=2
c=70147
if [ $kcls -gt 0 ];
then
    i=0
    while [ $i -lt $kcls ];
    do
	if [ `expr $i + 1` -eq $kcls ];
	then
	    b=4095;
	elif [ `expr $i + 2` -eq $kcls ];
	then
	    b=`expr \( $s + 1 \) + 4095 '*' 4096`
	else
	    b=`expr \( $s + 1 \) + \( $s + 2 \) '*' 4096`
	fi
	printf "0: %.6x" $b | sed -E 's/0: (..)(..)(..)/0: \3\2\1/'| xxd -r \
	    | dd of=$outfile bs=1 seek=$c conv=notrunc > /dev/null 2>&1
	i=`expr $i + 2`
	s=`expr $s + 2`
	c=`expr $c + 3`
    done
fi

if [ $icls -gt 0 ];
then
    i=0
    while [ $i -lt $icls ];
    do
	if [ `expr $i + 1` -eq $icls ];
	then
 	    b=4095
 	elif [ `expr $i + 2` -eq $icls ];
	then
 	    b=`expr \( $s + 1 \) + 4095 '*' 4096`
	else
 	    b=`expr \( $s + 1 \) + \( $s + 2 \) '*' 4096`
 	fi
 	printf "0: %.6x" $b | sed -E 's/0: (..)(..)(..)/0: \3\2\1/'| xxd -r \
	    | dd of=$outfile bs=1 seek=$c conv=notrunc > /dev/null 2>&1
 	i=`expr $i + 2`
	s=`expr $s + 2`
	c=`expr $c + 3`
    done
fi

# Write root directory
printf 'NO NAME    \010\000\000\000\000\000\000\000\000\000\000\000\000\000\000' \
    | dd of=$outfile bs=1 seek=74240 conv=notrunc > /dev/null 2>&1
# Write kernel
if [ $kcls -eq 0 ];
then
    printf 'KERNEL     \001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000' | dd of=$outfile bs=1 seek=74272 conv=notrunc > /dev/null 2>&1
else
    printf 'KERNEL     \001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\002\000' | dd of=$outfile bs=1 seek=74272 conv=notrunc > /dev/null 2>&1
fi

printf "0: %.8x" ${kernel_size} | sed -E 's/0: (..)(..)(..)(..)/0: \4\3\2\1/' \
    | xxd -r | dd of=$outfile bs=1 seek=74300 conv=notrunc > /dev/null 2>&1

# Write initramfs
if [ ${icls} -eq 0 ];
then
    printf 'INITRD     \001\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000' | dd of=$outfile bs=1 seek=74304 conv=notrunc > /dev/null 2>&1
else
    printf 'INITRD     \001\000\000\000\000\000\000\000\000\000\000\000\000\000\000' | dd of=$outfile bs=1 seek=74304 conv=notrunc > /dev/null 2>&1
    printf "0: %.4x" `expr 2 + \( ${kcls} + 1 \) / 2 \* 2` \
	| sed -E 's/0: (..)(..)/0: \2\1/'| xxd -r \
	| dd of=$outfile bs=1 seek=74330 conv=notrunc > /dev/null 2>&1
fi
printf "0: %.8x" ${initrd_size} | sed -E 's/0: (..)(..)(..)(..)/0: \4\3\2\1/' \
    | xxd -r | dd of=$outfile bs=1 seek=74332 conv=notrunc > /dev/null 2>&1

# Write kernel (contents)
dd if=$kernel of=$outfile bs=1 seek=90624 conv=notrunc > /dev/null 2>&1
# Write initramfs (contents)
pos=`expr 90624 + \( ${kcls} + 1 \) / 2 \* 2 \* 4096`
dd if=$initrd of=$outfile bs=1 seek=$pos conv=notrunc > /dev/null 2>&1

# Use truncate if your system supports: i.e., truncate aos.img 1474560
printf '\000' | dd of=$outfile bs=1 seek=1474559 conv=notrunc > /dev/null 2>&1
