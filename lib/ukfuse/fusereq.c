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

	if (!req->out_buffer) /* No reply was expected */
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
