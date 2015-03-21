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
| checkpoint 7 | kernload | release/kernload |
| checkpoint 8 | memmap   | release/memmap   |
| checkpoint 9 | bspinit  | release/bspinit  |
| checkpoint a | bspinit  | release/apinit   |
| checkpoint b | lapictmr | release/lapictmr |

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
The boot monitor loads kernel from a filesystem (FAT12/FAT16)
in the first partition of the disk.
Along with your design, this part will be related with your own file system
in the future, and consequently, FAT32 is not supported
to reduce the code complexity here.
This procedure is complemeted using BIOS calls, INT 13h.

## Checkpoint 8
We do a few more things in the real mode before transitioning to the kernel.
Some memory spaces are used by interrupt vector table, BIOS data, video RAM etc.,
and cannot be used by the kernel.
In this checkpoint, we load the memory map using a BIOS call, INT 15h,
that tells the kernel's memory manager where to be used.

## Checkpoint 9
Now we have got into the 64-bit long mode kernel in C.
As the first step of the kernel, we initialize the current processor,
bootstrap processor (BSP); GDT, IDT, ACPI, and APIC.
We configure the global descriptor table (GDT)
and interrupt descriptor table (IDT).
GDT was already once configured just before entering 32 bit mode,
but they are temporary ones and they did not support ring protection of CPUs.
In this checkpoint, we prepare four sets of GDT entries for ring 0 to 3.
ACPI is (first) used to implement a busy wait timer
using ACPI Power Management (PM) Timer.
Note that we implement ACPI parser by ourselves instead of using
the ACPICA (ACPI Component Architecture) here
because the ACPICA requires memory allocator etc
which we have not implemented yet.
We also note that any other timers such as time stamp counter (TSC)
and high precision event timer (HPET) may be used instead of ACPI PM timer
if they are available.

## Checkpoint a
Multicore/multiprocessor system becomes more common in these days.
In this checkpoint, we try to boot all cores/processors
from the bootstrap processor (BSP).
The processors booted from the BSP are called application processors (APs).
The simplest procedure to initialize the APs is the following:
1) The BSP load the program, so-called trampoline code,
into 4 KiB (aligned) page in the lower 1 MiB of memory.
2) The BSP broadcasts INIT IPI (inter-processor interrupt) to initialize
the APs, then waits 10 ms.
3) The BSP broadcasts the first SIPI (start-up IPI), then wait 200 us.
4) The BSP broadcasts the second SIPI (start-up IPI), then wait 200 us.
This procedure is called INIT-SIPI-SIPI IPI sequence
(See Section 8.4.4.1 of Intel SDM).

## Checkpoint b
Towards the multitasking operating system, we setup the local APIC timer
to generate periodic timer interrupts that will be used for the context switch.
