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

#include "boot.h"

/* Registers */
#define FLOPPY_STATUS_A         0x3f0
#define FLOPPY_STATUS_B         0x3f1
#define FLOPPY_DOR              0x3f2   /* Digital output register */
#define FLOPPY_MSR              0x3f4   /* Main status register */
#define FLOPPY_DATA_FIFO        0x3f5
#define FLOPPY_CCR              0x3f7   /* Configuration control register */
/* DORs */
#define DOR_DSEL                0x03
#define DOR_RESET               0x04
#define DOR_IRQ                 0x08
#define DOR_MOTA                0x10
#define DOR_MOTB                0x20
#define DOR_MOTC                0x40
#define DOR_MOTD                0x80
/* Commands */
#define COMMAND_SPECIFY         3
#define COMMAND_READ_DATA       6
#define COMMAND_RECALIBRATE     7
#define COMMAND_SENSE_INTERRUPT 8
#define COMMAND_SEEK            0xf

#define COMMAND_FLAG_MT         0x80
#define COMMAND_FLAG_MFM        0x40

/* MSR */
#define MSR_RQM                 0x80    /* Access permitted */

#define SECTOR_DTL_512          2       /* 512 bytes per sector */
#define GPL1                    0x1b    /* 3.5 inch 512 byte sector w/ MFM */

/* 20 times are sufficient:
   http://wiki.osdev.org/Floppy_Disk_Controller#Bit_MT */
#define TIMEOUT_LOOP            32

#define FLOPPY_DMA_ADDRESS      0x1000

/*
 * Read the main status register
 */
static u8
_read_msr(void)
{
    return inb(FLOPPY_MSR);
}

/*
 * Write a value to the digital output register
 */
static void
_write_dor(int val)
{
    outb(FLOPPY_DOR, val);
}

/*
 * Write a value to the configuration control register
 */
static void
_write_ccr(int val)
{
    outb(FLOPPY_CCR, val);
}

static int
_send_command(u8 cmd)
{
    int i;

    /* Wait until the host is ready to transfer data  */
    for ( i = 0; i < TIMEOUT_LOOP; i++ ) {
        if ( _read_msr() & MSR_RQM ) {
            /* Write the command to the data FIFO */
            outb(FLOPPY_DATA_FIFO, cmd);
            return 0;
        }
    }

    return -1;
}

static int
_read_data(void)
{
    int i;

    /* Wait until the host is ready to transfer data  */
    for ( i = 0; i < TIMEOUT_LOOP; i++ ) {
        if ( _read_msr() & MSR_RQM ) {
            /* Write the command to the data FIFO */
            return inb(FLOPPY_DATA_FIFO);
        }
    }

    return -1;
}

static void
_control_motor(int motor)
{
    if ( motor ) {
        /* Turn on */
        _write_dor(DOR_RESET | DOR_IRQ | DOR_MOTA | DOR_MOTB | DOR_MOTC
                   | DOR_MOTD);
    } else {
        /* Turn off */
        _write_dor(DOR_RESET | DOR_IRQ);
    }
}

static void
_sense_interrupt(int *st0, int *cyl)
{
    /* Sense interrupt */
    _send_command(COMMAND_SENSE_INTERRUPT);

    /* Read st0 and the current cylinder */
    *st0 = _read_data();
    *cyl = _read_data();
}

static void
_enable_controller(void)
{
    _write_dor(DOR_RESET | DOR_IRQ);
}

static void
_disable_controller(void)
{
    _write_dor(0);
}

static void
_wait_irq(void)
{
    /* FIXME */
}

static void
_init_dma(u16 dma)
{
    /* Mask DMA channel 2 */
    outb(0x0a, 0x06);
    /* Reset master flip flop */
    outb(0x0c, 0xff);
    /* Set the DMA address to 0x1000 */
    outb(0x04, dma & 0xff);        /* LSB */
    outb(0x04, (dma >> 8) & 0xff); /* MSB */
    /* Reset master flip flop */
    outb(0x0c, 0xff);
    /* Set the count to 0x23ff */
    outb(0x05, 0xff);
    outb(0x05, 0x23);
    /* Set the external page register to 0 */
    outb(0x81, 0x00);
    /* Unmask DMA channel 2 */
    outb(0x0a, 0x02);
}

static void
_prepare_for_read(void)
{
    /* Mask DMA channel 2 */
    outb(0x0a, 0x06);
    /* Read mode */
    outb(0x0b, 0x56);
    /* Unmask DMA channel 2 */
    outb(0x0a, 0x02);
}

static void
_prepare_for_write(void)
{
    /* Mask DMA channel 2 */
    outb(0x0a, 0x06);
    /* Write mode */
    outb(0x0b, 0x5a);
    /* Unmask DMA channel 2 */
    outb(0x0a, 0x02);
}

/*
 * Initialize the floppy controller
 */
void
floppy_init(void)
{
    /* Reset */
    floppy_reset();

    /* Setup DMA */
    _init_dma(FLOPPY_DMA_ADDRESS);

    /* Setup IRQ handler */
}

/*
 * Reset the controller
 */
void
floppy_reset(void)
{
    int i;
    int st0;
    int cyl;
    u8 srt_hut;
    u8 hlt_nd;

    /* Reset the controller */
    _disable_controller();
    _enable_controller();

    /* Four sense interrupt commands needed after a reset (see Intel 82077AA
       datasheet for the details) */
    for ( i = 0; i < 4; i++ ) {
        _sense_interrupt(&st0, &cyl);
    }

    /* Set transfer speed to 500kb/s (for 1.44M floppy) */
    _write_ccr(0x00);

    /* Specify the SRT (Step Rate Time), HUT (Head Unload Time), HLT (Head Load
       Time), and non-DMA mode flag */
    srt_hut = (0xd << 4) | 0xf; /* SRT = 3ms, HUT = 240ms */
    hlt_nd = (8 << 1) | 0;      /* HLT = 16ms, ND=0 (DMA mode) */
    _send_command(COMMAND_SPECIFY);
    _send_command(srt_hut);
    _send_command(hlt_nd);
}

/*
 * Recalibrate a drive
 */
int
floppy_recalibrate(int drive)
{
    int st0;
    int cyl;

    if ( drive >= 4 || drive < 0 ) {
        /* Invalid drive number */
        return -1;
    }

    /* Turn on the motor */
    _control_motor(1);

    /* Send the RECALIBRATE command */
    _send_command(COMMAND_RECALIBRATE);
    /* Drive number */
    _send_command(drive);
    /* Sense the interrupt */
    _sense_interrupt(&st0, &cyl);

    /* Turn off the motor */
    _control_motor(0);

    if ( 0 == cyl ) {
        /* Succeeded */
        return 0;
    } else {
        /* Failed to re-calibrate */
        return -1;
    }
}

/*
 * Seek
 */
static int
_seek(int drive, int cyl, int head)
{
    int st0;
    int rcyl;

    if ( drive < 0 || drive >= 4 ) {
        /* Invalid drive */
        return -1;
    }

    /* Sent the command to seek */
    _send_command(COMMAND_SEEK);
    _send_command((head << 2) | drive);
    _send_command(cyl);

    /* Wait until receive an interrupt */
    _wait_irq();
    _sense_interrupt(&st0, &rcyl);

    /* Check the returned cylinder */
    if ( rcyl == cyl ) {
        /* Yes */
        return 0;
    } else {
        /* Failed */
        return -1;
    }
}

/*
 * Read
 */
int
floppy_read(int drive, int cyl, int head, int sector, int nr)
{
    int st0;
    int st1;
    int st2;
    int rcyl;
    int rhead;
    int rsector;
    int rn;
    int ret;

    if ( drive < 0 || drive >= 4 ) {
        /* Invalid drive */
        return -1;
    }

    _control_motor(1);

    /* Seek */
    ret = _seek(drive, cyl, head);
    if ( ret < 0 ) {
        /* Failed */
        _control_motor(0);
        return -1;
    }

    /* Set the DMA mode to read */
    _prepare_for_read();

    _send_command(COMMAND_READ_DATA | COMMAND_FLAG_MT | COMMAND_FLAG_MFM);
    _send_command((head << 2) | drive);
    _send_command(cyl);
    _send_command(head);
    _send_command(sector);
    _send_command(SECTOR_DTL_512);
    _send_command(nr);
    _send_command(GPL1);
    _send_command(0xff);        /* DTL = 0xff for SECTOR_DTL_512 */

    /* Wait until an interrupt is received */
    _wait_irq();

    st0 = _read_data();
    st1 = _read_data();
    st2 = _read_data();
    rcyl = _read_data();
    rhead = _read_data();
    rsector = _read_data();
    rn = _read_data();          /* Must be same as SECTOR_DTL_512 */

    _control_motor(0);

    return 0;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
