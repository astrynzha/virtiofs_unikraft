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
 * Adapted from Unikraft 9preq.c
 * unikraft/lib/uk9p/9preq.c
 */

#include "uk/fusereq.h"
#include "uk/assert.h"
#include "uk/essentials.h"
#include "uk/fusedev.h"
#include "uk/print.h"
#include "uk/refcount.h"
#include "uk/wait.h"
#include "uk/arch/atomic.h"
#include <stdint.h>
#include <errno.h>
#include <string.h>
#if CONFIG_LIBUKSCHED
#include <uk/sched.h>
#include <uk/wait.h>
#endif

void uk_fusereq_init(struct uk_fuse_req *req)
{
	UK_INIT_LIST_HEAD(&req->_list);
	uk_refcount_init(&req->refcount, 1);
#if CONFIG_LIBUKSCHED
	uk_waitq_init(&req->wq);
#endif
}

void uk_fusereq_get(struct uk_fuse_req *req)
{
	uk_refcount_acquire(&req->refcount);
}

int uk_fusereq_put(struct uk_fuse_req *req)
{
	int last;

	last = uk_refcount_release(&req->refcount);
	if (last)
		uk_fusedev_req_to_freelist(req->_dev, req);
	return last;
}

int uk_fusereq_receive_cb(struct uk_fuse_req *req, uint32_t recv_size __unused)
{
	if (UK_READ_ONCE(req->state) != UK_FUSEREQ_SENT)
		return -EIO;

	UK_WRITE_ONCE(req->state, UK_FUSEREQ_RECEIVED);

#if CONFIG_LIBUKSCHED
	/* Notify any waiting threads. */
	uk_waitq_wake_up(&req->wq);
#endif

	// TODOFS: implement

	return 0;
}

int uk_fusereq_error(struct uk_fuse_req *req)
{
	int32_t fuse_error = 0;

	UK_ASSERT(req);

	if (UK_READ_ONCE(req->state) != UK_FUSEREQ_RECEIVED)
		return -EIO;

	if (req->out_buffer_size == 0) /* No reply was expected */
		goto end;

	fuse_error = ((struct fuse_out_header *) req->out_buffer)->error;
	if (fuse_error) {
		uk_pr_err("FUSE reply error code: %" __PRIs32 " (%s) "
		"\n", fuse_error, strerror(-fuse_error));
		return fuse_error;
	}

end:
	return 0;
}

int uk_fusereq_waitreply(struct uk_fuse_req *req)
{
	int rc;

#if CONFIG_LIBUKSCHED
	uk_waitq_wait_event(&req->wq, req->state == UK_FUSEREQ_RECEIVED);
#else
	while (UK_READ_ONCE(req->state) != UK_FUSEREQ_RECEIVED)
		;
#endif

	rc = uk_fusereq_error(req);

	return rc;
}
