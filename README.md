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
| checkpoint 2 | msecload | release/msecload |
| checkpoint 3 | apmoff   | release/apmoff   |
| checkpoint 4 | pickbd   | release/pickbd   |
| checkpoint 5 | pit      | release/pit      |
| checkpoint 6 | bootmon  | release/bootmon  |
| checkpoint 7 | kernload | release/kernload  |

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

## Checkpoint 2
This checkpoint is a small extension from the checkpoint 1 although
many lines of code are added.
This includes two main updates from the checkpoint 1:
1) This supports to read multiple sectors for the other program.
2) This displays the error code in case of disk read failure.

## Checkpoint 3
This checkpoint implements a function to power off the machine
using Advanced Power Management (APM) API.

## Checkpoint 4
Interrupts from peripheral devices (IRQ: Interrupt Request)
are routed to a processor
by a Programmable Interrupt Controller (PIC), Intel 8259(A).
To handle an interrupt from the keyboard,
the corresponding interrupt handler that reads keyboard input
from the keyboard controller
is setup in the Interrupt Vector Table (IVT) in this checkpoint.

## Checkpoint 5
Programmable Interval Timer (PIT), Intel 8253/8254
implements a crystal oscillator
and generates a periodic or interval signal output.
Usually, the signal output is handled by CPU as an interrupt through PIC.
Here, PIT is used as timer.

## Checkpoint 6
Boot monitor provides an entry point to the operating system.
In this checkpoint, the boot monitor waits user's interaction
to choose the boot option.
After the boot option is specified,
the software tries to enter 32/64-bit mode unless the option is power off.

## Checkpoint 7
In the previous checkpoints, the software use BIOS calls to load data
from a disk drive using INT 13h.
In this checkpoint, we try to fly away from the BIOS, and implement
floppy disk and AHCI (for SATA disks etc) driver
to load data from these disks.
