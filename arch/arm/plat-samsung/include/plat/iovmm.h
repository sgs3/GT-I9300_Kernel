/* linux/arch/arm/plat-s5p/include/plat/iovmm.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_PLAT_IOVMM_H
#define __ASM_PLAT_IOVMM_H

#ifdef CONFIG_EXYNOS_IOVMM

struct scatterlist;

int iovmm_setup(struct device *dev);
void iovmm_cleanup(struct device *dev);
int iovmm_activate(struct device *dev);
void iovmm_deactivate(struct device *dev);

/* iovmm_map() - Maps a list of physical memory chunks
 * @dev: the owner of the IO address space where the mapping is created
 * @sg: list of physical memory chunks to map
 * @offset: length in bytes where the mapping starts
 * @size: how much memory to map in bytes. @offset + @size must not exceed
 *        total size of @sg
 *
 * This function returns mapped IO address in the address space of @dev.
 * Returns 0 if mapping fails.
 *
 * The caller of this function must ensure that iovmm_cleanup() is not called
 * while this function is called.
 *
 */
dma_addr_t iovmm_map(struct device *dev, struct scatterlist *sg, off_t offset,
								size_t size);

/* iovmm_map() - unmaps the given IO address
 * @dev: the owner of the IO address space where @iova belongs
 * @iova: IO address that needs to be unmapped and freed.
 *
 * The caller of this function must ensure that iovmm_cleanup() is not called
 * while this function is called.
 */
void iovmm_unmap(struct device *dev, dma_addr_t iova);

/* iovmm_map_oto - create one to one mapping for the given physical address
 * @dev: the owner of the IO address space to map
 * @phys: physical address to map
 * @size: size of the mapping to create
 *
 * This function return 0 if mapping is successful. Otherwise, minus error
 * value.
 */
int iovmm_map_oto(struct device *dev, phys_addr_t phys, size_t size);

/* iovmm_unmap_oto - remove one to one mapping
 * @dev: the owner ofthe IO address space
 * @phys: physical address to remove mapping
 */
void iovmm_unmap_oto(struct device *dev, phys_addr_t phys);

#else
#define iovmm_setup(dev)		(-ENOSYS)
#define iovmm_cleanup(dev)		do { } while (0)
#define iovmm_activate(dev)		(-ENOSYS)
#define iovmm_deactivate(dev)		do { } while (0)
#define iovmm_map(dev, sg)		(0)
#define iovmm_unmap(dev, iova)		do { } while (0)
#define iovmm_map_oto(dev, phys, size)	(0)
#define iovmm_unmap_oto(dev, phys)	do { } while (0)
#endif /* CONFIG_EXYNOS_IOVMM */

#endif /*__ASM_PLAT_IOVMM_H*/
