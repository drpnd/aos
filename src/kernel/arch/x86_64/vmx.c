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
 * Create a new VMCS
 */
int
vmx_initialize_vmcs(void)
{
    long i;
    int ret;

    void *mem;
    u64 *ept;

    mem = kmalloc(1024 * 1024 * 256);
    if ( NULL == mem ) {
        return -1;
    }
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


    vmx_control_io_bitmap_a_full = 0x00000000;
    vmx_control_io_bitmap_b_full = 0x00000000;
    vmx_control_msr_bitmaps_full = 0x00000000;
    vmx_control_vm_exit_msr_store_full = 0x00000000;
    vmx_control_vm_exit_msr_load_full = 0x00000000;
    vmx_control_vm_entry_msr_load_full = 0x00000000;
    vmx_control_exective_vmcs_pointer_full = 0x00000000;
    vmx_control_tsc_offset_full = 0x00000000;
    vmx_control_ept_pointer_full = 0x18 | (u64)ept;

    vmx_control_pin_based = 0x00000016;
    vmx_control_primary_processor_based = 0x8401e9f2;
    vmx_control_exception_bitmap = 0x00000000;
    vmx_control_page_fault_error_code_mask = 0x00000000;
    vmx_control_page_fault_error_code_match = 0xffffffff;
    vmx_control_cr3_target_count = 0x00000002;
    vmx_control_vm_exit_controls = 0x00036fff;
    vmx_control_vm_exit_msr_store_count = 0x00000000;
    vmx_control_vm_exit_msr_load_count = 0x00000000;
    vmx_control_vm_entry_controls = 0x000011ff;
    vmx_control_vm_entry_msr_load_count = 0x00000000;
    vmx_control_vm_entry_interruption_information_field = 0x00000000;
    vmx_control_vm_entry_exception_error_code = 0x00000000;
    vmx_control_vm_entry_instruction_length = 0x00000000;
    vmx_control_tpr_threshold = 0x00000000;
    vmx_control_secondary_processor_based = 0x00000082;

    vmx_control_cr0_mask = 0x80000021;
    vmx_control_cr4_mask = 0x00002000;
    vmx_control_cr0_read_shadow = 0x80000021;
    vmx_control_cr4_read_shadow = 0x00002000;
    vmx_control_cr3_target_value0 = 0;
    vmx_control_cr3_target_value1 = 0;
    vmx_control_cr3_target_value2 = 0x00000000;
    vmx_control_cr3_target_value3 = 0x00000000;

    vmx_host_es_selector = 0x0000;
    vmx_host_cs_selector = 0;
    vmx_host_ss_selector = 0;
    vmx_host_ds_selector = 0x0000;
    vmx_host_fs_selector = 0x0000;
    vmx_host_gs_selector = 0x0000;
    vmx_host_tr_selector = 0;
    vmx_host_sysenter_cs = 0x00000000;
    vmx_host_cr0 = 0x80000021;
    vmx_host_cr3 = 0;
    vmx_host_cr4 = 0x00002020;
    vmx_host_fs_base = 0x00000000;
    vmx_host_gs_base = 0x00000000;
    vmx_host_tr_base = 0;
    vmx_host_gdtr_base = 0;
    vmx_host_idtr_base = 0;
    vmx_host_sysenter_esp = 0x00000000;
    vmx_host_sysenter_eip = 0x00000000;
    vmx_host_rsp = 0;
    vmx_host_rip = 0;

    vmx_guest_es_selector = 0x0000;
    vmx_guest_cs_selector = 0x0008;
    vmx_guest_ss_selector = 0;
    vmx_guest_ds_selector = 0x0000;
    vmx_guest_fs_selector = 0x0000;
    vmx_guest_gs_selector = 0x0000;
    vmx_guest_ldtr_selector = 0;
    vmx_guest_tr_selector = 0;
    vmx_guest_vmcs_link_pointer_full = 0xffffffff;
    vmx_guest_vmcs_link_pointer_high = 0xffffffff;
    vmx_guest_debugctl_full = 0x00000000;
    vmx_guest_debugctl_high = 0x00000000;
    vmx_guest_es_limit = 0x0000ffff;
    vmx_guest_cs_limit = 0x0000ffff;
    vmx_guest_ss_limit = 0x0000ffff;
    vmx_guest_ds_limit = 0x0000ffff;
    vmx_guest_fs_limit = 0x0000ffff;
    vmx_guest_gs_limit = 0x0000ffff;
    vmx_guest_ldtr_limit = 0;
    vmx_guest_tr_limit = 0;
    vmx_guest_gdtr_limit = 0;
    vmx_guest_idtr_limit = 0;
    vmx_guest_es_access_rights = 0x000000f3;
    vmx_guest_cs_access_rights = 0x000000f3;
    vmx_guest_ss_access_rights = 0x000000f3;
    vmx_guest_ds_access_rights = 0x000000f3;
    vmx_guest_fs_access_rights = 0x000000f3;
    vmx_guest_gs_access_rights = 0x000000f3;
    vmx_guest_ldtr_access_rights = 0x00000082;
    vmx_guest_tr_access_rights = 0x0000008b;
    vmx_guest_interruptibility_state = 0x00000000;
    vmx_guest_activity_state = 0x00000000;
    vmx_guest_smbase = 0x000a0000;
    vmx_guest_sysenter_cs = 0x00000000;

    vmx_guest_cr0 = 0x60000030;
    vmx_guest_cr3 = 0;
    vmx_guest_cr4 = 1 << 13;
    vmx_guest_es_base = 0x00000000;
    vmx_guest_cs_base = 0;
    vmx_guest_ss_base = 0;
    vmx_guest_ds_base = 0x00000000;
    vmx_guest_fs_base = 0x00000000;
    vmx_guest_gs_base = 0x00000000;
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

    for ( i = 0; i < sizeof(vmx_vmcs) / sizeof(struct vmx_vmcs); i++ ) {
        ret = vmwrite(vmx_vmcs[i].index, *(vmx_vmcs[i].ptr));
        if ( ret ) {
            return -1;
        }
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
