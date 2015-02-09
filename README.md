# AOS: Academic Operating System

## About
We are developing an operating system for my personal research
and practical education.
For the academic purpose, this motivation is similar to MINIX,
but we do not focus on theories.
Our main objective is to provide knowledges on hardware-related
programming.  This is one of the most difficult and complex parts
when we start the development of operating system from scratch.

## Developer
Hirochika Asai

## Checkpoints
| Checkpoint   | Tag      | Branch           |
| ------------ | -------- | ---------------- |
| checkpoint 0 | bootmsg  | release/bootmsg  |
| checkpoint 1 | diskload | release/diskload |
| checkpoint 2 | bootmon  | release/bootmon  |

## Checkpoint 0
The BIOS loads the first sector, first 512 bytes (a.k.a. master boot record)
of the image (aos.img),
which stores the initial program loader (src/boot/arch/x86_64/diskboot.s),
to 0x7c00.
Then, it jumps to the address 0x7c00 and executes the program
if the magic number at the last two bytes is valid (0x55aa).
The initial program loader displays a welcome message through BIOS call
using int 10h.

## Checkpoint 1
In this checkpoint, the initial program loader loads
another program (src/boot/arch/x86_64/bootmon.s) stored
in the second sector of the image (aos.img) to 0x9000
using int 13h BIOS call.
It then jumps to that address using far jump call
to execute the program that displays a welcome message.


