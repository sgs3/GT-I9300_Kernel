/*
 * linux/drivers/media/video/s5p-mfc/s5p_mfc_mem.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/dma-mapping.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-memops.h>
#include <asm/cacheflush.h>

#include "s5p_mfc_common.h"
#include "s5p_mfc_mem.h"
#include "s5p_mfc_pm.h"
#include "s5p_mfc_debug.h"

#define	MFC_ION_NAME	"s5p-mfc"

#if defined(CONFIG_S5P_MFC_VB2_CMA)
static const char *s5p_mem_types[] = {
	MFC_CMA_FW,
	MFC_CMA_BANK1,
	MFC_CMA_BANK2,
};

static unsigned long s5p_mem_alignments[] = {
	MFC_CMA_FW_ALIGN,
	MFC_CMA_BANK1_ALIGN,
	MFC_CMA_BANK2_ALIGN,
};

struct vb2_mem_ops *s5p_mfc_mem_ops(void)
{
	return (struct vb2_mem_ops *)&vb2_cma_phys_memops;
}

void **s5p_mfc_mem_init_multi(struct device *dev, unsigned int ctx_num)
{
/* TODO Cachable should be set */
	return (void **)vb2_cma_phys_init_multi(dev, ctx_num,
					   s5p_mem_types,
					   s5p_mem_alignments,0);
}

void s5p_mfc_mem_cleanup_multi(void **alloc_ctxes, unsigned int ctx_num)
{
	vb2_cma_phys_cleanup_multi(alloc_ctxes);
}
#elif defined(CONFIG_S5P_MFC_VB2_SDVMM)
struct vb2_mem_ops *s5p_mfc_mem_ops(void)
{
	return (struct vb2_mem_ops *)&vb2_sdvmm_memops;
}

void **s5p_mfc_mem_init_multi(struct device *dev, unsigned int ctx_num)
{
	struct vb2_vcm vcm;
	void ** alloc_ctxes;
	struct vb2_drv vb2_drv;

	vcm.vcm_id = VCM_DEV_MFC;
	/* FIXME: check port count */
	vcm.size = SZ_256M;

	vb2_drv.remap_dva = true;
	vb2_drv.cacheable = false;

	s5p_mfc_power_on();
	alloc_ctxes = (void **)vb2_sdvmm_init_multi(ctx_num, &vcm,
							NULL, &vb2_drv);
	s5p_mfc_power_off();

	return alloc_ctxes;
}

void s5p_mfc_mem_cleanup_multi(void **alloc_ctxes, unsigned int ctx_num)
{
	vb2_sdvmm_cleanup_multi(alloc_ctxes);
}
#elif defined(CONFIG_S5P_MFC_VB2_ION)
struct vb2_mem_ops *s5p_mfc_mem_ops(void)
{
	return (struct vb2_mem_ops *)&vb2_ion_memops;
}

void **s5p_mfc_mem_init_multi(struct device *dev, unsigned int ctx_num)
{
	struct s5p_mfc_dev *m_dev = platform_get_drvdata(to_platform_device(dev));
	void **alloc_ctxes;
	unsigned int i;

	alloc_ctxes = kmalloc(sizeof(*alloc_ctxes) * ctx_num, GFP_KERNEL);
	if (!alloc_ctxes)
		return NULL;

	for (i = 0; i < ctx_num; i++) {
		alloc_ctxes[i] = vb2_ion_create_context(dev,
				IS_MFCV6(m_dev) ? SZ_4K : SZ_128K,
				VB2ION_CTX_VMCONTIG | VB2ION_CTX_IOMMU);

		if (IS_ERR(alloc_ctxes[i]))
			break;
	}

	if (i < ctx_num) {
		while (i-- > 0)
			vb2_ion_destroy_context(alloc_ctxes[i]);

		kfree(alloc_ctxes);
		alloc_ctxes = NULL;
	}

	return alloc_ctxes;
}

void s5p_mfc_mem_cleanup_multi(void **alloc_ctxes, unsigned int ctx_num)
{
	while (ctx_num-- > 0)
		vb2_ion_destroy_context(alloc_ctxes[ctx_num]);

	kfree(alloc_ctxes);
}
#endif

#if defined(CONFIG_S5P_MFC_VB2_CMA)
struct vb2_cma_phys_conf {
	struct device		*dev;
	const char		*type;
	unsigned long		alignment;
	bool			cacheable;
};

struct vb2_cma_phys_buf {
	struct vb2_cma_phys_conf		*conf;
	dma_addr_t			paddr;
	unsigned long			size;
	struct vm_area_struct		*vma;
	atomic_t			refcount;
	struct vb2_vmarea_handler	handler;
	bool				cacheable;
};

void s5p_mfc_cache_clean_fw(void *cookie)
{
	phys_addr_t phys = (phys_addr_t)cookie;
	/* FIXME: cache maintenance operation */
	dmac_map_area(phys_to_virt(phys), SZ_1M, DMA_TO_DEVICE);
	outer_clean_range(phys, phys + SZ_1M);
}

void s5p_mfc_cache_clean(struct vb2_buffer *vb, int plane_no)
{
	struct vb2_cma_phys_buf *buf =
		(struct vb2_cma_phys_buf *)vb->planes[plane_no].mem_priv;
	void *start_addr;
	unsigned long size;
	unsigned long paddr = (dma_addr_t)buf->paddr;

	start_addr = (dma_addr_t *)phys_to_virt(buf->paddr);
	size = buf->size;

	dmac_map_area(start_addr, size, DMA_TO_DEVICE);
	outer_clean_range(paddr, paddr + size);
}

void s5p_mfc_cache_inv(struct vb2_buffer *vb, int plane_no)
{
	struct vb2_cma_phys_buf *buf =
		(struct vb2_cma_phys_buf *)vb->planes[plane_no].mem_priv;
	void *start_addr;
	unsigned long size;
	unsigned long paddr = (dma_addr_t)buf->paddr;

	start_addr = (dma_addr_t *)phys_to_virt(buf->paddr);
	size = buf->size;

	outer_inv_range(paddr, paddr + size);
	dmac_unmap_area(start_addr, size, DMA_FROM_DEVICE);
}

void s5p_mfc_mem_suspend(void *alloc_ctx)
{
	/* NOP */
}

int s5p_mfc_mem_resume(void *alloc_ctx)
{
	return 0;
}

void s5p_mfc_mem_set_cacheable(void *alloc_ctx, bool cacheable)
{
	vb2_cma_phys_set_cacheable(alloc_ctx, cacheable);
}

int s5p_mfc_mem_cache_flush(struct vb2_buffer *vb, u32 plane_no)
{
	vb2_cma_phys_cache_flush(vb, plane_no);
	return 0;
}
#elif defined(CONFIG_S5P_MFC_VB2_SDVMM)
void s5p_mfc_cache_clean(const void *start_addr, unsigned long size)
{
	unsigned long paddr;
	void *cur_addr, *end_addr;

	dmac_map_area(start_addr, size, DMA_TO_DEVICE);

	cur_addr = (void *)((unsigned long)start_addr & PAGE_MASK);
	end_addr = cur_addr + PAGE_ALIGN(size);

	while (cur_addr < end_addr) {
		paddr = page_to_pfn(vmalloc_to_page(cur_addr));
		paddr <<= PAGE_SHIFT;
		if (paddr)
			outer_clean_range(paddr, paddr + PAGE_SIZE);
		cur_addr += PAGE_SIZE;
	}

	/* FIXME: L2 operation optimization */
	/*
	unsigned long start, end, unitsize;
	unsigned long cur_addr, remain;

	dmac_map_area(start_addr, size, DMA_TO_DEVICE);

	cur_addr = (unsigned long)start_addr;
	remain = size;

	start = page_to_pfn(vmalloc_to_page(cur_addr));
	start <<= PAGE_SHIFT;
	if (start & PAGE_MASK) {
		unitsize = min((start | PAGE_MASK) - start + 1, remain);
		end = start + unitsize;
		outer_clean_range(start, end);
		remain -= unitsize;
		cur_addr += unitsize;
	}

	while (remain >= PAGE_SIZE) {
		start = page_to_pfn(vmalloc_to_page(cur_addr));
		start <<= PAGE_SHIFT;
		end = start + PAGE_SIZE;
		outer_clean_range(start, end);
		remain -= PAGE_SIZE;
		cur_addr += PAGE_SIZE;
	}

	if (remain) {
		start = page_to_pfn(vmalloc_to_page(cur_addr));
		start <<= PAGE_SHIFT;
		end = start + remain;
		outer_clean_range(start, end);
	}
	*/

}

void s5p_mfc_cache_inv(const void *start_addr, unsigned long size)
{
	unsigned long paddr;
	void *cur_addr, *end_addr;

	cur_addr = (void *)((unsigned long)start_addr & PAGE_MASK);
	end_addr = cur_addr + PAGE_ALIGN(size);

	while (cur_addr < end_addr) {
		paddr = page_to_pfn(vmalloc_to_page(cur_addr));
		paddr <<= PAGE_SHIFT;
		if (paddr)
			outer_inv_range(paddr, paddr + PAGE_SIZE);
		cur_addr += PAGE_SIZE;
	}

	dmac_unmap_area(start_addr, size, DMA_FROM_DEVICE);

	/* FIXME: L2 operation optimization */
	/*
	unsigned long start, end, unitsize;
	unsigned long cur_addr, remain;

	cur_addr = (unsigned long)start_addr;
	remain = size;

	start = page_to_pfn(vmalloc_to_page(cur_addr));
	start <<= PAGE_SHIFT;
	if (start & PAGE_MASK) {
		unitsize = min((start | PAGE_MASK) - start + 1, remain);
		end = start + unitsize;
		outer_inv_range(start, end);
		remain -= unitsize;
		cur_addr += unitsize;
	}

	while (remain >= PAGE_SIZE) {
		start = page_to_pfn(vmalloc_to_page(cur_addr));
		start <<= PAGE_SHIFT;
		end = start + PAGE_SIZE;
		outer_inv_range(start, end);
		remain -= PAGE_SIZE;
		cur_addr += PAGE_SIZE;
	}

	if (remain) {
		start = page_to_pfn(vmalloc_to_page(cur_addr));
		start <<= PAGE_SHIFT;
		end = start + remain;
		outer_inv_range(start, end);
	}

	dmac_unmap_area(start_addr, size, DMA_FROM_DEVICE);
	*/
}

void s5p_mfc_mem_suspend(void *alloc_ctx)
{
	s5p_mfc_clock_on();
	vb2_sdvmm_suspend(alloc_ctx);
	s5p_mfc_clock_off();
}

int s5p_mfc_mem_resume(void *alloc_ctx)
{
	s5p_mfc_clock_on();
	vb2_sdvmm_resume(alloc_ctx);
	s5p_mfc_clock_off();
	return 0;
}

void s5p_mfc_mem_set_cacheable(void *alloc_ctx, bool cacheable)
{
	vb2_sdvmm_set_cacheable(alloc_ctx, cacheable);
}

int s5p_mfc_mem_cache_flush(struct vb2_buffer *vb, u32 plane_no)
{
	return vb2_sdvmm_cache_flush(vb, plane_no);
}
#elif defined(CONFIG_S5P_MFC_VB2_ION)

void s5p_mfc_cache_clean_fw(void *cookie)
{
	int nents = 0;
	struct scatterlist *sg;

	sg = vb2_ion_get_sg(cookie, &nents);

	dma_sync_sg_for_device(NULL, sg, nents, DMA_TO_DEVICE);
}

void s5p_mfc_cache_clean(struct vb2_buffer *vb, int plane_no)
{
	void *cookie = vb2_plane_cookie(vb, plane_no);
	int nents = 0;
	struct scatterlist *sg;

	sg = vb2_ion_get_sg(cookie, &nents);

	dma_sync_sg_for_device(NULL, sg, nents, DMA_TO_DEVICE);
}

void s5p_mfc_cache_inv(struct vb2_buffer *vb, int plane_no)
{
	void *cookie = vb2_plane_cookie(vb, plane_no);
	int nents = 0;
	struct scatterlist *sg;

	sg = vb2_ion_get_sg(cookie, &nents);

	dma_sync_sg_for_device(NULL, sg, nents, DMA_FROM_DEVICE);
}

void s5p_mfc_mem_suspend(void *alloc_ctx)
{
	vb2_ion_detach_iommu(alloc_ctx);
}

int s5p_mfc_mem_resume(void *alloc_ctx)
{
	return vb2_ion_attach_iommu(alloc_ctx);
}

void s5p_mfc_mem_set_cacheable(void *alloc_ctx, bool cacheable)
{
	vb2_ion_set_cached(alloc_ctx, cacheable);
}

int s5p_mfc_mem_cache_flush(struct vb2_buffer *vb, u32 plane_no)
{
	return vb2_ion_cache_flush(vb, plane_no);
}
#endif
