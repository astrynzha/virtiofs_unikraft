/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Andrii Strynzha <a.strynzha@gmail.com>
 * 	    Cristian Banu <cristb@gmail.com>
 *
 * Copyright (c) 2019, Karlsruhe Institute of Technology (KIT).
 *               All rights reserved.
 * Copyright (c) 2019, University Politehnica of Bucharest. All rights reserved.
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


#include "errno.h"
#include "uk/alloc.h"
#include "uk/arch/atomic.h"
#include "uk/arch/lcpu.h"
#include "uk/arch/spinlock.h"
#include "uk/assert.h"
#include "uk/essentials.h"
#include "uk/fuse_i.h"
#include "uk/print.h"
#include "virtio/virtqueue.h"
#include <stdint.h>
#include <string.h>
#include <virtio/virtio_bus.h>
#include <virtio/virtio_fs.h>
#include <virtio/virtio_ids.h>
#include <virtio/virtio_config.h>
#include <virtio/virtio_types.h>
#include <uk/sglist.h>
#include <uk/plat/spinlock.h>
#include <uk/fuse.h>
#include <uk/fusedev.h>
#include <uk/fusereq.h>
#include <stdbool.h>
#include <uk/vf_vnops.h>
#include <uk/bench_tests.h>
/* TODOFS: remove */



#define DRIVER_NAME	"virtio-fs"
static struct uk_alloc *a;

/* List of initialized virtio fs devices. */
static UK_LIST_HEAD(virtio_fs_device_list);
static __spinlock virtio_fs_device_list_lock;

/**
 * @brief Holds information for communication with a single virtio-fs device,
 * which runs on host.
 *
 * Virtio-fs device is a device that runs on hosts and with which this driver
 * communicates.
 * The struct holds information that facilitates the communication between this
 * driver and a device.
 * Multiple virtio-fs devices may run on host. Each one is to be described by a
 * separate struct.
 */
struct virtio_fs_device {
	/* Virtio device. */
	struct virtio_dev *vdev;
	/* Name associated with this file system.. */
	char *tag;
	/* Total number of request virtqueues exposed by device */
	__virtio_le32 num_request_queues;
	/* Entry within the virtio devices' list. */
	struct uk_list_head _list;
	/* Virtqueue references. */
	struct virtqueue *vq_hiprio;
	struct virtqueue *vq_notify;
	struct virtqueue **vq_req;
	/* Hw queue identifier. */
	uint16_t hwvq_id;
	/* libukfuse associated device (NULL if the device is not in use). */
	struct uk_fuse_dev *fusedev;
	/* Scatter-gather list. */
	struct uk_sglist sg;
	//TODOFS: change
	struct uk_sglist_seg sgsegs[128];
	/* Spinlock protecting the sg list and the vq. */
	__spinlock spinlock;
};

static int virtio_fs_connect(struct uk_fuse_dev *fusedev,
			     const char *device_identifier)
{
	struct virtio_fs_device *dev = NULL;
	int rc = 0;
	int found = 0;

	/*
	Look for dev with tag==device_identifier
	virtio_fs_device_list is a list of uninitialized virtio-fs devices.
	`dev` with the needed tag is kept in the `dev` variable after
	the loop.
	 */
	ukarch_spin_lock(&virtio_fs_device_list_lock);
	uk_list_for_each_entry(dev, &virtio_fs_device_list, _list) {
		if (!strcmp(dev->tag, device_identifier)) {
			if (dev->fusedev != NULL) {
				rc = -EBUSY;
				goto out;
			}
			found = 1;
			break;
		}
	}

	if (!found) {
		rc = -EINVAL;
		goto out;
	}

	dev->fusedev = fusedev;
	fusedev->priv = dev;

out:
	ukarch_spin_unlock(&virtio_fs_device_list_lock);
	return rc;
}

static int virtio_fs_disconnect(struct uk_fuse_dev *fusedev)
{
	struct virtio_fs_device *dev;

	UK_ASSERT(fusedev);
	dev = fusedev->priv;

	/* Lock to prevent race conditions on fusedev
	   with virtio_fs_connect */
	ukarch_spin_lock(&virtio_fs_device_list_lock);
	dev->fusedev = NULL;
	ukarch_spin_unlock(&virtio_fs_device_list_lock);

	return 0;
}

/**
 * @brief
 *
 *
 * @param vq
 * @param priv
 * @return int
 */
static int virtio_fs_recv(struct virtqueue *vq, void *priv)
{
	struct virtio_fs_device *dev;
	uint32_t len;
	struct uk_fuse_req *req = NULL;
	int rc = 0;
	int handled = 0;

	UK_ASSERT(vq);
	UK_ASSERT(priv);

	dev = priv;

	while (1) {
		/*
		 * Protect against data races with virtio_9p_request() calls
		 * which are trying to enqueue to the same vq.
		 */
		ukarch_spin_lock(&dev->spinlock);
		rc = virtqueue_buffer_dequeue(vq, (void **)&req, &len);
		ukarch_spin_unlock(&dev->spinlock);
		if (rc < 0)
			break;

		uk_pr_debug("Reply received: unique: %" __PRIu64 ", opcode: "
			"%" __PRIu32 ", data length: %" __PRIu32" \n",
			((struct fuse_in_header *) req->in_buffer)->unique,
			((struct fuse_in_header *) req->in_buffer)->opcode,
			len);

		/*Notify the FUSE API that this request has been successfully
		 * received.
		 */
		uk_fusereq_receive_cb(req, len);

		/* TODOFS: req management? : */
		uk_fusereq_put(req);
		handled = 1;

		/* Received not what expected (usually less). */
		if (len != req->out_buffer_size) {
			/* Not always an error. Used for debugging bc the text
			is red */
			uk_pr_err("out_buffer_size is %"__PRIu32 ", but "
			"received %" __PRIu32 " bytes.\n", req->out_buffer_size,
			len);
			break;
		}

		/* Break if there are no more buffers on the virtqueue. */
		if (rc == 0)
			break;
	}

	/*
	 * As the virtqueue might have empty slots now, notify any threads
	 * blocked on ENOSPC errors.
	 */
	if (handled)
		uk_fusedev_xmit_notify(dev->fusedev);

	return handled;
}

/**
 * @brief
 *
 * @param fuse_dev
 * @param req
 * @return int -ENOSPC, if not enough descriptors are available on a vring.
 */
static int virtio_fs_request(struct uk_fuse_dev *fuse_dev,
			     struct uk_fuse_req *req)
{
	struct virtio_fs_device *dev;
	unsigned long flags;
	bool failed = false;
	size_t read_segs, write_segs = 0;
	int host_notified = 0;
	int rc = 0;

	UK_ASSERT(fuse_dev);
	UK_ASSERT(req);
	UK_ASSERT(req->in_buffer);

	uk_fusereq_get(req);
	dev = fuse_dev->priv;
	/*
	* Protect against data races with virtio_fs_recv() calls
	* which are trying to dequeue from the same vq.
	*/
	ukplat_spin_lock_irqsave(&dev->spinlock, flags);
	uk_sglist_reset(&dev->sg);

	rc = uk_sglist_append(&dev->sg, req->in_buffer,
			      req->in_buffer_size);
	if (rc < 0) {
		failed = true;
		goto out_unlock;
	}

	read_segs = dev->sg.sg_nseg;

	/* no reply expected, when req->out_buffer == 0 */
	if (req->out_buffer_size == 0)
		goto skip_outbuf;

	rc = uk_sglist_append(&dev->sg, req->out_buffer,
			req->out_buffer_size);
	if (rc < 0) {
		failed = true;
		goto out_unlock;
	}

	write_segs = dev->sg.sg_nseg - read_segs;

skip_outbuf:

	/* TODOFS: write vq management code (i.e., which req virtqueue to use)*/
	rc = virtqueue_buffer_enqueue(dev->vq_req[0], req, &dev->sg,
				      read_segs, write_segs);
	if (likely(rc >= 0)) {
		UK_WRITE_ONCE(req->state, UK_FUSEREQ_SENT);
		uk_pr_debug("Sending request: unique: %" __PRIu64 ", opcode: %"
			__PRIu32 ", nodeid: %" __PRIu64 ", pid %" __PRIu32 "\n",
			((struct fuse_in_header *) req->in_buffer)->unique,
			((struct fuse_in_header *) req->in_buffer)->opcode,
			((struct fuse_in_header *) req->in_buffer)->nodeid,
			((struct fuse_in_header *) req->in_buffer)->pid);

		virtqueue_host_notify(dev->vq_req[0]);
		host_notified = 1;
		rc = 0;
	}

out_unlock:
	if (failed)
		uk_pr_err(DRIVER_NAME": Failed to append to the sg list.\n");
	ukplat_spin_unlock_irqrestore(&dev->spinlock, flags);

	/*
	 * Release the reference to the FUSE request if it was not successfully
	 * sent.
	 */
	if (!host_notified)
		uk_fusereq_put(req);

	return rc;
}

static const struct uk_fusedev_trans_ops viofs_trans_ops = {
	.connect		= virtio_fs_connect,
	.disconnect		= virtio_fs_disconnect,
	.request		= virtio_fs_request
};

/**
 * @brief FUSE transport
 *
 * Interface and data needed for lib/ukfuse to use this driver as transport
 */
static struct uk_fusedev_trans viofs_trans = {
	.name			= "virtio",
	.ops			= &viofs_trans_ops,
	.a			= NULL /* Set by the driver initialization. */
};

static inline void virtio_fs_feature_set(struct virtio_fs_device *d)
{
	d->vdev->features = 0;
	// VIRTIO_FEATURE_SET(d->vdev->features, VIRTIO_9P_F_MOUNT_TAG);
	VIRTIO_FEATURE_SET(d->vdev->features, VIRTIO_F_VERSION_1);
}

static inline int virtio_fs_scan_device_config(struct virtio_fs_device *d)
{
	/* get tag */
	for (int i = 0; i < 9; i++) {
		if (0 > virtio_modern_config_get(d->vdev,
				__offsetof(struct virtio_fs_config, tag) + i*4,
				&d->tag[i*4], 4)) {
			uk_pr_err(DRIVER_NAME": Failed to read the tag on the\
			device %p\n", d);
			return -1;
		}
		// if (d->tag[i*4 + 3] == '\0')
		// 	break;
	}
	d->tag[36] = '\0';

	if (0 > virtio_modern_config_get(d->vdev,
		__offsetof(struct virtio_fs_config, num_request_queues),
		&d->num_request_queues, 4)) {
		uk_pr_err(DRIVER_NAME": Failed to read num_request_queues on \
		the device %p\n", d);
		return -1;
	}

	return 0;
}

static int virtio_fs_feature_negotiate(struct virtio_fs_device *d)
{
	__u64 host_features;
	uint8_t status;
	int rc = 0;

	host_features = virtio_feature_get(d->vdev);

	d->tag = uk_calloc(a, 37, sizeof(*d->tag));
	if (unlikely(!d->tag)) {
		rc = -ENOMEM;
		goto out;
	}

	rc = virtio_fs_scan_device_config(d);
	if (rc) {
		rc = -EAGAIN;
		goto free_mem;
	}

	d->vdev->features &= host_features;

	virtio_feature_set(d->vdev, d->vdev->features);
	rc = virtio_dev_status_update(d->vdev, VIRTIO_CONFIG_S_FEATURES_OK);
	if (rc)
		goto free_mem;

	status = virtio_dev_status_get(d->vdev);
	if ((status & VIRTIO_CONFIG_S_FEATURES_OK) == 0) {
		uk_pr_err("Desired features were not accepted.\n");
		rc = -ENOTSUP;
		goto free_mem;
	}
	return 0;

free_mem:
	uk_free(a, d->tag);
out:
	return rc;
}

static int virtio_fs_vq_alloc(struct virtio_fs_device *d)
{
	__virtio_le32 vq_avail = 0;
	__u16 qdesc_size[d->num_request_queues + 1];
	int rc = 0;

	vq_avail = virtio_find_vqs(d->vdev,
				d->num_request_queues + 1, qdesc_size);
	if (unlikely(vq_avail != d->num_request_queues + 1)) {
		uk_pr_err(DRIVER_NAME": Expected %" __PRIvirtio_le32 " queues,\
			  found %" __PRIvirtio_le32 "\n",
			  d->num_request_queues + 1, vq_avail);
		return -ENOMEM;
	}
	/* TODOFS: where to free? */
	d->vq_req = uk_calloc(a, d->num_request_queues,
				sizeof(struct virtqueue *));
	if (!d->vq_req)
		return -ENOMEM;

	uk_sglist_init(&d->sg, ARRAY_SIZE(d->sgsegs), &d->sgsegs[0]);

	/* Initialize the hiprio virtqueue first */
	d->vq_hiprio = virtio_vqueue_setup(d->vdev,
					   VIRTIO_FS_HIPRIO_QUEUE_ID,
					   qdesc_size[0],
					   virtio_fs_recv, a);
	if (unlikely(PTRISERR(d->vq_hiprio))) {
		uk_pr_err(DRIVER_NAME": Failed to set up the hiprio virtqueue \
			%"__PRIu16"\n", 0);
		rc = PTR2ERR(d->vq_hiprio);
		goto free_mem;
	}
	d->vq_hiprio->priv = d;
	// TODOFS: not 100% sure, if enabling is required. It might be optional
	virtio_vqueue_enable(d->vdev, d->vq_hiprio);

	/* Initialize the request virtqueues */
	for (uint16_t i = 0; i < d->num_request_queues; i++) {
		d->vq_req[i] = virtio_vqueue_setup(d->vdev, i+1,
						   qdesc_size[i+1],
						   virtio_fs_recv, a);
		if (unlikely(PTRISERR(d->vq_req[i]))) {
			uk_pr_err(DRIVER_NAME": Failed to set up a request \
				virtqueue %"__PRIu16"\n", i);
			rc = PTR2ERR(d->vq_req[i]);
			goto free_mem;
		}
		d->vq_req[i]->priv = d;
	// TODOFS: not 100% sure, if enabling is required. It might be optional
		virtio_vqueue_enable(d->vdev, d->vq_req[i]);
	}

	return 0;

free_mem:
	uk_free(a, d->vq_req);
	return rc;
}

static int virtio_fs_configure(struct virtio_fs_device *d)
{
	int rc = 0;

	rc = virtio_fs_feature_negotiate(d);
	if (rc != 0) {
		uk_pr_err(DRIVER_NAME": Failed to negotiate the device feature %d\n",
			rc);
		rc = -EINVAL;
		goto out_status_fail;
	}
	rc = virtio_fs_vq_alloc(d);
	if (rc) {
		uk_pr_err(DRIVER_NAME": Could not allocate virtqueue\n");
		goto out_status_fail;
	}

	uk_pr_info(DRIVER_NAME": Configured: features=0x%lx tag=%s\n",
			d->vdev->features, d->tag);
out:
	return rc;

out_status_fail:
	virtio_dev_status_update(d->vdev, VIRTIO_CONFIG_STATUS_FAIL);
	goto out;
}

static int virtio_fs_start(struct virtio_fs_device *d)
{
	virtqueue_intr_enable(d->vq_hiprio);
	for (__virtio_le32 i = 0; i < d->num_request_queues; i++)
		virtqueue_intr_enable(d->vq_req[i]);
	virtio_dev_drv_up(d->vdev);
	uk_pr_info(DRIVER_NAME": %s started\n", d->tag);

	return 0;
}

static int virtio_fs_drv_init(struct uk_alloc *drv_allocator)
{
	int rc = 0;

	if (unlikely(!drv_allocator)) {
		rc = -EINVAL;
		goto out;
	}

	a = drv_allocator;
	viofs_trans.a = drv_allocator;

	rc = uk_fusedev_trans_register(&viofs_trans);

out:
	return rc;
}

void test_method_1() {
	test_method();
}


static int virtio_fs_add_dev(struct virtio_dev *vdev)
{
	struct virtio_fs_device *d;
	int rc = 0;

	UK_ASSERT(vdev != NULL);

	d = uk_calloc(a, 1, sizeof(*d));
	if (!d) {
		rc = -ENOMEM;
		goto out;
	}
	ukarch_spin_init(&d->spinlock);
	d->vdev = vdev;
	virtio_fs_feature_set(d);
	rc = virtio_fs_configure(d);
	if (rc)
		goto out_free;
	rc = virtio_fs_start(d);
	if (rc)
		goto out_free;

	/* TODOFS:
	* 3.1.1. set the FAILED status bit, if irrecoverable failure
	*/
	ukarch_spin_lock(&virtio_fs_device_list_lock);
	uk_list_add(&d->_list, &virtio_fs_device_list);
	ukarch_spin_unlock(&virtio_fs_device_list_lock);


	add_vdev_for_dax(vdev);
	// test_method_1();
	// vf_test_method();
	bench_test();
out:
	return rc;
out_free:
	uk_free(a, d);
	goto out;
}

static const struct virtio_dev_id vfs_dev_id[] = {
	{VIRTIO_ID_FS},
	{VIRTIO_ID_INVALID} /* List Terminator */
};

static struct virtio_driver vfs_drv = {
	.dev_ids = vfs_dev_id,
	.init    = virtio_fs_drv_init,
	.add_dev = virtio_fs_add_dev
};

/* Registers driver in the virtio_driver_list in virtio_bus.c */
VIRTIO_BUS_REGISTER_DRIVER(&vfs_drv);
/*
static void libkvmvirtiofs_virtio_register_driver(void) {
    _virtio_bus_register_driver((&vfs_drv));

}
static const uk_ctor_func_t __attribute__((used)) __attribute__((section(".uk_ctortab" "1")))
 __uk_ctortab1_libkvmvirtiofs_virtio_register_driver = (libkvmvirtiofs_virtio_register_driver)

*/