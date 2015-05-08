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

#include <aos/const.h>
#include <sys/syscall.h>
#include "../../kernel.h"
#include "arch.h"
#include "memory.h"

#define IA32_VMX_BASIC 0x480
#define IA32_VMX_CR0_FIXED0 0x486
#define IA32_VMX_CR0_FIXED1 0x487
#define IA32_VMX_CR4_FIXED0 0x488
#define IA32_VMX_CR4_FIXED1 0x489

/*
 * Enable VMX
 */
int
vmx_enable(void)
{
    u64 rcx;
    u64 rdx;
    u64 vmx;
    u64 cr;
    u64 fixed0;
    u64 fixed1;
    u32 *vmcs;
    int ret;

    /* Check the VMX support */
    cpuid(1, &rcx, &rdx);
    if ( !(rcx & (1 << 5)) ) {
        /* VMX is not supported */
        return -1;
    }

    /* Get */
    vmx = rdmsr(IA32_VMX_BASIC);

    /* Set up CR0 */
    fixed0 = rdmsr(IA32_VMX_CR0_FIXED0);
    fixed1 = rdmsr(IA32_VMX_CR0_FIXED1);
    cr = get_cr0();
    set_cr0((cr | fixed0) & fixed1);

    /* Setup CR4 */
    fixed0 = rdmsr(IA32_VMX_CR4_FIXED0);
    fixed1 = rdmsr(IA32_VMX_CR4_FIXED1);
    cr = get_cr4();
    set_cr4((cr | fixed0) & fixed1);

    vmcs = kmalloc(4096);
    kmemset(vmcs, 0, 4096);
    vmcs[0] = vmx;

    ret = vmxon(&vmcs);
    if ( ret ) {
        return -1;
    }

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
