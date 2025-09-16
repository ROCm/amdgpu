/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KCL_KCL_MEMORY_H
#define _KCL_KCL_MEMORY_H

#ifndef HAVE_VMF_INSERT_PFN_PROT
vm_fault_t _kcl_vmf_insert_pfn_prot(struct vm_area_struct *vma, unsigned long addr,
					unsigned long pfn, pgprot_t pgprot);
static inline
vm_fault_t vmf_insert_pfn_prot(struct vm_area_struct *vma, unsigned long addr,
					unsigned long pfn, pgprot_t pgprot)
{
	return _kcl_vmf_insert_pfn_prot(vma, addr, pfn, pgprot);
}
#endif /* HAVE_VMF_INSERT_PFN_PROT */

#ifdef HAVE_NO_KVREALLOC
void *kvrealloc(const void *p, size_t oldsize, size_t newsize, gfp_t flags);
#endif

#endif
