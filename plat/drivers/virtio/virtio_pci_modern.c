/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Andrii Strynzha <a.strynzha@gmail.com>,
 *	    Sharan Santhanam <sharan.santhanam@neclab.eu>
 *
 * Copyright (c) 2018, NEC Europe Ltd., NEC Corporation. All rights reserved.
 * Copyright (c) 2019, Karlsruhe Institute of Technology (KIT).
 *               All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2017, Bryan Venteicher <bryanv@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Taken and adopted from https://github.com/freebsd/freebsd-src
 * sys/dev/virtio/pci/virtio_pci_modern.c
 * Commit: 2c2ef67
 */

/* Driver for the modern VirtIO PCI interface. */

#include "uk/assert.h"
#include "uk/plat/bootstrap.h"
#include "uk/thread.h"
#include <stdint.h>
#include <uk/list.h>
#include <uk/config.h>
#include <uk/arch/types.h>
#include <errno.h>
#include <uk/alloc.h>
#include <uk/print.h>
#include <uk/plat/lcpu.h>
#include <uk/plat/irq.h>
#include <pci/pci_bus.h>
#include <virtio/virtio_config.h>
#include <virtio/virtio_bus.h>
#include <virtio/virtqueue.h>
#include <virtio/virtio_ring.h>
#include <uk/plat/io.h>
#include <virtio/virtio_pci_modern.h>
#include <pci/pci_bus.h>

static struct uk_alloc *a;

/**
 * @brief Contains information to access a virtio config struct through a BAR
 *
 */
struct vpci_modern_resource_map {
	/* Address of a device configuration space, to which this resource
	 * belongs */
	uint32_t vrm_config_addr;
	/* Offset to the corresponding capability within the device
	   configuration space */
	int vrm_cap_offset;
	/* BAR #, in which the structure lies. Can be between 0x0 and 0x5 */
	uint8_t vrm_bar;
	/* Offset to the struct within the BAR */
	uint64_t vrm_struct_offset;
	/* Length of the struct within the BAR */
	uint64_t vrm_struct_len;
	/* Type of BAR. SYS_RES_{MEMORY, IOPORT} */
	int vrm_type;
};

/**
 * TODOFS: think how to change this.
 *
 */
struct virtio_pci_modern_dev {

	/* PCI device information */
	struct pci_device *pdev;
	/* Virtio Device */
	struct virtio_dev vdev;

	uint32_t vpci_notify_offset_multiplier;

	struct vpci_modern_resource_map vpci_common_res_map;
	struct vpci_modern_resource_map vpci_notify_res_map;
	struct vpci_modern_resource_map vpci_isr_res_map;
	struct vpci_modern_resource_map vpci_device_res_map;
	struct vpci_modern_resource_map vpci_shared_mem_res_map;

	bool shared_mem_present;
	// TODOFS: remove this.
	unsigned long pci_base_addr;
	unsigned long pci_isr_addr;
};


/**
 * Fetch the virtio pci information from the virtio device.
 * @param vdev
 *	Reference to the virtio device.
 */
#define to_virtio_pci_modern_dev(vdev) \
		__containerof(vdev, struct virtio_pci_modern_dev, vdev)

// TODOFS: declare all functions at the beginning of the file
/**
 * Configuration operations legacy functions (`virtio_config_ops`)
 */
static void 	vpci_modern_pci_dev_reset(struct virtio_dev *vdev);
static int 	vpci_modern_pci_config_set(struct virtio_dev *vdev, __u8 offset,
				      const void *buf, __u8 type_len);
static int 	vpci_modern_pci_config_get(struct virtio_dev *vdev, __u8 offset,
				      void *buf, __u8 type_len);
static __u64 	vpci_modern_features_get(struct virtio_dev *vdev);
static void 	vpci_modern_features_set(struct virtio_dev *vdev,
					 __u64 features);
static void 	vpci_modern_pci_status_set(struct virtio_dev *vdev, __u8 status);
static __u8 	vpci_modern_pci_status_get(struct virtio_dev *vdev);
static int 	vpci_modern_pci_vq_find(struct virtio_dev *vdev, __u16 num_vq,
				   __u16 *qdesc_size);
static struct virtqueue
		*vpci_modern_vq_setup(struct virtio_dev *vdev,
				      __u16 queue_id,
				      __u16 num_desc,
				      virtqueue_callback_t callback,
				      struct uk_alloc *a);
static void 	vpci_legacy_vq_release(struct virtio_dev *vdev,
		struct virtqueue *vq, struct uk_alloc *a);
static void vpci_modern_vq_enable(struct virtio_dev *vdev,
				  struct virtqueue *vq);



static int 	virtio_pci_handle(void *arg);
static int 	vpci_modern_notify(struct virtio_dev *vdev, __u16 queue_id);
static uint64_t	vpci_modern_base_addr_get(struct vpci_modern_resource_map *res_map);
static int 	vpci_modern_map_configs(struct virtio_pci_modern_dev *vpdev);
static int	vpci_modern_map_common_config(struct virtio_pci_modern_dev *vpdev);
static int	vpci_modern_map_notify_config(struct virtio_pci_modern_dev *vpdev);
static int	vpci_modern_map_isr_config(struct virtio_pci_modern_dev *vpdev);
static int	vpci_modern_map_device_config(struct virtio_pci_modern_dev *vpdev);
static int	vpci_modern_find_cap_resource(struct virtio_pci_modern_dev *vpdev,
					      uint8_t cfg_type, __s64 min_size,
					      int alignment,
					      struct vpci_modern_resource_map *res);
static int	vpci_modern_bar_type(struct virtio_pci_modern_dev *vpdev, int bar);
static int	virtio_pci_modern_find_cap(struct pci_device *pdev,
					   __u8 target_cfg_type, int *cap_offset);
static uint8_t	vpci_modern_config_generation(struct virtio_pci_modern_dev *vpdev);
static uint8_t vpci_modern_read_common_1(struct virtio_pci_modern_dev *vpdev,
					 uint8_t off);
static uint16_t vpci_modern_read_common_2(struct virtio_pci_modern_dev *vpdev,
					  uint8_t off);
static uint32_t	vpci_modern_read_common_4(struct virtio_pci_modern_dev *vpdev,
					  uint8_t off);
static void	vpci_modern_write_common_1(struct virtio_pci_modern_dev *vpdev,
					   uint8_t off, uint8_t val);
static void	vpci_modern_write_common_2(struct virtio_pci_modern_dev *vpdev,
					   uint8_t off, uint16_t val);
static void	vpci_modern_write_common_4(struct virtio_pci_modern_dev *vpdev,
					   uint8_t off, uint32_t val);
static void	vpci_modern_write_common_8(struct virtio_pci_modern_dev *vpdev,
					   uint8_t off, uint64_t val);
static uint8_t	vpci_modern_read_isr_1(struct virtio_pci_modern_dev *vpdev,
				       uint8_t off);
static int	vpci_modern_read_device_1(struct virtio_pci_modern_dev *vpdev,
					  uint8_t off, uint8_t *buf);
static int	vpci_modern_read_device_2(struct virtio_pci_modern_dev *vpdev,
					  uint8_t off, uint16_t *buf);
static int	vpci_modern_read_device_4(struct virtio_pci_modern_dev *vpdev,
					  uint8_t off, uint32_t *buf);
static void	vpci_modern_write_notify_2(struct virtio_pci_modern_dev *vpdev,
					   uint16_t off,
					   uint16_t val);

static int 	virtio_pci_modern_add_dev(struct pci_device *pci_dev);


static uint64_t
vpci_modern_get_shm_addr(struct virtio_dev *vdev, uint8_t shm_id);

static uint64_t
vpci_modern_get_shm_length(struct virtio_dev *vdev, uint8_t shm_id);

static bool
vpci_modern_shm_present(struct virtio_dev *vdev, uint8_t shm_id);

/**
 * Configuration operations legacy PCI device.
 */
static struct virtio_config_ops vpci_modern_ops = {
	.device_reset		= vpci_modern_pci_dev_reset,
	.modern_config_get	= vpci_modern_pci_config_get,
	.modern_config_set	= vpci_modern_pci_config_set,
	.features_get		= vpci_modern_features_get,
	.features_set		= vpci_modern_features_set,
	.status_get		= vpci_modern_pci_status_get,
	.status_set		= vpci_modern_pci_status_set,
	.vqs_find		= vpci_modern_pci_vq_find,
	.vq_setup		= vpci_modern_vq_setup,
	.vq_release		= vpci_legacy_vq_release,
	.vq_enable		= vpci_modern_vq_enable,
	.get_shm_addr		= vpci_modern_get_shm_addr,
	.get_shm_length		= vpci_modern_get_shm_length,
	.shm_present		= vpci_modern_shm_present
};

static void vpci_modern_vq_enable(struct virtio_dev *vdev, struct virtqueue *vq)
{
	struct virtio_pci_modern_dev *vpdev = vdev->priv;

	UK_ASSERT(vpdev);

	vpci_modern_write_common_2(vpdev, VIRTIO_MODERN_COMMON_Q_SELECT,
				   vq->queue_id);
	vpci_modern_write_common_2(vpdev, VIRTIO_MODERN_COMMON_Q_ENABLE,
				   1);
}

static void vpci_legacy_vq_release(struct virtio_dev *vdev,
		struct virtqueue *vq, struct uk_alloc *a)
{
	struct virtio_pci_modern_dev *vpdev = NULL;
	long flags;

	UK_ASSERT(vq != NULL);
	UK_ASSERT(a != NULL);
	vpdev = to_virtio_pci_modern_dev(vdev);

	/* Select and deactivate the queue */
	virtio_cwrite16((void *)(unsigned long)vpdev->pci_base_addr,
			VIRTIO_QUEUE_SEL, vq->queue_id);
	virtio_cwrite32((void *)(unsigned long)vpdev->pci_base_addr,
			VIRTIO_QUEUE_PFN, 0);

	flags = ukplat_lcpu_save_irqf();
	UK_TAILQ_REMOVE(&vpdev->vdev.vqs, vq, next);
	ukplat_lcpu_restore_irqf(flags);

	virtqueue_destroy(vq, a);
}

/**
 * @brief returns the base address where the BAR, containing the res_map begins
 *
 * @param res_map
 * @return uint64_t
 */
static uint64_t
vpci_modern_base_addr_get(struct vpci_modern_resource_map *res_map)
{
	int mem_type; /* Whether a BAR is 32 or 64 byte wide. Used only with
			 memory address BARs (not IO space BARs). */
	uint64_t base_addr = 0;
	uint64_t base_addr_hi_lo; /* In case of a 64 byte wide BAR, used to
				     separately fetch the hightest and the
				     lowest 32 bits of the base_addr. */

	UK_ASSERT(res_map);
	UK_ASSERT(res_map->vrm_type == SYS_RES_IOPORT ||
			res_map->vrm_type == SYS_RES_MEMORY);

	switch (res_map->vrm_type) {
	case SYS_RES_IOPORT:
		PCI_CONF_READ_OFFSET(uint64_t, &base_addr,
			res_map->vrm_config_addr,
			PCI_BAR_OFFSET(res_map->vrm_bar),
			PCI_BAR_IO_ADDR_SHIFT, PCI_BAR_IO_ADDR_MASK);
		UK_ASSERT(base_addr < 0xFFFF);
		// TODO: need to check that base_addr is < 2^16-1? Bc the
		// I/O address space is 16 bit addressable?
		break;
	case SYS_RES_MEMORY:
		PCI_CONF_READ_OFFSET(uint64_t, &mem_type,
			res_map->vrm_config_addr,
			PCI_BAR_OFFSET(res_map->vrm_bar),
			PCI_BAR_MEM_TYPE_SHIFT,
			PCI_BAR_MEM_TYPE_MASK);
		if (mem_type == 0x0) { /* 32bit memory space */
			PCI_CONF_READ_OFFSET(uint64_t, &base_addr,
				res_map->vrm_config_addr,
				PCI_BAR_OFFSET(res_map->vrm_bar),
				PCI_BAR_MEM_32_SHIFT,
				PCI_BAR_MEM_32_MASK);
		} else if (mem_type == 0x2) { /* 64bit memory space */
			/* Get the lowest 32 bits. */
			PCI_CONF_READ_OFFSET(uint64_t, &base_addr_hi_lo,
				res_map->vrm_config_addr,
				PCI_BAR_OFFSET(res_map->vrm_bar),
				PCI_BAR_MEM_32_SHIFT,
				PCI_BAR_MEM_32_MASK);
			base_addr = base_addr_hi_lo;
			/* Get the highest 32 bits. */
			PCI_CONF_READ_OFFSET(uint64_t, &base_addr_hi_lo,
				res_map->vrm_config_addr,
				PCI_BAR_OFFSET(res_map->vrm_bar + 1),
				PCI_BAR_MEM_32_SHIFT,
				PCI_BAR_MEM_64_MASK);
			base_addr_hi_lo = base_addr_hi_lo << 32;
			base_addr += base_addr_hi_lo;
		}
		break;
	}

	return base_addr;
}

static uint8_t vpci_modern_read_common_1(struct virtio_pci_modern_dev *vpdev,
					 uint8_t off)
{
	uint64_t base_addr;

	UK_ASSERT(vpdev);
	UK_ASSERT(vpdev->vpci_common_res_map.vrm_type == SYS_RES_IOPORT ||
			vpdev->vpci_common_res_map.vrm_type == SYS_RES_MEMORY);

	if (unlikely(off > vpdev->vpci_common_res_map.vrm_struct_len)) {
		uk_pr_err("Cannot read common config. The write offset is \
		greater than the struct length.\n");
		return -1;
	}
	base_addr = vpci_modern_base_addr_get(&vpdev->vpci_common_res_map)
			+ vpdev->vpci_common_res_map.vrm_struct_offset;


	switch (vpdev->vpci_common_res_map.vrm_type) {
	case SYS_RES_IOPORT:
		return virtio_cread8((void *) base_addr,off);
	case SYS_RES_MEMORY:
		return virtio_mem_cread8((void *) base_addr, off);
	default:
		uk_pr_err("Unknown BAR type\n");
		return -1;
	}
}

static uint16_t
vpci_modern_read_common_2(struct virtio_pci_modern_dev *vpdev,
			  uint8_t off)
{
	uint64_t base_addr;

	UK_ASSERT(vpdev);
	UK_ASSERT(vpdev->vpci_common_res_map.vrm_type == SYS_RES_IOPORT ||
			vpdev->vpci_common_res_map.vrm_type == SYS_RES_MEMORY);

	if (unlikely(off > vpdev->vpci_common_res_map.vrm_struct_len)) {
		uk_pr_err("Cannot read common config. The write offset is \
		greater than the struct length.\n");
		return -1;
	}
	// TODOFS: refactoring: maybe calculate the base address only once and
	// save in the map?
	base_addr = vpci_modern_base_addr_get(&vpdev->vpci_common_res_map)
			+ vpdev->vpci_common_res_map.vrm_struct_offset;

	switch (vpdev->vpci_common_res_map.vrm_type) {
	case SYS_RES_IOPORT:
		return virtio_cread16((void *) base_addr,off);
	case SYS_RES_MEMORY:
		return virtio_mem_cread16((void *) base_addr, off);
	default:
		uk_pr_err("Unknown BAR type\n");
		return -1;
	}
}

static uint32_t
vpci_modern_read_common_4(struct virtio_pci_modern_dev *vpdev,
			  uint8_t off)
{
	uint64_t base_addr;

	UK_ASSERT(vpdev);
	UK_ASSERT(vpdev->vpci_common_res_map.vrm_type == SYS_RES_IOPORT ||
			vpdev->vpci_common_res_map.vrm_type == SYS_RES_MEMORY);

	if (unlikely(off > vpdev->vpci_common_res_map.vrm_struct_len)) {
		uk_pr_err("Cannot read common config. The write offset is \
		greater than the struct length.\n");
		return -1;
	}
	// TODOFS: refactoring: maybe calculate the base address only once and
	// save in the map?
	base_addr = vpci_modern_base_addr_get(&vpdev->vpci_common_res_map)
			+ vpdev->vpci_common_res_map.vrm_struct_offset;

	switch (vpdev->vpci_common_res_map.vrm_type) {
	case SYS_RES_IOPORT:
		return virtio_cread32((void *) base_addr,off);
	case SYS_RES_MEMORY:
		return virtio_mem_cread32((void *) base_addr, off);
	default:
		uk_pr_err("Unknown BAR type\n");
		return -1;
	}
}

static void
vpci_modern_write_common_1(struct virtio_pci_modern_dev *vpdev, uint8_t off,
			   uint8_t val)
{
	uint64_t base_addr;

	UK_ASSERT(vpdev);
	UK_ASSERT(vpdev->vpci_common_res_map.vrm_type == SYS_RES_IOPORT ||
			vpdev->vpci_common_res_map.vrm_type == SYS_RES_MEMORY);

	if (unlikely(off > vpdev->vpci_common_res_map.vrm_struct_len)) {
		uk_pr_err("Cannot write common config. The write offset is \
		greater than the struct length.\n");
	}
	base_addr = vpci_modern_base_addr_get(&vpdev->vpci_common_res_map) +
		vpdev->vpci_common_res_map.vrm_struct_offset;

	switch (vpdev->vpci_common_res_map.vrm_type) {
	case SYS_RES_IOPORT:
		virtio_cwrite8((void *) base_addr,off, val);
		break;
	case SYS_RES_MEMORY:
		virtio_mem_cwrite8((void *) base_addr, off, val);
		break;
	default:
		uk_pr_err("Unknown BAR type\n");
	}
}

static void
vpci_modern_write_common_2(struct virtio_pci_modern_dev *vpdev, uint8_t off,
			   uint16_t val)
{
	uint64_t base_addr;

	UK_ASSERT(vpdev);
	UK_ASSERT(vpdev->vpci_common_res_map.vrm_type == SYS_RES_IOPORT ||
			vpdev->vpci_common_res_map.vrm_type == SYS_RES_MEMORY);

	if (unlikely(off > vpdev->vpci_common_res_map.vrm_struct_len)) {
		uk_pr_err("Cannot write common config. The write offset is \
		greater than the struct length.\n");
	}
	base_addr = vpci_modern_base_addr_get(&vpdev->vpci_common_res_map) +
		vpdev->vpci_common_res_map.vrm_struct_offset;

	switch (vpdev->vpci_common_res_map.vrm_type) {
	case SYS_RES_IOPORT:
		virtio_cwrite16((void *) base_addr,off, val);
		break;
	case SYS_RES_MEMORY:
		virtio_mem_cwrite16((void *) base_addr, off, val);
		break;
	default:
		uk_pr_err("Unknown BAR type\n");
	}
}

static void
vpci_modern_write_common_4(struct virtio_pci_modern_dev *vpdev, uint8_t off,
			   uint32_t val)
{
	uint64_t base_addr;

	UK_ASSERT(vpdev);
	UK_ASSERT(vpdev->vpci_common_res_map.vrm_type == SYS_RES_IOPORT ||
			vpdev->vpci_common_res_map.vrm_type == SYS_RES_MEMORY);

	if (unlikely(off > vpdev->vpci_common_res_map.vrm_struct_len)) {
		uk_pr_err("Cannot write common config. The write offset is \
		greater than the struct length.\n");
	}
	base_addr = vpci_modern_base_addr_get(&vpdev->vpci_common_res_map) +
		vpdev->vpci_common_res_map.vrm_struct_offset;

	switch (vpdev->vpci_common_res_map.vrm_type) {
	case SYS_RES_IOPORT:
		virtio_cwrite32((void *) base_addr,off, val);
		break;
	case SYS_RES_MEMORY:
		virtio_mem_cwrite32((void *) base_addr, off, val);
		break;
	default:
		uk_pr_err("Unknown BAR type\n");
	}
}

static void
vpci_modern_write_common_8(struct virtio_pci_modern_dev *vpdev, uint8_t off,
			   uint64_t val)
{
	uint32_t val0, val1;

	UK_ASSERT(vpdev);

	val0 = (uint32_t) val;
	val1 = val >> 32;

	vpci_modern_write_common_4(vpdev, off, val0);
	vpci_modern_write_common_4(vpdev, off + 4, val1);
}

static uint8_t
vpci_modern_read_isr_1(struct virtio_pci_modern_dev *vpdev, uint8_t off)
{
	uint64_t base_addr;

	UK_ASSERT(vpdev);
	UK_ASSERT(vpdev->vpci_isr_res_map.vrm_type == SYS_RES_IOPORT ||
			vpdev->vpci_isr_res_map.vrm_type == SYS_RES_MEMORY);

	if (unlikely(off > vpdev->vpci_isr_res_map.vrm_struct_len)) {
		uk_pr_err("Cannot write isr config. The write offset is \
		greater than the struct length.\n");
		return -1;
	}
	base_addr = vpci_modern_base_addr_get(&vpdev->vpci_isr_res_map)
			+ vpdev->vpci_isr_res_map.vrm_struct_offset;

	switch (vpdev->vpci_isr_res_map.vrm_type) {
	case SYS_RES_IOPORT:
		return virtio_cread8((void *) base_addr,off);
	case SYS_RES_MEMORY:
		return virtio_mem_cread8((void *) base_addr, off);
	default:
		uk_pr_err("Unknown BAR type\n");
		return -1;
	}
}

static int
vpci_modern_read_device_1(struct virtio_pci_modern_dev *vpdev, uint8_t off,
			  uint8_t *buf)
{
	uint64_t base_addr;

	UK_ASSERT(vpdev);
	UK_ASSERT(vpdev->vpci_device_res_map.vrm_type == SYS_RES_IOPORT ||
			vpdev->vpci_device_res_map.vrm_type == SYS_RES_MEMORY);

	if (unlikely(off > vpdev->vpci_device_res_map.vrm_struct_len)) {
		uk_pr_err("Cannot read device config. The write offset is \
		greater than the struct length.\n");
		return -1;
	}
	base_addr = vpci_modern_base_addr_get(&vpdev->vpci_device_res_map)
			+ vpdev->vpci_device_res_map.vrm_struct_offset;

	switch (vpdev->vpci_device_res_map.vrm_type) {
	case SYS_RES_IOPORT:
		*buf = virtio_cread8((void *) base_addr,off);
		break;
	case SYS_RES_MEMORY:
		*buf = virtio_mem_cread8((void *) base_addr, off);
		break;
	default:
		uk_pr_err("Unknown BAR type\n");
		return -1;
	}

	return 0;
}

static int
vpci_modern_read_device_2(struct virtio_pci_modern_dev *vpdev, uint8_t off,
			  uint16_t *buf)
{
	uint64_t base_addr;

	UK_ASSERT(vpdev);
	UK_ASSERT(vpdev->vpci_device_res_map.vrm_type == SYS_RES_IOPORT ||
			vpdev->vpci_device_res_map.vrm_type == SYS_RES_MEMORY);

	if (unlikely(off > vpdev->vpci_device_res_map.vrm_struct_len)) {
		uk_pr_err("Cannot read device config. The write offset is \
		greater than the struct length.\n");
		return -1;
	}
	base_addr = vpci_modern_base_addr_get(&vpdev->vpci_device_res_map)
			+ vpdev->vpci_device_res_map.vrm_struct_offset;

	switch (vpdev->vpci_device_res_map.vrm_type) {
	case SYS_RES_IOPORT:
		*buf = virtio_cread16((void *) base_addr,off);
		break;
	case SYS_RES_MEMORY:
		*buf = virtio_mem_cread16((void *) base_addr, off);
		break;
	default:
		uk_pr_err("Unknown BAR type\n");
		return -1;
	}

	return 0;
}

static int
vpci_modern_read_device_4(struct virtio_pci_modern_dev *vpdev, uint8_t off,
			  uint32_t *buf)
{
	uint64_t base_addr;

	UK_ASSERT(vpdev);
	UK_ASSERT(vpdev->vpci_device_res_map.vrm_type == SYS_RES_IOPORT ||
			vpdev->vpci_device_res_map.vrm_type == SYS_RES_MEMORY);

	if (unlikely(off > vpdev->vpci_device_res_map.vrm_struct_len)) {
		uk_pr_err("Cannot read device config. The write offset is \
		greater than the struct length.\n");
		return -1;
	}
	base_addr = vpci_modern_base_addr_get(&vpdev->vpci_device_res_map)
			+ vpdev->vpci_device_res_map.vrm_struct_offset;

	switch (vpdev->vpci_device_res_map.vrm_type) {
	case SYS_RES_IOPORT:
		*buf = virtio_cread32((void *) base_addr,off);
		break;
	case SYS_RES_MEMORY:
		*buf = virtio_mem_cread32((void *) base_addr, off);
		break;
	default:
		UK_CRASH("Unknown BAR type\n");
	}

	return 0;
}

static uint8_t
vpci_modern_config_generation(struct virtio_pci_modern_dev *vpdev)
{
	uint8_t gen;

	UK_ASSERT(vpdev);

	gen = vpci_modern_read_common_1(vpdev, VIRTIO_MODERN_COMMON_CFGGEN);

	return gen;
}

static int
vpci_modern_read_device_8(struct virtio_pci_modern_dev *vpdev, uint8_t off,
			  uint64_t *buf)
{
	uint8_t gen;
	uint32_t val0, val1;
	int rc = 0;

	/*
	 * Treat the 64-bit field as two 32-bit fields. Use the generation
	 * to ensure a consistent read. (Reads from multiple fields or fields
	 * greater, than 32 bits, are non-atomic)
	 */
	do {
		gen = vpci_modern_config_generation(vpdev);
		rc = vpci_modern_read_device_4(vpdev, off, &val0);
		rc |= vpci_modern_read_device_4(vpdev, off + 4, &val1);
	} while (gen != vpci_modern_config_generation(vpdev));

	if (rc)
		return rc;

	*buf = (((uint64_t) val1 << 32) | val0);
	return 0;
}

static void
vpci_modern_write_device_1(struct virtio_pci_modern_dev *vpdev, uint8_t off,
			   uint8_t val)
{
	uint64_t base_addr;

	UK_ASSERT(vpdev);
	UK_ASSERT(vpdev->vpci_device_res_map.vrm_type == SYS_RES_IOPORT ||
			vpdev->vpci_device_res_map.vrm_type == SYS_RES_MEMORY);

	if (unlikely(off > vpdev->vpci_device_res_map.vrm_struct_len)) {
		uk_pr_err("Cannot write device config. The write offset is \
		greater than the struct length.\n");
		return;
	}
	base_addr = vpci_modern_base_addr_get(&vpdev->vpci_device_res_map)
			+ vpdev->vpci_device_res_map.vrm_struct_offset;

	switch (vpdev->vpci_device_res_map.vrm_type) {
	case SYS_RES_IOPORT:
		virtio_cwrite8((void *) base_addr,off, val);
		break;
	case SYS_RES_MEMORY:
		virtio_mem_cwrite8((void *) base_addr, off, val);
		break;
	default:
		uk_pr_err("Unknown BAR type\n");
	}
}

static void
vpci_modern_write_device_2(struct virtio_pci_modern_dev *vpdev, uint8_t off,
			   uint16_t val)
{
	uint64_t base_addr;

	UK_ASSERT(vpdev);
	UK_ASSERT(vpdev->vpci_device_res_map.vrm_type == SYS_RES_IOPORT ||
			vpdev->vpci_device_res_map.vrm_type == SYS_RES_MEMORY);

	if (unlikely(off > vpdev->vpci_device_res_map.vrm_struct_len)) {
		uk_pr_err("Cannot write device config. The write offset is \
		greater than the struct length.\n");
		return;
	}
	base_addr = vpci_modern_base_addr_get(&vpdev->vpci_device_res_map)
			+ vpdev->vpci_device_res_map.vrm_struct_offset;

	switch (vpdev->vpci_device_res_map.vrm_type) {
	case SYS_RES_IOPORT:
		virtio_cwrite16((void *) base_addr,off, val);
		break;
	case SYS_RES_MEMORY:
		virtio_mem_cwrite16((void *) base_addr, off, val);
		break;
	default:
		uk_pr_err("Unknown BAR type\n");
	}
}

static void
vpci_modern_write_device_4(struct virtio_pci_modern_dev *vpdev, uint8_t off,
			   uint32_t val)
{
	uint64_t base_addr;

	UK_ASSERT(vpdev);
	UK_ASSERT(vpdev->vpci_device_res_map.vrm_type == SYS_RES_IOPORT ||
			vpdev->vpci_device_res_map.vrm_type == SYS_RES_MEMORY);

	if (unlikely(off > vpdev->vpci_device_res_map.vrm_struct_len)) {
		uk_pr_err("Cannot write device config. The write offset is \
		greater than the struct length.\n");
		return;
	}
	base_addr = vpci_modern_base_addr_get(&vpdev->vpci_device_res_map)
			+ vpdev->vpci_device_res_map.vrm_struct_offset;

	switch (vpdev->vpci_device_res_map.vrm_type) {
	case SYS_RES_IOPORT:
		virtio_cwrite32((void *) base_addr,off, val);
		break;
	case SYS_RES_MEMORY:
		virtio_mem_cwrite32((void *) base_addr, off, val);
		break;
	default:
		uk_pr_err("Unknown BAR type\n");
	}
}

static void
vpci_modern_write_device_8(struct virtio_pci_modern_dev *vpdev, uint8_t off,
			   uint64_t val)
{
	uint32_t val0, val1;

	val0 = (uint32_t) val;
	val1 = val >> 32;

	vpci_modern_write_device_4(vpdev, off, val0);
	vpci_modern_write_device_4(vpdev, off + 4, val1);

}

static void
vpci_modern_write_notify_2(struct virtio_pci_modern_dev *vpdev, uint16_t off,
			   uint16_t val)
{
	uint64_t base_addr;

	UK_ASSERT(vpdev);
	UK_ASSERT(vpdev->vpci_notify_res_map.vrm_type == SYS_RES_IOPORT ||
			vpdev->vpci_notify_res_map.vrm_type == SYS_RES_MEMORY);

	if (unlikely(off > vpdev->vpci_notify_res_map.vrm_struct_len)) {
		uk_pr_err("Cannot write common config. The write offset is \
		greater than the struct length.\n");
		return;
	}
	base_addr = vpci_modern_base_addr_get(&vpdev->vpci_notify_res_map)
		    + vpdev->vpci_notify_res_map.vrm_struct_offset;

	switch (vpdev->vpci_notify_res_map.vrm_type) {
	case SYS_RES_IOPORT:
		virtio_cwrite16((void *) base_addr, off, val);
		break;
	case SYS_RES_MEMORY:
		virtio_mem_cwrite16((void *) base_addr, off, val);
		break;
	}
}

/**
 * @brief setting the config for the vdev
 *
 * @param vdev
 * @param offset
 * @param buf
 * @param len
 * @return int
 */
static int vpci_modern_pci_config_set(struct virtio_dev *vdev, __u8 offset,
				      const void *buf, __u8 type_len)
{
	struct virtio_pci_modern_dev *vpdev = NULL;

	UK_ASSERT(vdev);
	vpdev = to_virtio_pci_modern_dev(vdev);

	if (vpdev->vpci_device_res_map.vrm_struct_len == 0) {
		uk_pr_err("Attempt to write device-specific configuration\
					which is not present\n");
		return -1;
	}

	switch (type_len) {
	case 1:
		vpci_modern_write_device_1(vpdev, offset,
					   *(const uint8_t *) buf);
		break;
	case 2: {
		vpci_modern_write_device_2(vpdev, offset,
					   *(const uint16_t *) buf);
		break;
	}
	case 4: {
		vpci_modern_write_device_4(vpdev, offset,
					   *(const uint32_t *) buf);
		break;
	}
	case 8: {
		vpci_modern_write_device_8(vpdev, offset,
					   *(const uint64_t *) buf);
		break;
	}
	default:
		uk_pr_err("%s: device %" __PRIu16 " invalid device config write\
		length %" __PRIu8 " offset %" __PRIu16 "\n", __func__,
		vdev->id.virtio_device_id, type_len, offset);
		return -1;
	}

	return 0;
}

/**
 * @brief Reads the device-specific configuration
 *
 * Reads from an @p offset of the device-specific configuration @p type_len
 * bytes and saves them into the @p buf. @p buf is assumed to be at least the
 * type_len bytes in size.
 *
 * @param vdev
 * @param offset
 * @param buf
 * @param len is unused
 * @param type_len
 * @return int
 */
static int vpci_modern_pci_config_get(struct virtio_dev *vdev, __u8 offset,
				      void *buf, __u8 type_len)
{
	struct virtio_pci_modern_dev *vpdev = NULL;
	int rc = 0;

	UK_ASSERT(vdev);
	vpdev = to_virtio_pci_modern_dev(vdev);

	if (vpdev->vpci_device_res_map.vrm_struct_len == 0) {
		uk_pr_err("Attempt to read device-specific configuration\
					which is not present\n");
		return -1;
	}

	switch (type_len) {
	case 1:
		rc = vpci_modern_read_device_1(vpdev, offset, (uint8_t *) buf);
		break;
	case 2:
		rc = vpci_modern_read_device_2(vpdev, offset, (uint16_t *) buf);
		break;
	case 4:
		rc = vpci_modern_read_device_4(vpdev, offset, (uint32_t *) buf);
		break;
	case 8:
		rc = vpci_modern_read_device_8(vpdev, offset, (uint64_t *) buf);
		break;
	default:
		uk_pr_err("%s: device %" __PRIu16 " invalid device config read \
		length %" __PRIu8 " offset %" __PRIu16 "\n", __func__,
		vdev->id.virtio_device_id, type_len, offset);
		return -1;
	}

	return rc;
}

static __u64 vpci_modern_features_get(struct virtio_dev *vdev)
{
	struct virtio_pci_modern_dev *vpdev = NULL;
	__u64  features0, features1;

	UK_ASSERT(vdev);

	vpdev = to_virtio_pci_modern_dev(vdev);
	vpci_modern_write_common_4(vpdev, VIRTIO_MODERN_COMMON_DFSELECT, 0);
	features0 = vpci_modern_read_common_4(vpdev, VIRTIO_MODERN_COMMON_DF);
	vpci_modern_write_common_4(vpdev, VIRTIO_MODERN_COMMON_DFSELECT, 1);
	features1 = vpci_modern_read_common_4(vpdev, VIRTIO_MODERN_COMMON_DF);

	return (((uint64_t) (features1 << 32)) | features0);
}

/**
 * @brief feature negotiation
 *
 * @param vdev
 * @param features
 */
static void vpci_modern_features_set(struct virtio_dev *vdev,
					 __u64 features)
{
	struct virtio_pci_modern_dev *vpdev = NULL;
	uint32_t features0, features1;

	UK_ASSERT(vdev);
	vpdev = to_virtio_pci_modern_dev(vdev);

	/* TODOFS: Mask out features not supported by the virtqueue driver */
	// features = virtqueue_feature_negotiate(features);
	features0 = features;
	features1 = features >> 32;

	vpci_modern_write_common_4(vpdev, VIRTIO_MODERN_COMMON_GFSELECT, 0);
	vpci_modern_write_common_4(vpdev, VIRTIO_MODERN_COMMON_GF, features0);
	vpci_modern_write_common_4(vpdev, VIRTIO_MODERN_COMMON_GFSELECT, 1);
	vpci_modern_write_common_4(vpdev, VIRTIO_MODERN_COMMON_GF, features1);
}

static __u8 vpci_modern_pci_status_get(struct virtio_dev *vdev)
{
	struct virtio_pci_modern_dev *vpdev = NULL;

	UK_ASSERT(vdev);

	vpdev = to_virtio_pci_modern_dev(vdev);
	return (vpci_modern_read_common_1(vpdev,
					 VIRTIO_MODERN_COMMON_STATUS));
}

static void vpci_modern_pci_status_set(struct virtio_dev *vdev,
				       __u8 status)
{
	struct virtio_pci_modern_dev *vpdev = NULL;

	UK_ASSERT(vdev);

	vpdev = to_virtio_pci_modern_dev(vdev);

	if (status != VIRTIO_CONFIG_STATUS_RESET)
		status |= vpci_modern_pci_status_get(vdev);

	vpci_modern_write_common_1(vpdev, VIRTIO_MODERN_COMMON_STATUS,
				    status);
}

static void vpci_modern_pci_dev_reset(struct virtio_dev *vdev)
{
	__u8 status;

	UK_ASSERT(vdev);

	/**
	 * Resetting the device.
	 */
	vpci_modern_pci_status_set(vdev, VIRTIO_CONFIG_STATUS_RESET);

	/**
	 * Waiting for the resetting the device. Find a better way
	 * of doing this instead of repeating register read.
	 *
	 * NOTE! Spec (4.1.4.3.2)
	 * Need to check if we have to wait for the reset to happen.
	 *
	 * TODOFS: spinwait?
	 */
	do {
		status = vpci_modern_pci_status_get(vdev);
	} while (status != VIRTIO_CONFIG_STATUS_RESET);
}


/**
 * @brief
 *
 * @param pdev
 * @param cfg_type
 * @param [out] cap_offset on success, offset inside the device configuration
 * space, where the found capability begins
 * @return int 0, if successfully found
 */
static int virtio_pci_modern_find_cap(struct pci_device *pdev,
				      __u8 target_cfg_type, int *cap_offset)
{
	int rc;
	uint8_t curr_cap, cfg_type, bar;

	UK_ASSERT(pdev);

	for (rc = arch_pci_find_cap(pdev,
				    VIRTIO_PCI_CAP_VENDOR_ID,
				    &curr_cap);
	     rc == 0;
	     rc = arch_pci_find_next_cap(pdev,
	     				 VIRTIO_PCI_CAP_VENDOR_ID,
					 curr_cap,
					 &curr_cap)) {

		/* TODOFS: this is x86 specific code. Need to abstract this
		away and push down into the pci_bus.c or pci_bus.h (probably),
		so that his function can stay architecture-agnostic */
		/* Spec: MUST ignore any vendor-specific capability structure
		 * which has a reserved bar value. */
		PCI_CONF_READ_OFFSET(uint8_t, &bar, pdev->config_addr,
			VIRTIO_PCI_CAP_BAR_OFFSET(curr_cap),
			VIRTIO_PCI_CAP_BAR_SHIFT,
			VIRTIO_PCI_CAP_BAR_MASK);
		if (bar >= VIRTIO_PCI_MODERN_MAX_BARS)
			continue;

		PCI_CONF_READ_OFFSET(uint8_t, &cfg_type, pdev->config_addr,
			VIRTIO_PCI_CAP_CFG_TYPE_OFFSET(curr_cap),
			VIRTIO_PCI_CAP_CFG_TYPE_SHIFT,
			VIRTIO_PCI_CAP_CFG_TYPE_MASK);
		if (cfg_type == target_cfg_type) {
			if (cap_offset != NULL)
				*cap_offset = curr_cap;
			break;
		}
	}

	return rc;
}

// TODOFS: remove?
// static int virtio_pci_modern_probe_configs(struct virtio_pci_modern_dev *vpdev)
// {
// 	int rc = 0;

// 	/*
// 	 * These config capabilities must be present. The DEVICE_CFG
// 	 * capability is only present if the device requires it.
// 	 */

// 	rc = virtio_pci_modern_find_cap(vpdev->pdev,
// 		VIRTIO_PCI_CAP_COMMON_CFG, NULL);
// 	if (unlikely(rc)) {
// 		uk_pr_err("cannot find COMMON_CFG capability\n");
// 		goto exit;
// 	}

// 	rc = virtio_pci_modern_find_cap(vpdev->pdev,
// 		VIRTIO_PCI_CAP_NOTIFY_CFG, NULL);
// 	if (unlikely(rc)) {
// 		uk_pr_err("cannot find NOTIFY_CFG capability\n");
// 		goto exit;
// 	}

// 	rc = virtio_pci_modern_find_cap(vpdev->pdev,
// 		VIRTIO_PCI_CAP_ISR_CFG, NULL);
// 	if (unlikely(rc)) {
// 		uk_pr_err("cannot find ISR_CFG capability\n");
// 		goto exit;
// 	}

// exit:
// 	return rc;
// }

static int
vpci_modern_bar_type(struct virtio_pci_modern_dev *vpdev, int bar)
{
	uint32_t val;

	/*
	 * The BAR described by a config capability may be either an IOPORT or
	 * MEM. Here we determine this type.
	 */
	PCI_CONF_READ_OFFSET(uint32_t, &val, vpdev->pdev->config_addr,
			     PCI_BAR_OFFSET(bar),
			     PCI_BASE_ADDRESS_SHIFT, PCI_BASE_ADDRESS_MASK);
	if (PCI_BAR_IO(val))
		return (SYS_RES_IOPORT);
	else if (PCI_BAR_MEM(val))
	 	return (SYS_RES_MEMORY);
	else {
		uk_pr_err("Device %" __PRIu16 " unknown BAR type %" __PRIu32 \
		"\n", vpdev->vdev.id.virtio_device_id, val);
		return -1;
	}
}

/**
 * @brief Initializes a resource map @p res, that specifies, how a configuration
 * struct, associated with the @p cfg_type capability, can be accessed.
 *
 * It gets the data from a virtio pci capability with type @p cfg_type and
 * copies it over into the resource map @p res.
 * The resource map contains information about how the virtio configuration
 * structure, associated with a capability of type @p cfg_type, can be located.
 * It includes a BAR, through which it is located, a BAR type (memory space or
 * I/O space) and an offset to the structure within the memory, mapped by the
 * BAR.
 *
 * @param vpdev
 * @param cfg_type
 * @param min_size
 * @param alignment
 * @param[out] res
 * @return int
 */
static int
vpci_modern_find_cap_resource(struct virtio_pci_modern_dev *vpdev,
			      uint8_t cfg_type, __s64 min_size, int alignment,
			      struct vpci_modern_resource_map *res)
{
	struct pci_device *pdev;
	int rc = 0;
	int cap_offset; /* offset inside the device config space, where the
			   capability begins */
	uint8_t bar, cap_length;
	uint32_t struct_offset, struct_len;

	UK_ASSERT(vpdev);
	UK_ASSERT(res);

	pdev = vpdev->pdev;

	rc = virtio_pci_modern_find_cap(pdev, cfg_type, &cap_offset);
	if (rc) {
		rc = -ENOATTR;
		goto exit;
	}

	PCI_CONF_READ_OFFSET(uint8_t, &cap_length, pdev->config_addr,
		VIRTIO_PCI_CAP_LEN_OFFSET(cap_offset),
		VIRTIO_PCI_CAP_LEN_SHIFT,
		VIRTIO_PCI_CAP_LEN_MASK);

	if (unlikely(cap_length < sizeof(struct virtio_pci_cap))) {
		uk_pr_err("Capability %" __PRIu8 " length %" __PRIu8 " is \
			   less than expected\n",
			   cfg_type, cap_length);
		return -ENXIO;
	}
	PCI_CONF_READ_OFFSET(uint8_t, &bar, pdev->config_addr,
			VIRTIO_PCI_CAP_BAR_OFFSET(cap_offset),
			VIRTIO_PCI_CAP_BAR_SHIFT,
			VIRTIO_PCI_CAP_BAR_MASK);
	if (unlikely(bar > 0x5)) {
		uk_pr_err("Unsupported bar number %" __PRIu8 "\n", bar);
		return -ENXIO;
	}
	PCI_CONF_READ_OFFSET(uint32_t, &struct_offset, pdev->config_addr,
			VIRTIO_PCI_CAP_S_OFFSET_OFFSET(cap_offset),
			VIRTIO_PCI_CAP_S_OFFSET_SHIFT,
			VIRTIO_PCI_CAP_S_OFFSET_MASK);
	PCI_CONF_READ_OFFSET(uint32_t, &struct_len, pdev->config_addr,
			VIRTIO_PCI_STRUCT_LEN_OFFSET(cap_offset),
			VIRTIO_PCI_STRUCT_LEN_SHIFT,
			VIRTIO_PCI_STRUCT_LEN_MASK);

	if (unlikely(min_size != -1 && struct_len < min_size)) {
		uk_pr_err("Capability %" __PRIu8 " struct length %" __PRIu32 \
			  "less than min %" __PRIs64 "\n",
			  cfg_type, struct_len, min_size);
		return -ENXIO;
	}

	if (unlikely(struct_offset % alignment)) {
		uk_pr_err("Capability %" __PRIu8 " struct offset %" __PRIu32 \
			  " not aligned to %d\n",
			   cfg_type, struct_len, alignment);
		return -ENXIO;
	}

	res->vrm_config_addr = pdev->config_addr;
	res->vrm_cap_offset = cap_offset;
	res->vrm_bar = bar;
	res->vrm_struct_offset = struct_offset;
	res->vrm_struct_len = struct_len;
	res->vrm_type = vpci_modern_bar_type(vpdev, bar);
	if (unlikely(res->vrm_type == -1))
		return -ENXIO;

exit:
	return rc;
}

static int
vpci_modern_map_notify_config(struct virtio_pci_modern_dev *vpdev)
{
	int rc = 0, cap_offset;

	UK_ASSERT(vpdev);

	rc = vpci_modern_find_cap_resource(vpdev, VIRTIO_PCI_CAP_NOTIFY_CFG,
	-1, 2, &vpdev->vpci_notify_res_map);
	if (rc) {
		uk_pr_err("Device %" __PRIu16 " cannot find cap NOTIFY_CFG \
		resource\n", vpdev->vdev.id.virtio_device_id);
		goto exit;
	}

	cap_offset = vpdev->vpci_notify_res_map.vrm_cap_offset;

	PCI_CONF_READ_OFFSET(uint32_t, &vpdev->vpci_notify_offset_multiplier,
	vpdev->pdev->config_addr,
	cap_offset + __offsetof(struct virtio_pci_notify_cap, notify_off_multiplier),
	VIRTIO_NOTIFY_MULTIPLIER_SHIFT,
	VIRTIO_NOTIFY_MULTIPLIER_MASK);

exit:
	return rc;
}

static int
vpci_modern_map_common_config(struct virtio_pci_modern_dev *vpdev)
{
	int rc = 0;

	UK_ASSERT(vpdev);

	rc = vpci_modern_find_cap_resource(vpdev, VIRTIO_PCI_CAP_COMMON_CFG,
		sizeof(struct virtio_pci_common_cfg), 4,
		&vpdev->vpci_common_res_map);
	if (rc) {
		uk_pr_err("Device %" __PRIu16 " cannot find cap COMMON_CFG \
		resource\n", vpdev->vdev.id.virtio_device_id);
		goto exit;
	}

exit:
	return rc;
}

static int
vpci_modern_map_shared_memory(struct virtio_pci_modern_dev *vpdev)
{
	int rc = 0;
	struct vpci_modern_resource_map res;
	uint64_t offset_hi, length_hi;

	UK_ASSERT(vpdev);

	rc = vpci_modern_find_cap_resource(vpdev,
		VIRTIO_PCI_CAP_SHARED_MEMORY_CFG, -1,
		1, &res);
	if (rc == -ENOATTR) {
		/* TODOFS: change to info. (pr_err to make terminal text red) */
		rc = 0;
		uk_pr_err("Device %" __PRIu16 " does not have a shared memory"
		" capability \n", vpdev->vdev.id.virtio_device_id);
		/* Shared memory regions are optional, therefore the
		   capabilities can be not present */
		goto exit;
	}
	if (rc) {
		uk_pr_err("Error while looking for a shared memory"
			  "capability\n");
		goto exit;
	}

	PCI_CONF_READ_OFFSET(uint32_t, &offset_hi, vpdev->pdev->config_addr,
		VIRTIO_PCI_CAP_OFF_HI(res.vrm_cap_offset),
		VIRTIO_PCI_CAP_OFF_HI_SHIFT,
		VIRTIO_PCI_CAP_OFF_HI_MASK);

	PCI_CONF_READ_OFFSET(uint32_t, &length_hi, vpdev->pdev->config_addr,
		VIRTIO_PCI_CAP_LEN_HI(res.vrm_cap_offset),
		VIRTIO_PCI_CAP_LEN_HI_SHIFT,
		VIRTIO_PCI_CAP_LEN_HI_MASK);

	vpdev->vpci_shared_mem_res_map.vrm_config_addr = res.vrm_config_addr;
	vpdev->vpci_shared_mem_res_map.vrm_cap_offset = res.vrm_cap_offset;
	vpdev->vpci_shared_mem_res_map.vrm_bar = res.vrm_bar;
	vpdev->vpci_shared_mem_res_map.vrm_struct_len = res.vrm_struct_len;
	vpdev->vpci_shared_mem_res_map.vrm_type = res.vrm_type;
	vpdev->vpci_shared_mem_res_map.vrm_struct_offset =
		res.vrm_struct_offset;

	vpdev->vpci_shared_mem_res_map.vrm_struct_offset |= (offset_hi << 32);
	vpdev->vpci_shared_mem_res_map.vrm_struct_len |= (length_hi << 32);
	vpdev->shared_mem_present = true;

exit:
	return rc;
}

/**
 * @brief get the address of the beginning of the shared memory region with
 * ID @p shm_id
 *
 * Output is valid only if the vpci_modern_shm_present returns true
 *
 * @param dev
 * @param shm_id
 * @return uint64_t the output is valid only if
 */
static uint64_t
vpci_modern_get_shm_addr(struct virtio_dev *vdev, uint8_t shm_id)
{
	/* TODOFS: in the general case we have to go through all the shared
	   memory capabilities and find the one with the correct ID.
	   in the case of virtiofs, however, there can only be one shared
	   memory region (ID 0), therefore this method suffices for now. */

	struct virtio_pci_modern_dev *vpdev;
	uint64_t base_addr;

	UK_ASSERT(vdev);
	vpdev = to_virtio_pci_modern_dev(vdev);

	if (!vpdev->shared_mem_present) {
		uk_pr_err("Shared memory region with id %" __PRIu8 " is not \
		present \n", shm_id);
		return -1;
	}

	base_addr = vpci_modern_base_addr_get(&vpdev->vpci_shared_mem_res_map);
	base_addr += vpdev->vpci_shared_mem_res_map.vrm_struct_offset;

	return base_addr;
}

static uint64_t
vpci_modern_get_shm_length(struct virtio_dev *vdev, uint8_t shm_id)
{
	/* TODOFS: in the general case we have to go through all the shared
	   memory capabilities and find the one with the correct ID.
	   in the case of virtiofs, however, there can only be one shared
	   memory region (ID 0), therefore this method suffices for now. */

	struct virtio_pci_modern_dev *vpdev;

	UK_ASSERT(vdev);
	vpdev = to_virtio_pci_modern_dev(vdev);

	if (!vpdev->shared_mem_present) {
		uk_pr_err("Shared memory region with id %" __PRIu8 " is not \
		present \n", shm_id);
		return -1;
	}

	return vpdev->vpci_shared_mem_res_map.vrm_struct_len;
}

/**
 * @brief checks if the shared memory region with ID @p shm_id is present.
 *
 * @param dev
 * @param shm_id
 * @return true
 * @return false
 */
static bool
vpci_modern_shm_present(struct virtio_dev *vdev, uint8_t shm_id)
{
	/* TODOFS: in the general case we have to go through all the shared
	   memory capabilities and find the one with the correct ID.
	   in the case of virtiofs, however, there can only be one shared
	   memory region (ID 0), therefore this method suffices for now. */
	(void) shm_id;
	struct virtio_pci_modern_dev *vpdev;

	UK_ASSERT(vdev);
	vpdev = to_virtio_pci_modern_dev(vdev);

	return vpdev->shared_mem_present;
}

static int
vpci_modern_map_isr_config(struct virtio_pci_modern_dev *vpdev) {
	int rc = 0;

	rc = vpci_modern_find_cap_resource(vpdev, VIRTIO_PCI_CAP_ISR_CFG,
	 sizeof(uint8_t), 1, &vpdev->vpci_isr_res_map);
	if (rc) {
		uk_pr_err("Device %" __PRIu16 " cannot find cap ISR_CFG \
		resource\n", vpdev->vdev.id.virtio_device_id);
		goto exit;
	}

exit:
	return rc;

}

static int
vpci_modern_map_device_config(struct virtio_pci_modern_dev *vpdev)
{
	int rc = 0;

	rc = vpci_modern_find_cap_resource(vpdev, VIRTIO_PCI_CAP_DEVICE_CFG,
	-1, 4, &vpdev->vpci_device_res_map);
	/* TODOFS: read up, how to determine, if the Device-specific
	 * configuration is optional for a device.
	 */
	// if (rc == ENOENT) {
	// 	/* Device configuration is optional depending on device. */
	// 	rc = 0;
	// 	goto exit;
	// } else if (rc) {
	/* 	uk_pr_err("Device %" __PRIu16 " cannot find cap DEVICE_CFG \ */
	// 	resource\n", vpdev->vdev.id.virtio_device_id);
	// 	goto exit;
	// }
	if (rc) {
		uk_pr_info("Device %" __PRIu16 " cannot find device-specific  \
		configuration\n", vpdev->vdev.id.virtio_device_id);
		/* indicates that the device-specific configuration is absent */
		vpdev->vpci_device_res_map.vrm_struct_len = 0;
		rc = 0;
		goto exit;
	}

exit:
	return rc;
}

/**
 * @brief some interrupt handler for a virtqueue used in
 * `vpci_legacy_pci_vq_find`. Sends an interrupt to each vq of the `arg`
 * virtio_pci_dev
 *
 * @param arg virtio_pci_dev vpdev from `vpci_legacy_pci_vq_find`
 * @return int
 */
static int virtio_pci_handle(void *arg)
{
	struct virtio_pci_modern_dev *d = (struct virtio_pci_modern_dev *) arg;
	uint8_t isr_status;
	struct virtqueue *vq;
	int rc = 0;

	UK_ASSERT(arg);

	/* Reading the isr status is used to acknowledge the interrupt */
	isr_status = vpci_modern_read_isr_1(d, 0);
	if (isr_status & VIRTIO_PCI_ISR_CONFIG) {
		uk_pr_warn("Unsupported config change interrupt received on virtio-pci device %p\n",
			   d);
	}

	/* Queue has an interrupt */
	if (isr_status & VIRTIO_PCI_ISR_HAS_INTR) {
		/* calls a callback of a vq each and returns its result */
		UK_TAILQ_FOREACH(vq, &d->vdev.vqs, next) {
			virtqueue_ring_interrupt(vq);
			rc = 1;
		}
	}
	return rc;
}

static int vpci_modern_map_configs(struct virtio_pci_modern_dev *vpdev)
{
	int rc = 0;

	rc = vpci_modern_map_common_config(vpdev);
	if (unlikely(rc))
		goto exit;

	rc = vpci_modern_map_notify_config(vpdev);
	if (unlikely(rc))
		goto exit;

	rc = vpci_modern_map_isr_config(vpdev);
	if (unlikely(rc))
		goto exit;

	rc = vpci_modern_map_device_config(vpdev);
	if (unlikely(rc))
		goto exit;

	rc = vpci_modern_map_shared_memory(vpdev);

exit:
	return rc;
}

/**
 * @brief function needed for setting up the virtqueues.
 * (Used in `vpci_modern_vq_setup`)
 *
 * @param vdev
 * @param queue_id
 * @return int
 */
static int vpci_modern_notify(struct virtio_dev *vdev, __u16 queue_id)
{
	struct virtio_pci_modern_dev *vpdev;
	uint16_t q_notify_off;

	UK_ASSERT(vdev);
	vpdev = to_virtio_pci_modern_dev(vdev);
	/* TODOFS: optimization: save this offset on queue initialization inside
	 * the queue struct and pass it to this function. So that we don't have
	 * to access memory here.
	 */
	/* Select virtqueue and read the queue_notify_off */
	vpci_modern_write_common_2(vpdev, VIRTIO_MODERN_COMMON_Q_SELECT, queue_id);
	q_notify_off = vpci_modern_read_common_2(vpdev,
						VIRTIO_MODERN_COMMON_Q_NOFF);

	vpci_modern_write_notify_2(vpdev,
			q_notify_off * vpdev->vpci_notify_offset_multiplier,
			queue_id);

	return 0;
}

/**
 * @brief finds the virtqueues and saves sizes in the `qdesc_size` array.
 * Returns the number of virtqueues found
 *
 * @param vdev
 * @param num_vqs
 * @param[out] qdesc_size array of queue size for each queue.
 * @return int
 */
static int vpci_modern_pci_vq_find(struct virtio_dev *vdev, __u16 num_vqs,
				   __u16 *qdesc_size)
{
	struct virtio_pci_modern_dev *vpdev = NULL;
	int vq_cnt = 0, i = 0, rc = 0;
	uint16_t max_num_vqs;

	UK_ASSERT(vdev);
	vpdev = to_virtio_pci_modern_dev(vdev);

	/* TODOFS: should it be here or somewhere else? */
	max_num_vqs = vpci_modern_read_common_2(vpdev, VIRTIO_MODERN_COMMON_NUMQ);
	if (num_vqs > max_num_vqs) {
		uk_pr_err("num_vqs larger than the max number of vqs supported"
			  "\n");
		return -1;
	}

	/* Registering the interrupt for the device. Function forwards the
	   interrupt to each vq of the device */
	rc = ukplat_irq_register(vpdev->pdev->irq, virtio_pci_handle, vpdev);
	if (rc != 0) {
		uk_pr_err("Failed to register the interrupt\n");
		return rc;
	}

	/* Count the number of queues and get their sizes */
	for (i = 0; i < num_vqs; i++) {
		vpci_modern_write_common_2(vpdev,
					VIRTIO_MODERN_COMMON_Q_SELECT, i);
		qdesc_size[i] = vpci_modern_read_common_2(vpdev,
					VIRTIO_MODERN_COMMON_Q_SIZE);
		if (unlikely(!qdesc_size[i])) {
			uk_pr_err("Virtqueue %d not available\n", i);
			continue;
		}
		vq_cnt++;
	}
	return vq_cnt;
}

/**
 * @brief initialize a virtqueue and append it to the end of the
 * `vdev` vqs (virtualqueues) list
 *
 * @param vdev
 * @param queue_id
 * @param num_desc is the queue_size. Corresponds to the maximum number of
 * 		   buffers in the virtqueue.
 * @param callback comes from a driver, e.g. virtio_9p.c
 * @param a
 * @return struct virtqueue*
 */
static struct virtqueue *vpci_modern_vq_setup(struct virtio_dev *vdev,
					      __u16 queue_id,
					      __u16 num_desc,
					      virtqueue_callback_t callback,
					      struct uk_alloc *a)
{
	struct virtio_pci_modern_dev *vpdev = NULL;
	struct virtqueue *vq;
	struct virtqueue_vring *vrq;
	__paddr_t desc_paddr, avail_paddr, used_paddr;
	long flags;

	UK_ASSERT(vdev != NULL);

	vpdev = to_virtio_pci_modern_dev(vdev);
	/* Allocate and zero virtqueue in contiguous physical memory, on a 4096
	 * byte alignment.*/
	vq = virtqueue_create(queue_id, num_desc, VIRTIO_PCI_VRING_ALIGN,
			      callback, vpci_modern_notify, vdev, a);
	if (PTRISERR(vq)) {
		uk_pr_err("Failed to create the virtqueue: %d\n",
			  PTR2ERR(vq));
		goto err_exit;
	}

	vrq = to_virtqueue_vring(vq);
	/* Physical address of the queue */
	/* TODOFS: refract:
	 * create a function in virtio_ring.c that returns these
	 */
	desc_paddr = ukplat_virt_to_phys(vrq->vring.desc);
	avail_paddr = ukplat_virt_to_phys(vrq->vring.avail);
	used_paddr = ukplat_virt_to_phys(vrq->vring.used);

	/* Select the queue of interest */
	vpci_modern_write_common_2(vpdev,
				VIRTIO_MODERN_COMMON_Q_SELECT, queue_id);
	vpci_modern_write_common_8(vpdev,
				VIRTIO_MODERN_COMMON_Q_DESCLO, desc_paddr);
	vpci_modern_write_common_8(vpdev,
				VIRTIO_MODERN_COMMON_Q_AVAILLO, avail_paddr);
	vpci_modern_write_common_8(vpdev,
				VIRTIO_MODERN_COMMON_Q_USEDLO, used_paddr);

	flags = ukplat_lcpu_save_irqf();
	UK_TAILQ_INSERT_TAIL(&vpdev->vdev.vqs, vq, next);
	ukplat_lcpu_restore_irqf(flags);

err_exit:
	return vq;
}

/**
 * @brief Partially initializes the virtio_pci_dev from the pci_dev,
 * calls the `virtio_pci_legacy_add_dev` method and then registers the
 * device on the virtio bus, using the `virtio_bus_register_device` method.
 *
 * @param pdev
 * @return int
 */
static int virtio_pci_modern_add_dev(struct pci_device *pdev)
{
	struct virtio_pci_modern_dev *vpci_dev = NULL;
	int rc = 0;

	UK_ASSERT(pdev != NULL);

	/* Checking if the device is a modern PCI device*/
	if (unlikely(pdev->id.vendor_id != VENDOR_QUMRANET_VIRTIO)) {
		uk_pr_err("Device %04x does not match the driver\n",
			  pdev->id.device_id);
		rc = -ENXIO;
		goto exit;
	}
	if (unlikely(pdev->id.device_id < VIRTIO_MODERN_ID_START ||
	    pdev->id.device_id > VIRTIO_MODERN_ID_END)) {
		uk_pr_err("Virtio Device %04x does not match the driver\n",
			  pdev->id.device_id);
		rc = -ENXIO;
		goto exit;
	}

	vpci_dev = uk_malloc(a, sizeof(*vpci_dev));
	if (unlikely(!vpci_dev)) {
		uk_pr_err("Failed to allocate virtio-pci device\n");
		return -ENOMEM;
	}
	/* Initializing the virtio pci device */
	vpci_dev->pdev = pdev;
	/* Initializing the virtio device */
	vpci_dev->vdev.id.virtio_device_id =
		pdev->id.device_id - VIRTIO_MODERN_ID_START;
	vpci_dev->vdev.cops = &vpci_modern_ops;
	vpci_dev->vdev.priv = vpci_dev;
	vpci_dev->shared_mem_present = false;

	rc = vpci_modern_map_configs(vpci_dev);
	if (unlikely(rc)) {
		uk_pr_err("Cannot map configs for virtio device %04x \n",
			  pdev->id.device_id);
			  goto free_pci_dev;
	}

	uk_pr_info("Added virtio-pci device %04x\n",
		   pdev->id.device_id);
	uk_pr_info("Added virtio-pci subsystem_device_id %04x\n",
		   pdev->id.subsystem_device_id);

	rc = virtio_bus_register_device(&vpci_dev->vdev);
	if (unlikely(rc != 0)) {
		uk_pr_err("Failed to register the virtio device: %d\n", rc);
		goto free_pci_dev;
	}

exit:
	return rc;

free_pci_dev:
	uk_free(a, vpci_dev);
	goto exit;
}

static int virtio_pci_drv_init(struct uk_alloc *drv_allocator)
{
	/* driver initialization */
	if (!drv_allocator)
		return -EINVAL;

	a = drv_allocator;
	return 0;
}

/* TODOFS: change to accept only modern device IDs */
static const struct pci_device_id virtio_pci_ids[] = {
	{PCI_DEVICE_ID(VENDOR_QUMRANET_VIRTIO, PCI_ANY_ID)},
	/* End of Driver List */
	{PCI_ANY_DEVICE_ID},
};

static struct pci_driver virtio_pci_drv = {
	.device_ids = virtio_pci_ids,
	.init = virtio_pci_drv_init,
	.add_dev = virtio_pci_modern_add_dev
};
PCI_REGISTER_DRIVER(&virtio_pci_drv);
