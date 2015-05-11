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
#include "vmx.h"

#define IA32_VMX_BASIC 0x480
#define IA32_VMX_CR0_FIXED0 0x486
#define IA32_VMX_CR0_FIXED1 0x487
#define IA32_VMX_CR4_FIXED0 0x488
#define IA32_VMX_CR4_FIXED1 0x489


#if 0
/* Code example */
{
    /* VMXON: See Vol. 3C, 31.5 */
    if ( vmx_enable() ) {
        panic("Failed on vmxon");
    }

    if ( vmx_initialize_vmcs() ) {
        panic("Failed on initialization");
    }

    if ( vmlaunch() ) {
        panic("Failed on vmlaunch");
    }
}
#endif

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


void
vmx_vm_exit_handler(void)
{
    u64 rd;

    /* VM exit reason */
    rd = vmread(0x4402);

    rd = rd & 0xff;
    if ( 1 == rd ) {
        sti();
        vmresume();
    } else if ( 12 == rd ) {
        sti();
        halt();
        vmresume();
    } else if ( 52 == rd ) {
        panic("VM exit (preemption expired)");
    } else {
        panic("VM exit");
    }
}

/*
 * Create a new VMCS
 */
int
vmx_initialize_vmcs(void)
{
    long i;
    int ret;

    u32 *vmcs;
    u64 vmx;

    u8 *mem;
    u64 *ept;

    struct {
        u16 limit;
        u64 base;
    } __attribute__ ((packed)) desc;

    /* New VMCS */
    vmcs = kmalloc(4096);
    vmx = rdmsr(IA32_VMX_BASIC);
    vmcs[0] = vmx;
    if ( vmclear(&vmcs) ) {
        kfree(vmcs);
        return -1;
    }
    if ( vmptrld(&vmcs) ) {
        kfree(vmcs);
        return -1;
    }


    mem = kmalloc(1024 * 1024 * 256);
    if ( NULL == mem ) {
        return -1;
    }
    mem[0x7c00] = 0xf4;
    mem[0x7c01] = 0xeb;
    mem[0x7c02] = 0xfd;
    ept = kmalloc(4096 * 3);
    if ( NULL == ept ) {
        kfree(mem);
        return -1;
    }
    kmemset(ept, 0, 4096 * 3);
    ept[0] = 0x07 | (u64)&ept[512];
    ept[512] = 0x07 | (u64)&ept[1024];
    for ( i = 0; i < 128; i++ ) {
        ept[1024 + i] = 0x87 | ((u64)mem + i * 1024 * 1024 * 2);
    }


    vmx_control_ept_pointer_full = 0x18 | (u64)ept;

    vmx_control_pin_based = 0x0000001f;
    vmx_control_primary_processor_based = 0x8401e9f2;
    vmx_control_vm_exit_controls = 0x00036fff;
    vmx_control_vm_entry_controls = 0x000011ff;
    vmx_control_secondary_processor_based = 0x00000082;

    __asm__ __volatile__ ( "movq %%es,%%rax" : "=a"(vmx_host_es_selector) );
    __asm__ __volatile__ ( "movq %%cs,%%rax" : "=a"(vmx_host_cs_selector) );
    __asm__ __volatile__ ( "movq %%ss,%%rax" : "=a"(vmx_host_ss_selector) );
    __asm__ __volatile__ ( "movq %%ds,%%rax" : "=a"(vmx_host_ds_selector) );
    __asm__ __volatile__ ( "movq %%fs,%%rax" : "=a"(vmx_host_fs_selector) );
    __asm__ __volatile__ ( "movq %%gs,%%rax" : "=a"(vmx_host_gs_selector) );
    __asm__ __volatile__ ( "xorq %%rax,%%rax; str %%ax"
                           : "=a"(vmx_host_tr_selector) );
    vmx_host_efer_full = rdmsr(0x0c0000080); /* EFER MSR */
    vmx_host_cr0 = get_cr0();
    vmx_host_cr3 = (u64)get_cr3();
    vmx_host_cr4 = get_cr4();
    sgdt(&desc);
    vmx_host_gdtr_base = desc.base;
    sidt(&desc);
    vmx_host_idtr_base = desc.base;
    vmx_host_rsp = (u64)kmalloc(4096);
    vmx_host_rip = (u64)vmx_vm_exit_handler;

    vmx_guest_es_selector = 0x0;
    vmx_guest_cs_selector = 0x0;
    vmx_guest_ss_selector = 0x0;
    vmx_guest_ds_selector = 0x0;
    vmx_guest_fs_selector = 0x0;
    vmx_guest_gs_selector = 0x0;
    vmx_guest_es_limit = 0x0000ffff;
    vmx_guest_cs_limit = 0x0000ffff;
    vmx_guest_ss_limit = 0x0000ffff;
    vmx_guest_ds_limit = 0x0000ffff;
    vmx_guest_fs_limit = 0x0000ffff;
    vmx_guest_gs_limit = 0x0000ffff;
    vmx_guest_tr_limit = 0x000000ff;
    vmx_guest_ldtr_limit = 0xffffffff;
    vmx_guest_gdtr_limit = 0x0000ffff;
    vmx_guest_idtr_limit = 0x0000ffff;
    vmx_guest_es_access_rights = 0x00000093;
    vmx_guest_cs_access_rights = 0x0000009b;
    vmx_guest_ss_access_rights = 0x00000093;
    vmx_guest_ds_access_rights = 0x00000093;
    vmx_guest_fs_access_rights = 0x00000093;
    vmx_guest_gs_access_rights = 0x00000093;
    vmx_guest_ldtr_access_rights = 0x00010000;
    vmx_guest_tr_access_rights = 0x0000008b;

    vmx_guest_cr0 = 0x60000030;
    vmx_guest_cr3 = 0;
    vmx_guest_cr4 = 1 << 13;
    vmx_guest_es_base = 0;
    vmx_guest_cs_base = 0;
    vmx_guest_ss_base = 0;
    vmx_guest_ds_base = 0;
    vmx_guest_fs_base = 0;
    vmx_guest_gs_base = 0;
    vmx_guest_ldtr_base = 0;
    vmx_guest_tr_base = 0;
    vmx_guest_gdtr_base = 0;
    vmx_guest_idtr_base = 0;
    vmx_guest_dr7 = 0x00000400;
    vmx_guest_rsp = 0;
    vmx_guest_rip = 0x7c00;
    vmx_guest_rflags = 2;
    vmx_guest_pending_debug_exceptions = 0x00000000;
    vmx_guest_sysenter_esp = 0x00000000;
    vmx_guest_sysenter_eip = 0x00000000;

    vmx_guest_vmcs_link_pointer_full = 0xffffffffffffffffULL;

    for ( i = 0; i < sizeof(vmx_vmcs) / sizeof(struct vmx_vmcs); i++ ) {
        ret = vmwrite(vmx_vmcs[i].index, *(vmx_vmcs[i].ptr));
        if ( ret ) {
            return -1;
        }
    }

    return 0;
}

/*
 * Get the error code of the previous instruction
 */
u64
vmx_get_error(void)
{
    return vmread(0x4400);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
