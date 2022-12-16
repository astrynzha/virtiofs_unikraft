/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Author: Andrii Strynzha <a.strynzha@gmail.com>
 * 	   Cristian Banu <cristb@gmail.com>
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
/**
 * Adapted from Unikraft 9pdev.c
 * unikraft/lib/uk9p/9pdev.c
 */

#include "uk/fusedev.h"
#include "uk/fusedev_core.h"
#include "uk/fusedev_trans.h"
#include "uk/arch/spinlock.h"
#include "uk/fusereq.h"
#include <uk/plat/spinlock.h>
#include <uk/alloc.h>
#include <uk/errptr.h>
#ifdef CONFIG_LIBUKSCHED
#include <uk/wait.h>
#endif

static void _req_mgmt_init(struct uk_fusedev_req_mgmt *req_mgmt)
{
	ukarch_spin_init(&req_mgmt->spinlock);
	UK_INIT_LIST_HEAD(&req_mgmt->req_list);
	UK_INIT_LIST_HEAD(&req_mgmt->req_free_list);
}

static void _req_mgmt_req_to_freelist_locked(struct uk_fusedev_req_mgmt *req_mgmt,
					     struct uk_fuse_req *req)
{
	uk_list_add(&req->_list, &req_mgmt->req_free_list);
}

void uk_fusedev_req_to_freelist(struct uk_fuse_dev *dev,
				struct uk_fuse_req *req)
{
	unsigned long flags;

	if (!dev)
		return;

	ukplat_spin_lock_irqsave(&dev->_req_mgmt.spinlock, flags);
	_req_mgmt_req_to_freelist_locked(&dev->_req_mgmt, req);
	ukplat_spin_unlock_irqrestore(&dev->_req_mgmt.spinlock, flags);
}

static void _req_mgmt_add_req_locked(struct uk_fusedev_req_mgmt *req_mgmt,
				struct uk_fuse_req *req)
{
	uk_list_add(&req->_list, &req_mgmt->req_list);
}

static struct uk_fuse_req *
_req_mgmt_from_freelist_locked(struct uk_fusedev_req_mgmt *req_mgmt)
{
	struct uk_fuse_req *req;

	if (uk_list_empty(&req_mgmt->req_free_list))
		return NULL;

	req = uk_list_first_entry(&req_mgmt->req_free_list,
			struct uk_fuse_req, _list);
	uk_list_del(&req->_list);

	return req;
}

static void _req_mgmt_del_req_locked(struct uk_fusedev_req_mgmt *req_mgmt
				     __unused, struct uk_fuse_req *req)
{
	uk_list_del(&req->_list);
}

static void _req_mgmt_cleanup(struct uk_fusedev_req_mgmt *req_mgmt)
{
	unsigned long flags;
	struct uk_fuse_req *req, *reqn;

	ukplat_spin_lock_irqsave(&req_mgmt->spinlock, flags);
	uk_list_for_each_entry_safe(req, reqn, &req_mgmt->req_list, _list) {
		_req_mgmt_del_req_locked(req_mgmt, req);
		if (!uk_fusereq_put(req)) {
			/* If in the future these references get released, mark
			 * _dev as NULL so uk_fusedev_req_to_freelist doesn't
			 * attempt to place them in an invalid memory region.
			 *
			 * As _dev is not used for any other purpose, this
			 * doesn't impact any other logic related to 9p request
			 * processing.
			 */
			req->_dev = NULL;
			/* TODOFS: print an error message here
			uk_pr_err("Tag %d still has references on cleanup.\n",
				tag);
			*/
		}
	}
	uk_list_for_each_entry_safe(req, reqn, &req_mgmt->req_free_list,
			_list) {
		uk_list_del(&req->_list);
		uk_free(req->_a, req);
	}
	ukplat_spin_unlock_irqrestore(&req_mgmt->spinlock, flags);
}

struct uk_fuse_req *uk_fusedev_req_create(struct uk_fuse_dev *dev)
{
	struct uk_fuse_req *req;
	int rc = 0;
	unsigned long flags;

	UK_ASSERT(dev);

	ukplat_spin_lock_irqsave(&dev->_req_mgmt.spinlock, flags);
	if (!(req = _req_mgmt_from_freelist_locked(&dev->_req_mgmt))) {
		/* Don't allocate with the spinlock held. */
		ukplat_spin_unlock_irqrestore(&dev->_req_mgmt.spinlock, flags);
		req = uk_calloc(dev->a, 1, sizeof(*req));
		if (req == NULL) {
			rc = -ENOMEM;
			goto out;
		}
		req->_dev = dev;
		/*
		 * Duplicate this, instead of using req->_dev, as we can't rely
		 * on the value of _dev at time of free. Check comment in
		 * _req_mgmt_cleanup.
		 */
		req->_a = dev->a;
		ukplat_spin_lock_irqsave(&dev->_req_mgmt.spinlock, flags);
	}

	uk_fusereq_init(req);

	UK_ASSERT(req->_dev == dev);

	_req_mgmt_add_req_locked(&dev->_req_mgmt, req);
	ukplat_spin_unlock_irqrestore(&dev->_req_mgmt.spinlock, flags);

	req->state = UK_FUSEREQ_INITIALIZED;

	return req;

out:
	return ERR2PTR(rc);
}

int uk_fusedev_req_remove(struct uk_fuse_dev *dev, struct uk_fuse_req *req)
{
	unsigned long flags;

	ukplat_spin_lock_irqsave(&dev->_req_mgmt.spinlock, flags);
	_req_mgmt_del_req_locked(&dev->_req_mgmt, req);
	ukplat_spin_unlock_irqrestore(&dev->_req_mgmt.spinlock, flags);

	return uk_fusereq_put(req);
}

int uk_fusedev_request(struct uk_fuse_dev *dev, struct uk_fuse_req *req)
{
	int rc;

	UK_ASSERT(dev);
	UK_ASSERT(req);

	if (UK_READ_ONCE(req->state) != UK_FUSEREQ_READY) {
		return -EINVAL;
	}

	if (dev->state != UK_FUSEDEV_CONNECTED) {
		return -EIO;
	}

	/* -ENOSPC is returned, if not enough descriptors are available on a
	   virtqueue */
#if CONFIG_LIBUKSCHED
	uk_waitq_wait_event(&dev->xmit_wq,
		(rc = dev->ops->request(dev, req)) != -ENOSPC);
#else
	do {
		/*
		 * Retry the request while it has no space available on the
		 * transport layer.
		 */
		rc = dev->ops->request(dev, req);
	} while (rc == -ENOSPC);
#endif

	return rc;
}

void uk_fusedev_xmit_notify(struct uk_fuse_dev *dev)
{
	#if CONFIG_LIBUKSCHED
	uk_waitq_wake_up(&dev->xmit_wq);
	#endif
}

struct uk_fuse_dev *uk_fusedev_connect(const struct uk_fusedev_trans *trans,
				       const char *device_identifier,
				       struct uk_alloc *a)
{
	struct uk_fuse_dev *dev;
	int rc = 0;

	UK_ASSERT(trans);

	if (a == NULL)
		a = trans->a;

	dev = uk_calloc(a, 1, sizeof(*dev));
	if (dev == NULL) {
		return ERR2PTR(-ENOMEM);
	}

	INIT_FUSE_DEV(dev);

	dev->ops = trans->ops;
	dev->a = a;

#if CONFIG_LIBUKSCHED
	uk_waitq_init(&dev->xmit_wq);
#endif

	_req_mgmt_init(&dev->_req_mgmt);
	rc = dev->ops->connect(dev, device_identifier);
	if (rc < 0)
		goto free_dev;

	dev->state = UK_FUSEDEV_CONNECTED;

	return dev;

free_dev:
	_req_mgmt_cleanup(&dev->_req_mgmt);
	uk_free(a, dev);
	return ERR2PTR(rc);
}


int uk_fusedev_disconnect(struct uk_fuse_dev *dev)
{
	int rc = 0;

	UK_ASSERT(dev);
	UK_ASSERT(dev->state == UK_FUSEDEV_CONNECTED);

	dev->state = UK_FUSEDEV_DISCONNECTING;

	/* Clean up the requests before closing the channel. */
	_req_mgmt_cleanup(&dev->_req_mgmt);

	/*
	 * Even if the disconnect from the transport layer fails, the memory
	 * allocated for the FUSE device is freed.
	 */
	rc = dev->ops->disconnect(dev);

	uk_free(dev->a, dev);
	return rc;
}