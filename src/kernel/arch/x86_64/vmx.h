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

#ifndef _KERNEL_VMX_H
#define _KERNEL_VMX_H

#include <aos/const.h>
#include <aos/types.h>

struct vmx_vmcs {
    u32 index;
    u64 *ptr;
};

u64 vmx_control_io_bitmap_a_full;
u64 vmx_control_io_bitmap_a_high;
u64 vmx_control_io_bitmap_b_full;
u64 vmx_control_io_bitmap_b_high;
u64 vmx_control_msr_bitmaps_full;
u64 vmx_control_msr_bitmaps_high;
u64 vmx_control_vm_exit_msr_store_full;
u64 vmx_control_vm_exit_msr_store_high;
u64 vmx_control_vm_exit_msr_load_full;
u64 vmx_control_vm_exit_msr_load_high;
u64 vmx_control_vm_entry_msr_load_full;
u64 vmx_control_vm_entry_msr_load_high;
u64 vmx_control_exective_vmcs_pointer_full;
u64 vmx_control_exective_vmcs_pointer_high;
u64 vmx_control_tsc_offset_full;
u64 vmx_control_tsc_offset_high;
u64 vmx_control_ept_pointer_full;
u64 vmx_control_ept_pointer_high;

u64 vmx_control_pin_based;
u64 vmx_control_primary_processor_based;
u64 vmx_control_exception_bitmap;
u64 vmx_control_page_fault_error_code_mask;
u64 vmx_control_page_fault_error_code_match;
u64 vmx_control_cr3_target_count;
u64 vmx_control_vm_exit_controls;
u64 vmx_control_vm_exit_msr_store_count;
u64 vmx_control_vm_exit_msr_load_count;
u64 vmx_control_vm_entry_controls;
u64 vmx_control_vm_entry_msr_load_count;
u64 vmx_control_vm_entry_interruption_information_field;
u64 vmx_control_vm_entry_exception_error_code;
u64 vmx_control_vm_entry_instruction_length;
u64 vmx_control_tpr_threshold;
u64 vmx_control_secondary_processor_based;
/*u64 vmx_control_ple_gap;*/
/*u64 vmx_control_ple_window;*/
u64 vmx_control_cr0_mask;
u64 vmx_control_cr4_mask;
u64 vmx_control_cr0_read_shadow;
u64 vmx_control_cr4_read_shadow;
u64 vmx_control_cr3_target_value0;
u64 vmx_control_cr3_target_value1;
u64 vmx_control_cr3_target_value2;
u64 vmx_control_cr3_target_value3;

u64 vmx_host_es_selector;
u64 vmx_host_cs_selector;
u64 vmx_host_ss_selector;
u64 vmx_host_ds_selector;
u64 vmx_host_fs_selector;
u64 vmx_host_gs_selector;
u64 vmx_host_tr_selector;
u64 vmx_host_efer_full;
u64 vmx_host_sysenter_cs;
u64 vmx_host_cr0;
u64 vmx_host_cr3;
u64 vmx_host_cr4;
u64 vmx_host_fs_base;
u64 vmx_host_gs_base;
u64 vmx_host_tr_base;
u64 vmx_host_gdtr_base;
u64 vmx_host_idtr_base;
u64 vmx_host_sysenter_esp;
u64 vmx_host_sysenter_eip;
u64 vmx_host_rsp;
u64 vmx_host_rip;

u64 vmx_guest_es_selector;
u64 vmx_guest_cs_selector;
u64 vmx_guest_ss_selector;
u64 vmx_guest_ds_selector;
u64 vmx_guest_fs_selector;
u64 vmx_guest_gs_selector;
u64 vmx_guest_ldtr_selector;
u64 vmx_guest_tr_selector;
/*u64 vmx_guest_interrupt_status;*/

u64 vmx_guest_vmcs_link_pointer_full;
u64 vmx_guest_vmcs_link_pointer_high;
u64 vmx_guest_debugctl_full;
u64 vmx_guest_debugctl_high;

u64 vmx_guest_es_limit;
u64 vmx_guest_cs_limit;
u64 vmx_guest_ss_limit;
u64 vmx_guest_ds_limit;
u64 vmx_guest_fs_limit;
u64 vmx_guest_gs_limit;
u64 vmx_guest_ldtr_limit;
u64 vmx_guest_tr_limit;
u64 vmx_guest_gdtr_limit;
u64 vmx_guest_idtr_limit;
u64 vmx_guest_es_access_rights;
u64 vmx_guest_cs_access_rights;
u64 vmx_guest_ss_access_rights;
u64 vmx_guest_ds_access_rights;
u64 vmx_guest_fs_access_rights;
u64 vmx_guest_gs_access_rights;
u64 vmx_guest_ldtr_access_rights;
u64 vmx_guest_tr_access_rights;
u64 vmx_guest_interruptibility_state;
u64 vmx_guest_activity_state;
u64 vmx_guest_smbase;
u64 vmx_guest_sysenter_cs;
/*u64 vmx_preemption_timer_value;*/

u64 vmx_guest_cr0;
u64 vmx_guest_cr3;
u64 vmx_guest_cr4;
u64 vmx_guest_es_base;
u64 vmx_guest_cs_base;
u64 vmx_guest_ss_base;
u64 vmx_guest_ds_base;
u64 vmx_guest_fs_base;
u64 vmx_guest_gs_base;
u64 vmx_guest_ldtr_base;
u64 vmx_guest_tr_base;
u64 vmx_guest_gdtr_base;
u64 vmx_guest_idtr_base;
u64 vmx_guest_dr7;
u64 vmx_guest_rsp;
u64 vmx_guest_rip;
u64 vmx_guest_rflags;
u64 vmx_guest_pending_debug_exceptions;
u64 vmx_guest_sysenter_esp;
u64 vmx_guest_sysenter_eip;

struct vmx_vmcs vmx_vmcs [] = {
    /* Control fields */
    /* 64-bit */
    /*{ 0x2000, &vmx_control_io_bitmap_a_full },*/
    /*{ 0x2001, &vmx_control_io_bitmap_a_high },*/
    /*{ 0x2002, &vmx_control_io_bitmap_b_full },*/
    /*{ 0x2003, &vmx_control_io_bitmap_b_high },*/
    /*{ 0x2004, &vmx_control_msr_bitmaps_full },*/
    /*{ 0x2005, &vmx_control_msr_bitmaps_high },*/
    /*{ 0x2006, &vmx_control_vm_exit_msr_store_full },*/
    /*{ 0x2007, &vmx_control_vm_exit_msr_store_high },*/
    /*{ 0x2008, &vmx_control_vm_exit_msr_load_full },*/
    /*{ 0x2009, &vmx_control_vm_exit_msr_load_high },*/
    /*{ 0x200a, &vmx_control_vm_entry_msr_load_full },*/
    /*{ 0x200b, &vmx_control_vm_entry_msr_load_high },*/
    /*{ 0x200c, &vmx_control_exective_vmcs_pointer_full },*/
    /*{ 0x200d, &vmx_control_exective_vmcs_pointer_high },*/
    /*{ 0x2010, &vmx_control_tsc_offset_full },*/
    /*{ 0x2011, &vmx_control_tsc_offset_high },*/
    { 0x201a, &vmx_control_ept_pointer_full },
    /*{ 0x201b, &vmx_control_ept_pointer_high },*/
    /* 32-bit */
    { 0x4000, &vmx_control_pin_based },
    { 0x4002, &vmx_control_primary_processor_based },
    /*{ 0x4004, &vmx_control_exception_bitmap },*/
    /*{ 0x4006, &vmx_control_page_fault_error_code_mask },*/
    /*{ 0x4008, &vmx_control_page_fault_error_code_match },*/
    /*{ 0x400a, &vmx_control_cr3_target_count },*/
    { 0x400c, &vmx_control_vm_exit_controls },
    /*{ 0x400e, &vmx_control_vm_exit_msr_store_count },*/
    /*{ 0x4010, &vmx_control_vm_exit_msr_load_count },*/
    { 0x4012, &vmx_control_vm_entry_controls },
    /*{ 0x4014, &vmx_control_vm_entry_msr_load_count },*/
    /*{ 0x4016, &vmx_control_vm_entry_interruption_information_field },*/
    /*{ 0x4018, &vmx_control_vm_entry_exception_error_code },*/
    /*{ 0x401a, &vmx_control_vm_entry_instruction_length },*/
    /*{ 0x401c, &vmx_control_tpr_threshold },*/
    { 0x401e, &vmx_control_secondary_processor_based },
    /*{ 0x4020, &vmx_control_ple_gap },*/
    /*{ 0x4022, &vmx_control_ple_window },*/
    /* Natural */
    /*{ 0x6000, &vmx_control_cr0_mask },*/
    /*{ 0x6002, &vmx_control_cr4_mask },*/
    /*{ 0x6004, &vmx_control_cr0_read_shadow },*/
    /*{ 0x6006, &vmx_control_cr4_read_shadow },*/
    /*{ 0x6008, &vmx_control_cr3_target_value0 },*/
    /*{ 0x600a, &vmx_control_cr3_target_value1 },*/
    /*{ 0x600c, &vmx_control_cr3_target_value2 },*/
    /*{ 0x600e, &vmx_control_cr3_target_value3 },*/

    /* Host-state fields */
    /* 16-bit */
    { 0x0c00, &vmx_host_es_selector },
    { 0x0c02, &vmx_host_cs_selector },
    { 0x0c04, &vmx_host_ss_selector },
    { 0x0c06, &vmx_host_ds_selector },
    { 0x0c08, &vmx_host_fs_selector },
    { 0x0c0a, &vmx_host_gs_selector },
    { 0x0c0c, &vmx_host_tr_selector },
    /* 64-bit */
    { 0x2c02, &vmx_host_efer_full },
    /* 32-bit */
    /*{ 0x4c00, &vmx_host_sysenter_cs },*/
    /* Natural */
    { 0x6c00, &vmx_host_cr0 },
    { 0x6c02, &vmx_host_cr3 },
    { 0x6c04, &vmx_host_cr4 },
    /*{ 0x6c06, &vmx_host_fs_base },*/
    /*{ 0x6c08, &vmx_host_gs_base },*/
    /*{ 0x6c0a, &vmx_host_tr_base },*/
    { 0x6c0c, &vmx_host_gdtr_base },
    { 0x6c0e, &vmx_host_idtr_base },
    /*{ 0x6c10, &vmx_host_sysenter_esp },*/
    /*{ 0x6c12, &vmx_host_sysenter_eip },*/
    { 0x6c14, &vmx_host_rsp },
    { 0x6c16, &vmx_host_rip },

    /* Guest-state fields */
    /* 16-bit */
    { 0x0800, &vmx_guest_es_selector },
    { 0x0802, &vmx_guest_cs_selector },
    { 0x0804, &vmx_guest_ss_selector },
    { 0x0806, &vmx_guest_ds_selector },
    { 0x0808, &vmx_guest_fs_selector },
    { 0x080a, &vmx_guest_gs_selector },
    { 0x080c, &vmx_guest_ldtr_selector },
    { 0x080e, &vmx_guest_tr_selector },
    /*{ 0x0810, &vmx_guest_interrupt_status },*/
    /* 64-bit */
    { 0x2800, &vmx_guest_vmcs_link_pointer_full },
    /*{ 0x2801, &vmx_guest_vmcs_link_pointer_high },*/
    /*{ 0x2802, &vmx_guest_debugctl_full },*/
    /*{ 0x2803, &vmx_guest_debugctl_high },*/
    /* 32-bit */
    { 0x4800, &vmx_guest_es_limit },
    { 0x4802, &vmx_guest_cs_limit },
    { 0x4804, &vmx_guest_ss_limit },
    { 0x4806, &vmx_guest_ds_limit },
    { 0x4808, &vmx_guest_fs_limit },
    { 0x480a, &vmx_guest_gs_limit },
    { 0x480c, &vmx_guest_ldtr_limit },
    { 0x480e, &vmx_guest_tr_limit },
    { 0x4810, &vmx_guest_gdtr_limit },
    { 0x4812, &vmx_guest_idtr_limit },
    { 0x4814, &vmx_guest_es_access_rights },
    { 0x4816, &vmx_guest_cs_access_rights },
    { 0x4818, &vmx_guest_ss_access_rights },
    { 0x481a, &vmx_guest_ds_access_rights },
    { 0x481c, &vmx_guest_fs_access_rights },
    { 0x481e, &vmx_guest_gs_access_rights },
    { 0x4820, &vmx_guest_ldtr_access_rights },
    { 0x4822, &vmx_guest_tr_access_rights },
    /*{ 0x4824, &vmx_guest_interruptibility_state },*/
    /*{ 0x4826, &vmx_guest_activity_state },*/
    /*{ 0x4828, &vmx_guest_smbase },*/
    /*{ 0x482a, &vmx_guest_sysenter_cs },*/
    /*{ 0x482e, &vmx_preemption_timer_value },*/
    /* Natural */
    { 0x6800, &vmx_guest_cr0 },
    { 0x6802, &vmx_guest_cr3 },
    { 0x6804, &vmx_guest_cr4 },
    { 0x6806, &vmx_guest_es_base },
    { 0x6808, &vmx_guest_cs_base },
    { 0x680a, &vmx_guest_ss_base },
    { 0x680c, &vmx_guest_ds_base },
    { 0x680e, &vmx_guest_fs_base },
    { 0x6810, &vmx_guest_gs_base },
    { 0x6812, &vmx_guest_ldtr_base },
    { 0x6814, &vmx_guest_tr_base },
    { 0x6816, &vmx_guest_gdtr_base },
    { 0x6818, &vmx_guest_idtr_base },
    { 0x681a, &vmx_guest_dr7 },
    { 0x681c, &vmx_guest_rsp },
    { 0x681e, &vmx_guest_rip },
    { 0x6820, &vmx_guest_rflags },
    { 0x6822, &vmx_guest_pending_debug_exceptions },
    { 0x6824, &vmx_guest_sysenter_esp },
    { 0x6826, &vmx_guest_sysenter_eip },
};

#endif /* _KERNEL_VMX_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
