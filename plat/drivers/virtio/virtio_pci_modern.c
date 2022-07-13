/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Sharan Santhanam <sharan.santhanam@neclab.eu>,
 *	    Andrii Strynzha <a.strynzha@gmail.com>
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

/* Driver for the modern VirtIO PCI interface. */

#include "uk/assert.h"
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
#include <virtio/virtio_pci_modern.h>
#include <pci/pci_bus.h>

static struct uk_alloc *a;


/**
 * TODOFS: think how to change this.
 *
 */
struct virtio_pci_modern_dev {
	/* Virtio Device */
	struct virtio_dev vdev;
	/* Address that lies in the I/O BAR0 */
	__u64 pci_base_addr;
	/* ISR Address Range */
	__u64 pci_isr_addr;
	/* PCI device information */
	struct pci_device *pdev;
};

/**
 * Fetch the virtio pci information from the virtio device.
 * @param vdev
 *	Reference to the virtio device.
 */
#define to_virtiopcimoderndev(vdev) \
		__containerof(vdev, struct virtio_pci_modern_dev, vdev)

// TODOFS: declare all functions at the beginning of the file
/**
 * Configuration operations legacy functions (`virtio_config_ops`)
 */
static void vpci_modern_pci_dev_reset(struct virtio_dev *vdev);
static int vpci_legacy_pci_config_set(struct virtio_dev *vdev, __u16 offset,
				      const void *buf, __u32 len);
static int vpci_legacy_pci_config_get(struct virtio_dev *vdev, __u16 offset,
				      void *buf, __u32 len, __u8 type_len);
static __u64 vpci_legacy_pci_features_get(struct virtio_dev *vdev);
static void vpci_legacy_pci_features_set(struct virtio_dev *vdev,
					 __u64 features);
static int vpci_legacy_pci_vq_find(struct virtio_dev *vdev, __u16 num_vq,
				   __u16 *qdesc_size);
static void vpci_legacy_pci_status_set(struct virtio_dev *vdev, __u8 status);
static __u8 vpci_legacy_pci_status_get(struct virtio_dev *vdev);
static struct virtqueue *vpci_legacy_vq_setup(struct virtio_dev *vdev,
					      __u16 queue_id,
					      __u16 num_desc,
					      virtqueue_callback_t callback,
					      struct uk_alloc *a);
static void vpci_legacy_vq_release(struct virtio_dev *vdev,
		struct virtqueue *vq, struct uk_alloc *a);
/**
 * @brief some interrupt handler for a virtqueue used in
 * `vpci_legacy_pci_vq_find`. Sends an interrupt to each vq of the `arg`
 * virtio_pci_dev
 *
 * @param arg virtio_pci_dev vpdev from `vpci_legacy_pci_vq_find`
 * @return int
 */
static int virtio_pci_handle(void *arg);
static int vpci_legacy_notify(struct virtio_dev *vdev, __u16 queue_id);
static int virtio_pci_modern_probe_configs(struct pci_device *pci_dev);
static int virtio_pci_modern_add_dev(struct pci_device *pci_dev);

/**
 * Configuration operations legacy PCI device.
 */
static struct virtio_config_ops vpci_legacy_ops = {
	.device_reset = vpci_modern_pci_dev_reset,
	.config_get   = vpci_legacy_pci_config_get,
	.config_set   = vpci_legacy_pci_config_set,
	.features_get = vpci_legacy_pci_features_get,
	.features_set = vpci_legacy_pci_features_set,
	.status_get   = vpci_legacy_pci_status_get,
	.status_set   = vpci_legacy_pci_status_set,
	.vqs_find     = vpci_legacy_pci_vq_find,
	.vq_setup     = vpci_legacy_vq_setup,
	.vq_release   = vpci_legacy_vq_release,
};

/**
 * @brief function needed for setting up the virtqueues.
 * (Used in `vpci_legacy_vq_setup`)
 *
 * @param vdev
 * @param queue_id
 * @return int
 */
static int vpci_legacy_notify(struct virtio_dev *vdev, __u16 queue_id)
{
	struct virtio_pci_modern_dev *vpdev;

	UK_ASSERT(vdev);
	vpdev = to_virtiopcimoderndev(vdev);
	virtio_cwrite16((void *)(unsigned long) vpdev->pci_base_addr,
			VIRTIO_PCI_QUEUE_NOTIFY, queue_id);

	return 0;
}

static int virtio_pci_handle(void *arg)
{
	struct virtio_pci_modern_dev *d = (struct virtio_pci_modern_dev *) arg;
	uint8_t isr_status;
	struct virtqueue *vq;
	int rc = 0;

	UK_ASSERT(arg);

	/* Reading the isr status is used to acknowledge the interrupt */
	isr_status = virtio_cread8((void *)(unsigned long)d->pci_isr_addr, 0);
	/* We don't support configuration interrupt on the device */
	if (isr_status & VIRTIO_PCI_ISR_CONFIG) {
		uk_pr_warn("Unsupported config change interrupt received on virtio-pci device %p\n",
			   d);
	}

	/* Queue has an interrupt */
	if (isr_status & VIRTIO_PCI_ISR_HAS_INTR) {
		/* calls a callback of a vq each and returns its result */
		UK_TAILQ_FOREACH(vq, &d->vdev.vqs, next) {
			rc |= virtqueue_ring_interrupt(vq);
		}
	}
	return rc;
}

/**
 * @brief initialize a virtqueue and append it to the end of the
 * `vdev` vqs (virtualqueues) list
 *
 * @param vdev
 * @param queue_id
 * @param num_desc
 * @param callback comes from a driver, e.g. virtio_9p.c
 * @param a
 * @return struct virtqueue*
 */
static struct virtqueue *vpci_legacy_vq_setup(struct virtio_dev *vdev,
					      __u16 queue_id,
					      __u16 num_desc,
					      virtqueue_callback_t callback,
					      struct uk_alloc *a)
{
	struct virtio_pci_modern_dev *vpdev = NULL;
	struct virtqueue *vq;
	__paddr_t addr;
	long flags;

	UK_ASSERT(vdev != NULL);

	vpdev = to_virtiopcimoderndev(vdev);
	vq = virtqueue_create(queue_id, num_desc, VIRTIO_PCI_VRING_ALIGN,
			      callback, vpci_legacy_notify, vdev, a);
	if (PTRISERR(vq)) {
		uk_pr_err("Failed to create the virtqueue: %d\n",
			  PTR2ERR(vq));
		goto err_exit;
	}

	/* Physical address of the queue */
	addr = virtqueue_physaddr(vq);
	/* Select the queue of interest */
	virtio_cwrite16((void *)(unsigned long)vpdev->pci_base_addr,
			VIRTIO_PCI_QUEUE_SEL, queue_id);
	virtio_cwrite32((void *)(unsigned long)vpdev->pci_base_addr,
			VIRTIO_PCI_QUEUE_PFN,
			addr >> VIRTIO_PCI_QUEUE_ADDR_SHIFT);

	flags = ukplat_lcpu_save_irqf();
	UK_TAILQ_INSERT_TAIL(&vpdev->vdev.vqs, vq, next);
	ukplat_lcpu_restore_irqf(flags);

err_exit:
	return vq;
}

static void vpci_legacy_vq_release(struct virtio_dev *vdev,
		struct virtqueue *vq, struct uk_alloc *a)
{
	struct virtio_pci_modern_dev *vpdev = NULL;
	long flags;

	UK_ASSERT(vq != NULL);
	UK_ASSERT(a != NULL);
	vpdev = to_virtiopcimoderndev(vdev);

	/* Select and deactivate the queue */
	virtio_cwrite16((void *)(unsigned long)vpdev->pci_base_addr,
			VIRTIO_PCI_QUEUE_SEL, vq->queue_id);
	virtio_cwrite32((void *)(unsigned long)vpdev->pci_base_addr,
			VIRTIO_PCI_QUEUE_PFN, 0);

	flags = ukplat_lcpu_save_irqf();
	UK_TAILQ_REMOVE(&vpdev->vdev.vqs, vq, next);
	ukplat_lcpu_restore_irqf(flags);

	virtqueue_destroy(vq, a);
}

/**
 * @brief finds the virtqueues and saves sizes in the `qdesc_size` array.
 * Returns the number of virtqueues found
 *
 * @param vdev
 * @param num_vqs
 * @param qdesc_size array that comes from the driver
 * @return int
 */
static int vpci_legacy_pci_vq_find(struct virtio_dev *vdev, __u16 num_vqs,
				   __u16 *qdesc_size)
{
	struct virtio_pci_modern_dev *vpdev = NULL;
	int vq_cnt = 0, i = 0, rc = 0;

	UK_ASSERT(vdev);
	vpdev = to_virtiopcimoderndev(vdev);

	/* Registering the interrupt for the device. Function forwards the
	   interrupt to each vq of the device */
	rc = ukplat_irq_register(vpdev->pdev->irq, virtio_pci_handle, vpdev);
	if (rc != 0) {
		uk_pr_err("Failed to register the interrupt\n");
		return rc;
	}

	/* Count the number of queues and get their sizes */
	for (i = 0; i < num_vqs; i++) {
		virtio_cwrite16((void *) (unsigned long)vpdev->pci_base_addr,
				VIRTIO_PCI_QUEUE_SEL, i);
		qdesc_size[i] = virtio_cread16(
				(void *) (unsigned long)vpdev->pci_base_addr,
				VIRTIO_PCI_QUEUE_SIZE);
		if (unlikely(!qdesc_size[i])) {
			uk_pr_err("Virtqueue %d not available\n", i);
			continue;
		}
		vq_cnt++;
	}
	return vq_cnt;
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
static int vpci_legacy_pci_config_set(struct virtio_dev *vdev, __u16 offset,
				      const void *buf, __u32 len)
{
	struct virtio_pci_modern_dev *vpdev = NULL;

	UK_ASSERT(vdev);
	vpdev = to_virtiopcimoderndev(vdev);

	_virtio_cwrite_bytes((void *)(unsigned long)vpdev->pci_base_addr,
			     VIRTIO_PCI_CONFIG_OFF + offset, buf, len, 1);

	return 0;
}

static int vpci_legacy_pci_config_get(struct virtio_dev *vdev, __u16 offset,
				      void *buf, __u32 len, __u8 type_len)
{
	struct virtio_pci_modern_dev *vpdev = NULL;
	int rc = 0;

	UK_ASSERT(vdev);
	vpdev = to_virtiopcimoderndev(vdev);

	/* Reading an entity less than 4 bytes are atomic */
	if (type_len == len && type_len <= 4) {
		_virtio_cread_bytes(
				(void *) (unsigned long)vpdev->pci_base_addr,
				VIRTIO_PCI_CONFIG_OFF + offset, buf, len,
				type_len);
	} else {
		rc = virtio_cread_bytes_many(
				(void *) (unsigned long)vpdev->pci_base_addr,
				VIRTIO_PCI_CONFIG_OFF + offset,	buf, len);
		if (rc != (int)len)
			return -EFAULT;
	}

	return 0;
}

static __u8 vpci_legacy_pci_status_get(struct virtio_dev *vdev)
{
	struct virtio_pci_modern_dev *vpdev = NULL;

	UK_ASSERT(vdev);
	vpdev = to_virtiopcimoderndev(vdev);
	return virtio_cread8((void *) (unsigned long) vpdev->pci_base_addr,
			     VIRTIO_PCI_STATUS);
}

static void vpci_legacy_pci_status_set(struct virtio_dev *vdev, __u8 status)
{
	struct virtio_pci_modern_dev *vpdev = NULL;
	__u8 curr_status = 0;

	/* Reset should be performed using the reset interface */
	UK_ASSERT(vdev || status != VIRTIO_CONFIG_STATUS_RESET);

	vpdev = to_virtiopcimoderndev(vdev);
	curr_status = vpci_legacy_pci_status_get(vdev);
	status |= curr_status;
	virtio_cwrite8((void *)(unsigned long) vpdev->pci_base_addr,
		       VIRTIO_PCI_STATUS, status);
}

static void vpci_modern_pci_dev_reset(struct virtio_dev *vdev)
{
	struct virtio_pci_modern_dev *vpdev = NULL;
	__u8 status;

	UK_ASSERT(vdev);

	vpdev = to_virtiopcimoderndev(vdev);
	/**
	 * Resetting the device.
	 */
	virtio_cwrite8((void *) (unsigned long)vpdev->pci_base_addr,
		       20, VIRTIO_CONFIG_STATUS_RESET);
	/**
	 * Waiting for the resetting the device. Find a better way
	 * of doing this instead of repeating register read.
	 *
	 * NOTE! Spec (4.1.4.3.2)
	 * Need to check if we have to wait for the reset to happen.
	 */
	do {
		status = virtio_cread8(
				(void *)(unsigned long)vpdev->pci_base_addr,
				20);
	} while (status != VIRTIO_CONFIG_STATUS_RESET);
}

static __u64 vpci_legacy_pci_features_get(struct virtio_dev *vdev)
{
	struct virtio_pci_modern_dev *vpdev = NULL;
	__u64  features;

	UK_ASSERT(vdev);

	vpdev = to_virtiopcimoderndev(vdev);
	features = virtio_cread32((void *) (unsigned long)vpdev->pci_base_addr,
				  VIRTIO_PCI_HOST_FEATURES);
	return features;
}

/**
 * @brief feature negotiation
 *
 * @param vdev
 * @param features
 */
static void vpci_legacy_pci_features_set(struct virtio_dev *vdev,
					 __u64 features)
{
	struct virtio_pci_modern_dev *vpdev = NULL;

	UK_ASSERT(vdev);
	vpdev = to_virtiopcimoderndev(vdev);
	/* Mask out features not supported by the virtqueue driver */
	features = virtqueue_feature_negotiate(features);
	virtio_cwrite32((void *) (unsigned long)vpdev->pci_base_addr,
			VIRTIO_PCI_GUEST_FEATURES, (__u32)features);
}



/**
 * @brief
 *
 * @param pci_dev
 * @param cfg_type
 * @param [out] cap_offset
 * @return int
 */
static int virtio_pci_modern_find_cap(struct pci_device *pci_dev,
				      __u8 target_cfg_type, int *cap_offset)
{
	int rc;
	uint8_t curr_cap, cfg_type, bar;

	UK_ASSERT(pci_dev);

	for (rc = arch_pci_find_cap(pci_dev,
				    VIRTIO_PCI_CAP_VENDOR_ID,
				    &curr_cap);
	     rc == 0;
	     rc = arch_pci_find_next_cap(pci_dev,
	     				 VIRTIO_PCI_CAP_VENDOR_ID,
					 curr_cap,
					 &curr_cap)) {

		/* Spec: MUST ignore any vendor-specific capability structure
		 * which has a reserved bar value. */
		PCI_CONF_READ_OFFSET(uint8_t, &bar, pci_dev->config_addr,
			VIRTIO_PCI_CAP_BAR_OFFSET(curr_cap),
			VIRTIO_PCI_CAP_BAR_SHIFT,
			VIRTIO_PCI_CAP_BAR_MASK);
		if (bar >= VIRTIO_PCI_MODERN_MAX_BARS)
			continue;

		PCI_CONF_READ_OFFSET(uint8_t, &cfg_type, pci_dev->config_addr,
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

static int virtio_pci_modern_probe_configs(struct pci_device *pci_dev)
{
	int rc = 0;

	/*
	 * These config capabilities must be present. The DEVICE_CFG
	 * capability is only present if the device requires it.
	 */

	rc = virtio_pci_modern_find_cap(pci_dev,
		VIRTIO_PCI_CAP_COMMON_CFG, NULL);
	if (rc) {
		uk_pr_err("cannot find COMMON_CFG capability\n");
		goto exit;
	}

	rc = virtio_pci_modern_find_cap(pci_dev,
		VIRTIO_PCI_CAP_NOTIFY_CFG, NULL);
	if (rc) {
		uk_pr_err("cannot find NOTIFY_CFG capability\n");
		goto exit;
	}

	rc = virtio_pci_modern_find_cap(pci_dev,
		VIRTIO_PCI_CAP_ISR_CFG, NULL);
	if (rc) {
		uk_pr_err("cannot find ISR_CFG capability\n");
		goto exit;
	}

exit:
	return rc;
}

/**
 * @brief Partially initializes the virtio_pci_dev from the pci_dev,
 * calls the `virtio_pci_legacy_add_dev` method and then registers the
 * device on the virtio bus, using the `virtio_bus_register_device` method.
 *
 * @param pci_dev
 * @return int
 */
static int virtio_pci_modern_add_dev(struct pci_device *pci_dev)
{
	struct virtio_pci_modern_dev *vpci_dev = NULL;
	int rc = 0;

	UK_ASSERT(pci_dev != NULL);

	/* Checking if the device is a modern PCI device*/
	if (pci_dev->id.vendor_id != VENDOR_QUMRANET_VIRTIO) {
		uk_pr_err("Device %04x does not match the driver\n",
			  pci_dev->id.device_id);
		rc = -ENXIO;
		goto exit;
	}
	if (pci_dev->id.device_id < VIRTIO_PCI_MODERN_ID_START ||
	    pci_dev->id.device_id > VIRTIO_PCI_MODERN_ID_END) {
		uk_pr_err("Virtio Device %04x does not match the driver\n",
			  pci_dev->id.device_id);
		rc = -ENXIO;
		goto exit;
	}
	if (virtio_pci_modern_probe_configs(pci_dev) != 0) {
		rc = -ENXIO;
		goto exit;
	}

	vpci_dev = uk_malloc(a, sizeof(*vpci_dev));
	if (!vpci_dev) {
		uk_pr_err("Failed to allocate virtio-pci device\n");
		return -ENOMEM;
	}
	/* Initializing the virtio pci device */
	vpci_dev->pdev = pci_dev;
	/* Initializing the virtio pci device */
	vpci_dev->vdev.id.virtio_device_id =
		pci_dev->id.device_id - VIRTIO_PCI_MODERN_ID_START;
	vpci_dev->vdev.cops = &vpci_legacy_ops;

	uk_pr_info("Added virtio-pci device %04x\n",
		   pci_dev->id.device_id);
	uk_pr_info("Added virtio-pci subsystem_device_id %04x\n",
		   pci_dev->id.subsystem_device_id);

	rc = virtio_bus_register_modern_device(&vpci_dev->vdev);
	if (rc != 0) {
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
