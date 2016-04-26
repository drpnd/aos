#
# Copyright (c) 2016 Hirochika Asai
# All rights reserved.
#
# Authors:
#      Hirochika Asai  <asai@jar.jp>
#

i=1
## Save the number of arguments
argc=$#

## Output file
outfile="initramfs"

## Source directory (working directory)
srcdir="./src"

## Reset the file entries
rm -f $outfile
dd if=/dev/zero of=$outfile seek=0 bs=1 count=4096 conv=notrunc > /dev/null 2>&1

## Process each argument
entry=0
while [ $i -le $argc ];
do
    ## Parse the argument
    arg=$1
    target=`echo $arg | cut -d : -f 1`
    fname=`echo $arg | cut -d : -f 2`
    ## Check the size of initramfs
    offset=`stat -f "%z" $outfile`

    ## Write the filename
    printf "$fname\000" | dd of=$outfile seek=`expr $entry \* 32` bs=1 conv=notrunc > /dev/null 2>&1
    ## Write the offset to the file (little endian)
    printf "0: %.16x" $offset | sed -E 's/0: (..)(..)(..)(..)(..)(..)(..)(..)/0: \8\7\6\5\4\3\2\1/' | xxd -r | dd of=$outfile bs=1 seek=`expr $entry \* 32 + 16` conv=notrunc > /dev/null 2>&1
    ## Write the size of the file (little endian)
    printf "0: %.16x" `stat -f "%z" $srcdir/$target` | sed -E 's/0: (..)(..)(..)(..)(..)(..)(..)(..)/0: \8\7\6\5\4\3\2\1/'| xxd -r | dd of=$outfile bs=1 seek=`expr $entry \* 32 + 24` conv=notrunc > /dev/null 2>&1
    ## Write the content of the file
    dd if=$srcdir/$target of=$outfile seek=$offset bs=1 conv=notrunc > /dev/null 2>&1

    ## Go to the next file
    shift
    i=`expr $i + 1`
    entry=`expr $entry + 1`
done

