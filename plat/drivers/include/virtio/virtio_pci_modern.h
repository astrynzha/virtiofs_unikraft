/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Andrii Strynzha <a.strynzha@gmail.com>,
 *
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

/* SPDX-License-Identifier: ISC */
/*
 * Authors: Dan Williams
 *          Costin Lupu <costin.lupu@cs.pub.ro>
 *          Sharan Santhanam <sharan.santhanam@neclab.eu>
 *
 * Copyright (c) 2015, IBM
 * Copyright (c) 2018, NEC Europe Ltd., NEC Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software
 * for any purpose with or without fee is hereby granted, provided
 * that the above copyright notice and this permission notice appear
 * in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * Taken and adapted from solo5 virtio_pci.h
 * kernel/virtio/virtio_pci.h
 * Commit-id: 6e0e12133aa7
 */

 /*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright IBM Corp. 2007
 *
 * Authors:
 *  Anthony Liguori  <aliguori@us.ibm.com>
 *
 * This header is BSD licensed so anyone can use the definitions to implement
 * compatible drivers/servers.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of IBM nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL IBM OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
/** Taken and adopted from FreeBSD virtio_pci_modern_var.h
 * /sys/dev/virtio/pci/virtio_pci_modern_var.h
 * Commit-id: 5c4c96d
 */

/**
 * This header contains structures and variables needed to drive a modern
 * virtio device.
 *
 * They are specified in the official virtio pecification.
 * https://github.com/oasis-tcs/virtio-spec
 * This file abides by Version 1.2 of the Specification
 */

#ifndef __PLAT_DRV_VIRTIO_PCI_MODERN_H__
#define __PLAT_DRV_VIRTIO_PCI_MODERN_H__

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus __ */

#include <uk/arch/types.h>
#include <virtio/virtio_types.h>
#include <uk/list.h>
#include <pci/pci_bus.h>


// TODOFS:  refactoring: change the offset/shift/mask suffixes to o/s/m

#define PCI_BAR_OFFSET(bar)		(PCI_BASE_ADDRESS_0 + 4 * (bar))

/* The PCI virtio capability header: */
struct virtio_pci_cap {
	uint8_t vndr;		/* Generic PCI field: PCI_CAP_ID_VNDR */
	uint8_t next;		/* Generic PCI field: pointer to the next
				 * capability. 0 if this is the last one */
	uint8_t cap_len;	/* Generic PCI field: capability length */
	uint8_t cfg_type;	/* Identifies the structure. */
	uint8_t bar;		/* Where to find it. */
	uint8_t id;		/* Multiple capabilities of the same type */
	uint8_t padding[2];	/* Pad to full dword. */
	uint32_t struct_offset;	/* Offset within bar. */
	uint32_t struct_len;	/* Length of the structure, described by this
				 * capability, in bytes. */
};

#define VIRTIO_PCI_CAP_VENDOR_ID		0x09 /* Identifies a virtio capability */
#define VIRTIO_PCI_MODERN_MAX_BARS		6

/* Macro versions of offsets: */
#define VIRTIO_PCI_CAP_LEN_OFFSET(capreg)	(capreg)
#define VIRTIO_PCI_CAP_LEN_SHIFT		16
#define VIRTIO_PCI_CAP_LEN_MASK			0x000000FF

#define VIRTIO_PCI_CAP_CFG_TYPE_OFFSET(capreg)	(capreg)
#define VIRTIO_PCI_CAP_CFG_TYPE_SHIFT		24
#define VIRTIO_PCI_CAP_CFG_TYPE_MASK		0x000000FF

#define VIRTIO_PCI_CAP_BAR_OFFSET(capreg)	(capreg+4)
#define VIRTIO_PCI_CAP_BAR_SHIFT		0
#define VIRTIO_PCI_CAP_BAR_MASK			0x000000FF

#define VIRTIO_PCI_CAP_S_OFFSET_OFFSET(capreg)	(capreg+8)
#define VIRTIO_PCI_CAP_S_OFFSET_SHIFT		0
#define VIRTIO_PCI_CAP_S_OFFSET_MASK		0xFFFFFFFF

#define VIRTIO_PCI_STRUCT_LEN_OFFSET(capreg)	(capreg+12)
#define VIRTIO_PCI_STRUCT_LEN_SHIFT		0
#define VIRTIO_PCI_STRUCT_LEN_MASK		0xFFFFFFFF

#define VIRTIO_PCI_CAP_OFF_HI(capreg)		(capreg+16)
#define VIRTIO_PCI_CAP_OFF_HI_SHIFT		0
#define VIRTIO_PCI_CAP_OFF_HI_MASK		0xFFFFFFFF

#define VIRTIO_PCI_CAP_LEN_HI(capreg)		(capreg+20)
#define VIRTIO_PCI_CAP_LEN_HI_SHIFT		0
#define VIRTIO_PCI_CAP_LEN_HI_MASK		0xFFFFFFFF




/* Capability IDs, found in the @cfg_type field in the
 * @virtio_pci_cap structure
 */
/* Common configuration */
#define VIRTIO_PCI_CAP_COMMON_CFG 1
/* Notifications */
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2
/* ISR Status */
#define VIRTIO_PCI_CAP_ISR_CFG 3
/* Device specific configuration */
#define VIRTIO_PCI_CAP_DEVICE_CFG 4
/* PCI configuration access */
#define VIRTIO_PCI_CAP_PCI_CFG 5
/* Shared memory region */
#define VIRTIO_PCI_CAP_SHARED_MEMORY_CFG 8
/* Vendor-specific data */
#define VIRTIO_PCI_CAP_VENDOR_CFG 9

/* Fields in VIRTIO_PCI_CAP_COMMON_CFG: */
struct virtio_pci_common_cfg {
	/* About the whole device. */
	__virtio_le32 device_feature_select;	/* read-write */
	__virtio_le32 device_feature;		/* read-only for driver */
	__virtio_le32 driver_feature_select;	/* read-write */
	__virtio_le32 driver_feature;		/* read-write */
	__virtio_le32 config_msix_vector;	/* read-write */
	__virtio_le32 num_queues;		/* read-only for driver */
	__u8 device_status;			/* read-write */
	__u8 config_generation;			/* read-only for driver */

	/* About a specific virtqueue. */
	__virtio_le16 queue_select;		/* read-write */
	__virtio_le16 queue_size;		/* read-write */
	__virtio_le16 queue_msix_vector;	/* read-write */
	__virtio_le16 queue_enable;		/* read-write */
	__virtio_le16 queue_notify_off;		/* read-only for driver */
	__virtio_le64 queue_desc;		/* read-write */
	__virtio_le64 queue_driver;		/* read-write */
	__virtio_le64 queue_device;		/* read-write */
	__virtio_le16 queue_notify_data;	/* read-only for driver */
	__virtio_le16 queue_reset;		/* read-write */
};

/* Fields in VIRTIO_PCI_CAP_PCI_CFG capability: */
struct virtio_pci_cfg_cap {
	struct virtio_pci_cap cap;
	__u8 pci_cfg_data[4]; /* Data for BAR access. */
};

/* Fields in VIRTIO_PCI_CAP_NOTIFY_CFG capability: */
struct virtio_pci_notify_cap {
	struct virtio_pci_cap cap;
	__virtio_le32 notify_off_multiplier; /* Multiplier for queue_notify_off. */
};

#define VIRTIO_NOTIFY_MULTIPLIER_SHIFT	0
#define VIRTIO_NOTIFY_MULTIPLIER_MASK	0xFFFFFFFF


#define VENDOR_QUMRANET_VIRTIO		(0x1AF4)
#define VIRTIO_ID_START			(0x1000)
#define VIRTIO_LEGACY_ID_START		(VIRTIO_PCI_ID_START)
#define VIRTIO_LEGACY_ID_END		(0x103F)
#define VIRTIO_MODERN_ID_START		(0x1040)
#define VIRTIO_MODERN_ID_END		(0x107F)
#define VIRTIO_ID_END			(VIRTIO_PCI_MODERN_ID_END)


#define VIRTIO_MODERN_CAP_VNDR		0
#define VIRTIO_MODERN_CAP_NEXT		1
#define VIRTIO_MODERN_CAP_LEN		2
#define VIRTIO_MODERN_CAP_CFG_TYPE	3
#define VIRTIO_MODERN_CAP_BAR		4
#define VIRTIO_MODERN_CAP_OFFSET	8
#define VIRTIO_MODERN_CAP_LENGTH	12

#define VIRTIO_MODERN_NOTIFY_CAP_MULT	16

#define VIRTIO_MODERN_COMMON_DFSELECT	0
#define VIRTIO_MODERN_COMMON_DF		4
#define VIRTIO_MODERN_COMMON_GFSELECT	8
#define VIRTIO_MODERN_COMMON_GF		12
#define VIRTIO_MODERN_COMMON_MSIX	16
#define VIRTIO_MODERN_COMMON_NUMQ	18
#define VIRTIO_MODERN_COMMON_STATUS	20
#define VIRTIO_MODERN_COMMON_CFGGEN	21
#define VIRTIO_MODERN_COMMON_Q_SELECT	22
#define VIRTIO_MODERN_COMMON_Q_SIZE	24
#define VIRTIO_MODERN_COMMON_Q_MSIX	26
#define VIRTIO_MODERN_COMMON_Q_ENABLE	28
#define VIRTIO_MODERN_COMMON_Q_NOFF	30
#define VIRTIO_MODERN_COMMON_Q_DESCLO	32
#define VIRTIO_MODERN_COMMON_Q_DESCHI	36
#define VIRTIO_MODERN_COMMON_Q_AVAILLO	40
#define VIRTIO_MODERN_COMMON_Q_AVAILHI	44
#define VIRTIO_MODERN_COMMON_Q_USEDLO	48
#define VIRTIO_MODERN_COMMON_Q_USEDHI	52



#define VIRTIO_HOST_FEATURES		0    /* 32-bit r/o */
#define VIRTIO_GUEST_FEATURES		4    /* 32-bit r/w */
#define VIRTIO_QUEUE_PFN		8    /* 32-bit r/w */
#define VIRTIO_QUEUE_SIZE		12   /* 16-bit r/o */
#define VIRTIO_QUEUE_SEL		14   /* 16-bit r/w */
#define VIRTIO_QUEUE_NOTIFY		16   /* 16-bit r/w */

#define SYS_RES_MEMORY			3    /* i/o memory */
#define SYS_RES_IOPORT			4    /* i/o ports */

/*
 * Shift size used for writing physical queue address to QUEUE_PFN
 */
#define VIRTIO_PCI_QUEUE_ADDR_SHIFT     12

/*
 * The status register lets us tell the device where we are in
 * initialization
 */
#define VIRTIO_PCI_STATUS               18   /* 8-bit r/w */

/*
 * Reading the value will return the current contents of the interrupt
 * status register and will also clear it.  This is effectively a
 * read-and-acknowledge.
 */
#define VIRTIO_PCI_ISR                  19   /* 8-bit r/o */
#define VIRTIO_PCI_ISR_HAS_INTR         0x1  /* interrupt is for this device */
#define VIRTIO_PCI_ISR_CONFIG           0x2  /* config change bit */

/* TODO Revisit when adding MSI support. */
#define VIRTIO_PCI_CONFIG_OFF           20
#define VIRTIO_PCI_VRING_ALIGN          4096

#ifdef __cplusplus
}
#endif /* __cplusplus __ */

#endif /* __PLAT_DRV_VIRTIO_PCI_MODERN_H__ */
