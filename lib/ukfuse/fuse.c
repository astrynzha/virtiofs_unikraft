/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Author: Andrii Strynzha <a.strynzha@gmail.com>
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
/*
 * Copyright (C) 2019-2020 Red Hat, Inc.
 *
 * Written By: Gal Hammer <ghammer@redhat.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met :
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and / or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of their contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/**
 * Adapted from virtio-win virtiofs.cpp
 * kvm-guest-drivers-windows/viofs/svc/virtiofs.cpp
 * Commit-id: d4713c5
 */

#include "uk/fuse.h"
#include <fcntl.h>
#include "uk/essentials.h"
#include "uk/alloc.h"
#include "uk/assert.h"
#include "uk/fusedev.h"
#include "uk/fuse_i.h"
#include "uk/fusedev_core.h"
#include "uk/fusereq.h"
#include "uk/fusedev_trans.h"
#include "uk/print.h"
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>
#include <uk/arch/atomic.h>
#include <uk/errptr.h>
#include <uk/process.h>
#include <uk/essentials.h>
#include <stdlib.h>
/* TODOFS: remove */
#include <uk/vf_vnops.h>


/**
 * @brief returns @p base to the power of @p exp
 *
 * TODOFS: remove, if Unikraft has it's own power function
 *
 */
static inline uint32_t bpow(uint32_t base, uint32_t exp)
{
	uint32_t result = 1;
	for (;;)
	{
		if (exp & 1)
			result *= base;
		exp >>= 1;
		if (!exp)
			break;
		base *= base;
	}

	return result;
}

/* TODO: Temporary declaration. To remove, when getpid is added to nolibc */
pid_t getpid(void);

struct virtio_fs_data {
	uint32_t max_pages;
	uint32_t max_write;
};

static int64_t getUniqueIdentifier()
{
	static int64_t uniq = 1;

	return ukarch_inc(&uniq);
}

static void FUSE_HEADER_INIT(struct fuse_in_header *hdr, uint32_t opcode,
    uint64_t nodeid, uint32_t datalen)
{
	hdr->len = sizeof(*hdr) + datalen;
	hdr->opcode = opcode;
	hdr->unique = getUniqueIdentifier();
	hdr->nodeid = nodeid;
	hdr->uid = 1000;
	hdr->gid = 1000;
	hdr->pid = getpid();
}

static inline int send_and_wait(struct uk_fuse_dev *dev,
				struct uk_fuse_req *req)
{
	int rc;

	UK_WRITE_ONCE(req->state, UK_FUSEREQ_READY);

	if ((rc = uk_fusedev_request(dev, req)))
		return rc;

	if ((rc = uk_fusereq_waitreply(req)))
		return rc;

	return 0;
}


/**
 * @brief
 *
 * @param dev
 * @param nodeid
 * @param fh
 * @param fsync_flags use FUSE_FSYNC_FDATASYNC if only data is to be synced
 * (no metadata). Otherwise set fsync_flags to 0.
 * @return int
 */
int uk_fuse_request_fsync(struct uk_fuse_dev *dev, bool is_dir,
			  uint64_t nodeid, uint64_t fh, uint32_t flags)
{
	int rc = 0;
	FUSE_FSYNC_IN fsync_in = {0};
	FUSE_FSYNC_OUT fsync_out = {0};
	struct uk_fuse_req *req;

	UK_ASSERT(dev);

	FUSE_HEADER_INIT(&fsync_in.hdr, is_dir ? FUSE_FSYNCDIR : FUSE_FSYNC,
		nodeid, sizeof(struct fuse_fsync_in));

	fsync_in.fsync.fh =  fh;
	fsync_in.fsync.fsync_flags = flags;

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	req->in_buffer = &fsync_in;
	req->in_buffer_size = sizeof(fsync_in);
	req->out_buffer = &fsync_out;
	req->out_buffer_size = sizeof(fsync_out);

	if ((rc = send_and_wait(dev, req)))
		goto free;

	uk_fusedev_req_remove(dev, req);
	return 0;

free:
	uk_fusedev_req_remove(dev, req);
	return rc;
}

/**
 * @brief removemapping request to release more than one mapping with one
 * request.
 *
 * The caller has to initialize all the fuse_removemapping_one structs inside
 * the @p removemapping_in. In total, at least
 * @p removemapping_one_cnt pieces have to be initialized.
 *
 * fuse_in_header and fuse_removemapping_in in @p removemapping_in
 * don't have to be initialized by the caller.
 *
 * @param dev
 * @param one_arr array of fuse_removemapping_one's
 * @param one_arr_len
 * @return int
 */
int uk_fuse_request_removemapping_multiple(struct uk_fuse_dev *dev,
					FUSE_REMOVEMAPPING_IN *removemapping_in,
					size_t removemapping_one_cnt)
{
	int rc = 0;
	FUSE_REMOVEMAPPING_OUT removemapping_out = {0};
	struct uk_fuse_req *req;
	uint64_t datalen;

	UK_ASSERT(dev);

	datalen = sizeof(struct fuse_removemapping_in)
		+ sizeof(struct fuse_removemapping_one) * removemapping_one_cnt;

	/* Zero the memory of fuse_in_header and fuse_removemapping_in and
	   reinitialize it here to make sure it is done correctly */
	memset(removemapping_in, 0, sizeof(struct fuse_in_header)
		+ sizeof(struct fuse_removemapping_in));
	/* nodeid is irrelevant for the current implementation of
	   FUSE_REMOVEMAPPING */
	FUSE_HEADER_INIT(&removemapping_in->hdr, FUSE_REMOVEMAPPING,
		0, datalen);

	removemapping_in->removemapping_in.count = removemapping_one_cnt;

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		rc = PTR2ERR(req);

	req->in_buffer = removemapping_in;
	req->in_buffer_size = sizeof(struct fuse_in_header) + datalen;
	req->out_buffer = &removemapping_out;
	req->out_buffer_size = sizeof(removemapping_out);

	if ((rc = send_and_wait(dev, req)))
		goto free;


	uk_fusedev_req_remove(dev, req);
	return 0;

free:
	uk_fusedev_req_remove(dev, req);
	return rc;
}

int uk_fuse_request_removemapping_legacy(struct uk_fuse_dev *dev,
					 uint64_t nodeid, uint64_t fh,
					 uint64_t moffset, uint64_t len)
{
	int rc = 0;
	FUSE_REMOVEMAPPING_IN_LEGACY removemapping_in = {0};
	FUSE_REMOVEMAPPING_OUT removemapping_out = {0};
	struct uk_fuse_req *req;

	UK_ASSERT(dev);

	FUSE_HEADER_INIT(&removemapping_in.hdr, FUSE_REMOVEMAPPING, nodeid,
		sizeof(struct fuse_removemapping_in_legacy));

	removemapping_in.removemapping_in.fh = fh;
	removemapping_in.removemapping_in.moffset = moffset;
	removemapping_in.removemapping_in.len = len;

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	req->in_buffer = &removemapping_in;
	req->in_buffer_size = sizeof(FUSE_REMOVEMAPPING_IN_LEGACY);
	req->out_buffer = &removemapping_out;
	req->out_buffer_size = sizeof(FUSE_REMOVEMAPPING_OUT);

	if ((rc = send_and_wait(dev, req)))
		goto free;

	uk_fusedev_req_remove(dev, req);
	return 0;

free:
	uk_fusedev_req_remove(dev, req);
	return rc;



}

/**
 * @brief removemappnig request for one fuse_removemapping_one region
 *
 * @param dev
 * @param moffset
 * @param len
 * @return int
 */
int uk_fuse_request_removemapping(struct uk_fuse_dev *dev, uint64_t moffset,
				     uint64_t len)
{
	int rc = 0;
	FUSE_REMOVEMAPPING_IN *removemapping_in;
	FUSE_REMOVEMAPPING_OUT removemapping_out = {0};
	struct uk_fuse_req *req;
	uint64_t datalen;

	UK_ASSERT(dev);

	datalen = sizeof(struct fuse_removemapping_in)
			+ sizeof(struct fuse_removemapping_one);
	removemapping_in = calloc(1, sizeof(struct fuse_in_header)
			   + datalen);
	if (!removemapping_in) {
		uk_pr_err("calloc failed\n");
		return -1;
	}

	FUSE_HEADER_INIT(&removemapping_in->hdr, FUSE_REMOVEMAPPING,
			 0, datalen);

	removemapping_in->removemapping_in.count = 1;
	removemapping_in->removemapping_one[0].moffset = moffset;
	removemapping_in->removemapping_one[0].len = len;

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req)) {
		rc = PTR2ERR(req);
		goto free_calloc;
	}

	req->in_buffer = removemapping_in;
	req->in_buffer_size = sizeof(struct fuse_in_header) + datalen;
	req->out_buffer = &removemapping_out;
	req->out_buffer_size = sizeof(removemapping_out);

	if ((rc = send_and_wait(dev, req)))
		goto free;

	uk_fusedev_req_remove(dev, req);
	free(removemapping_in);
	return 0;

free:
	uk_fusedev_req_remove(dev, req);
free_calloc:
	free(removemapping_in);
	return rc;
}

/**
 * @brief
 *
 * The @p foffset and @p moffset have to be a multiple of dev->map_alignment
 *
 * @param dev
 * @param nodeid
 * @param fh
 * @param foffset
 * @param len
 * @param flags
 * @param moffset
 * @return int
 */
int uk_fuse_request_setupmapping(struct uk_fuse_dev *dev, uint64_t nodeid,
				 uint64_t fh, uint64_t foffset, uint64_t len,
				 uint64_t flags, uint64_t moffset)
{
	int rc = 0;
	FUSE_SETUPMAPPING_IN setupmapping_in = {0};
	FUSE_SETUPMAPPING_OUT setupmapping_out = {0};
	struct uk_fuse_req *req;

	UK_ASSERT(dev);
	UK_ASSERT(dev->map_alignment);
	UK_ASSERT(foffset % dev->map_alignment == 0);
	UK_ASSERT(moffset % dev->map_alignment == 0);

	FUSE_HEADER_INIT(&setupmapping_in.hdr, FUSE_SETUPMAPPING,
			 nodeid, sizeof(struct fuse_setupmapping_in));

	setupmapping_in.setupmapping.fh = fh;
	setupmapping_in.setupmapping.flags = flags;
	setupmapping_in.setupmapping.foffset = foffset;
	setupmapping_in.setupmapping.len = len;
	setupmapping_in.setupmapping.moffset = moffset;

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	req->in_buffer = &setupmapping_in;
	req->in_buffer_size = sizeof(setupmapping_in);
	req->out_buffer = &setupmapping_out;
	req->out_buffer_size = sizeof(setupmapping_out);

	if ((rc = send_and_wait(dev, req)))
		goto free;

	uk_fusedev_req_remove(dev, req);
	return 0;

free:
	uk_fusedev_req_remove(dev, req);
	return rc;
}

/**
 * @brief
 *
 * As far as I understand it, this is used for hole detection and not for
 * repositioning the file offset within an open file descriptor.
 * (Reading/writing from an offset is done by indicating @p file_off in
 * uk_fuse_request_read()/uk_fuse_request_write().
 *
 * @param dev
 * @param nodeid
 * @param fh
 * @param offset
 * @param whence
 * @param[out] total_offset upon successful completion the resulting offset
 * location as measured in bytes from the beginning of the file is returned
 * @return int
 */
int uk_fuse_request_lseek(struct uk_fuse_dev *dev, uint64_t nodeid, uint64_t fh,
			  uint64_t offset, uint32_t whence,
			  off_t *offset_out)
{
	int rc = 0;
	FUSE_LSEEK_IN lseek_in = {0};
	FUSE_LSEEK_OUT lseek_out = {0};
	struct uk_fuse_req *req;

	UK_ASSERT(dev);

	FUSE_HEADER_INIT(&lseek_in.hdr, FUSE_LSEEK, nodeid,
			 sizeof(lseek_in.lseek));

	lseek_in.lseek.fh = fh;
	lseek_in.lseek.offset = offset;
	lseek_in.lseek.whence = whence;

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	req->in_buffer = &lseek_in;
	req->in_buffer_size = sizeof(lseek_in);
	req->out_buffer = &lseek_out;
	req->out_buffer_size = sizeof(lseek_out);

	if ((rc = send_and_wait(dev, req)))
		goto exit;

	*offset_out = lseek_out.lseek.offset;

exit:
	uk_fusedev_req_remove(dev, req);
	return rc;
}

/**
 * @brief
 *
 * READDIRPLUS = READDIR + LOOKUP in one.
 *
 * @param dev
 * @param buff_len Length of data to read.
 * @param nodeid
 * @param fh
 * TODOFS: define suitable output. I defined such output because I needed to
 * benchmark file listings
 * @param[out] dirents an array of dirents.
 * @param[out] num_dirents number of dirents that the function returns
 * in @p dirents
 * @return int
 */
int uk_fuse_request_readdirplus(struct uk_fuse_dev *dev, uint64_t buff_len,
				uint64_t nodeid, uint64_t fh,
				struct fuse_dirent *dirents,
				size_t *num_dirents)
{
	int rc = 0;
	FUSE_READ_IN read_in = {0};
	FUSE_READ_OUT *read_out;
	uint64_t offset = 0;
	uint32_t remains;
	struct uk_fuse_req *req;
	struct fuse_direntplus *direntplus;
	*num_dirents = 0;

	UK_ASSERT(dev);
	UK_ASSERT(dirents);
	UK_ASSERT(num_dirents);

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	read_out = uk_calloc(dev->a, 1, sizeof(struct fuse_out_header)
		+ buff_len * 2);

	if (!read_out) {
		uk_pr_err("Could not allocate memory\n");
		return -1;
	}

	while (1) {
		FUSE_HEADER_INIT(&read_in.hdr, FUSE_READDIRPLUS,
				 nodeid, sizeof(read_in.read));

		read_in.read.fh = fh;
		read_in.read.offset = offset;
		read_in.read.size = buff_len * 2;

		req->in_buffer = &read_in;
		req->in_buffer_size = read_in.hdr.len;
		req->out_buffer = read_out;
		req->out_buffer_size = sizeof(struct fuse_out_header)
			+ read_in.read.size;

		if ((rc = send_and_wait(dev, req)))
			goto free;

		remains = read_out->hdr.len - sizeof(struct fuse_out_header);
		/* A successful request with no data means no more dirents */
		if (!remains) {
			uk_pr_debug("A successful request with no data."
				    "I.e. no more dirents\n");
			break;
		}

		direntplus = (struct fuse_direntplus *) read_out->buf;

		uk_pr_debug("Dirents in request %" __PRIu64 ":\n",
			read_in.hdr.unique);

		while (remains > sizeof(struct fuse_direntplus)) {
		/* At least one dirent is left */
			uk_pr_debug("    %" __PRIu64 "/%s -> %" __PRIu64 "\n",
			nodeid, direntplus->dirent.name,
			direntplus->entry_out.nodeid);

			dirents[*num_dirents] = direntplus->dirent;
			(*num_dirents)++;

			if (strcmp(direntplus->dirent.name, ".") &&
			    strcmp(direntplus->dirent.name, "..")) {
			/* TODOFS: increment nlookup if not "." or ".."
			   for this I'll prolly need to store a hashmap in
			   the uk_fuse_dev, which maps nodeids to nlookup
			   numbers of each file/directory */
			}

			offset = direntplus->dirent.off;
			remains -= FUSE_DIRENTPLUS_SIZE(direntplus);
			direntplus = (struct fuse_direntplus *)
				     ((char *) direntplus +
				     FUSE_DIRENTPLUS_SIZE(direntplus));
		}
	}



free:
	uk_free(dev->a, read_out);
	uk_fusedev_req_remove(dev, req);
	return rc;
}

/**
 * @brief
 *
 * @param dev
 * @param parent_nodeid
 * @param dir_name
 * @param mode type specification bits (e.g. S_IFDIR) may not be set (is set inside virtiofsd)
 * @param[out] nodeid
 * @param[out] nlookup
 * @return int
 */
int uk_fuse_request_mkdir(struct uk_fuse_dev *dev, uint64_t parent_nodeid,
			  const char *dir_name, uint32_t mode,
			  uint64_t *nodeid, uint64_t *nlookup)
{
	int rc = 0;
	FUSE_MKDIR_IN mkdir_in = {0};
	FUSE_MKDIR_OUT mkdir_out = {0};
	struct uk_fuse_req *req;

	UK_ASSERT(dev);
	UK_ASSERT(dir_name);
	UK_ASSERT(nodeid);
	UK_ASSERT(nlookup);

	FUSE_HEADER_INIT(&mkdir_in.hdr, FUSE_MKDIR, parent_nodeid,
		sizeof(struct fuse_mkdir_in) + strlen(dir_name) + 1);

	if (strlen(dir_name) > NAME_MAX) {
		uk_pr_err("Directory name is larger than %d characters\n",
			NAME_MAX);
		return -1;
	}

	mkdir_in.hdr.uid = dev->owner_uid;
	mkdir_in.hdr.gid = dev->owner_gid;

	strcpy(mkdir_in.name, dir_name);
	mkdir_in.mkdir.mode = mode | 0111; /* ---x--x--x */

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	req->in_buffer = &mkdir_in;
	req->in_buffer_size = mkdir_in.hdr.len;
	req->out_buffer = &mkdir_out;
	req->out_buffer_size = sizeof(mkdir_out); // we don't expect a reply;

	if ((rc = send_and_wait(dev, req)))
		goto free;

	*nodeid = mkdir_out.entry.nodeid;
	*nlookup = 1; // newly created directory has nlookup = 1

free:
	uk_fusedev_req_remove(dev, req);
	return rc;
}

int uk_fuse_request_forget(struct uk_fuse_dev *dev, uint64_t nodeid,
			   uint64_t nlookup)
{
	int rc = 0;
	FUSE_FORGET_IN forget_in = {0};
	struct uk_fuse_req *req;

	UK_ASSERT(dev);

	FUSE_HEADER_INIT(&forget_in.hdr, FUSE_FORGET,
		nodeid, sizeof(forget_in.forget));

	forget_in.forget.nlookup = nlookup;

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	req->in_buffer = &forget_in;
	req->in_buffer_size = forget_in.hdr.len;
	req->out_buffer = NULL;
	req->out_buffer_size = 0; // we don't expect a reply;

	if ((rc = send_and_wait(dev, req)))
		goto free;

free:
	uk_fusedev_req_remove(dev, req);
	return rc;

}

/**
 * @brief delete a file
 *
 * @param dev
 * @param filename
 * @param nlookup
 * @return int
 */
int uk_fuse_request_unlink(struct uk_fuse_dev *dev, const char *filename,
			   bool is_dir, uint64_t nodeid, uint64_t nlookup,
			   uint64_t parent_nodeid)
{
	int rc = 0;
	FUSE_UNLINK_IN unlink_in = {0};
	FUSE_UNLINK_OUT unlink_out = {0};
	struct uk_fuse_req *req;

	UK_ASSERT(dev);
	UK_ASSERT(filename);

	if (strlen(filename) > NAME_MAX) {
		uk_pr_err("Filename is larger than %d characters\n", NAME_MAX);
		return -1;
	}

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	FUSE_HEADER_INIT(&unlink_in.hdr, is_dir ? FUSE_RMDIR : FUSE_UNLINK,
	parent_nodeid, strlen(filename) + 1);

	strcpy(unlink_in.name, filename);

	req->in_buffer = &unlink_in;
	req->in_buffer_size = unlink_in.hdr.len;
	req->out_buffer = &unlink_out;
	req->out_buffer_size = sizeof(unlink_out);

	if ((rc = send_and_wait(dev, req)))
		goto free;

	uk_fuse_request_forget(dev, nodeid, nlookup);

free:
	uk_fusedev_req_remove(dev, req);
	return rc;
}

/**
 * @brief
 *
 * @param dev
 * @param fh
 * @param nodeid
 * @param file_off
 * @param length
 * @param[out] out_buf buffer, containing read data
 * @param[out] bytes_transferred how many bytes have been read
 * @return int
 */
int uk_fuse_request_read(struct uk_fuse_dev *dev, uint64_t nodeid, uint64_t fh,
			 uint64_t file_off, uint32_t length,
			 void *out_buf, uint32_t *bytes_transferred)
{
	int rc = 0;
	uint32_t max_req_buf_size; /* buffer for one request */
	*bytes_transferred = 0;
	FUSE_READ_OUT *read_out;
	struct uk_fuse_req *req;

	UK_ASSERT(dev);
	UK_ASSERT(out_buf);
	UK_ASSERT(bytes_transferred);

	if (out_buf == NULL) {
		uk_pr_err("%s: Provided buffer is invalid", __func__);
		return -1;
	}

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	/* Maximum size of a buffer for one request.
	   (Host page size is unknown, but it can't be less than 4KiB. */
	max_req_buf_size = MIN(dev->max_pages * PAGE_SIZE_4k, length);

	read_out = uk_calloc(dev->a, 1, sizeof(*read_out)
						 + max_req_buf_size);
	if (read_out == NULL) {
		rc = -1;
		goto req_remove;
	}

	while (length)
	{
		uint32_t req_buf_size = MIN(length, max_req_buf_size);
		FUSE_READ_IN read_in;
		uint32_t req_out_size; /* how many bytes of data a single
					  request has returned */

		read_in.read.fh = fh;
		read_in.read.offset = file_off;
		read_in.read.size = req_buf_size;

		FUSE_HEADER_INIT(&read_in.hdr, FUSE_READ, nodeid,
			sizeof(read_in.read));

		req->in_buffer = &read_in;
		req->in_buffer_size = sizeof(read_in);
		/* TODOFS: when substituting this for a MACRO, beware of
		   read_out and not &read_out. */
		req->out_buffer = read_out;
		req->out_buffer_size = sizeof(*read_out) + req_buf_size;

		if ((rc = send_and_wait(dev, req)))
			goto free;

		req_out_size = read_out->hdr.len
					- sizeof(struct fuse_out_header);
		memcpy((char *) out_buf, read_out->buf, req_out_size);
		*bytes_transferred += req_out_size;

		/* A successful read with no bytes read means file offset
		   is at or past the end of file. */
		if (req_out_size == 0) {
			uk_pr_err("End of file reached\n");
			rc = EOF;
			goto free;
		}

		if (req_out_size < req_buf_size)
			break;

		out_buf = (char *) out_buf + req_out_size;
		file_off += req_out_size;
		length -= req_out_size;
	}

	uk_pr_debug("%s: Bytes transfered %" __PRIu32 "\n", __func__,
							*bytes_transferred);

free:
	uk_free(dev->a, read_out);
req_remove:
	uk_fusedev_req_remove(dev, req);
	return rc;
}

/**
 * @brief
 *
 * @param dev
 * @param fh
 * @param nodeid
 * @param length
 * @param off
 * @param[out] bytes_transferred how many bytes have been transferred
 * by this function call
 * @return int
 */
int uk_fuse_request_write(struct uk_fuse_dev *dev, uint64_t nodeid, uint64_t fh,
			  const void *in_buf, uint32_t length, uint64_t off,
			  uint32_t *bytes_transferred)
{
	int rc = 0;
	FUSE_WRITE_IN *write_in;
	FUSE_WRITE_OUT write_out = {0};
	struct uk_fuse_req *req;
	uint32_t write_size;
	*bytes_transferred = 0;

	UK_ASSERT(dev);
	UK_ASSERT(in_buf);
	UK_ASSERT(bytes_transferred);

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	write_size = MIN(length, dev->max_write);
	write_in = uk_calloc(dev->a, 1, sizeof(*write_in) + write_size);
	if (write_in == NULL) {
		rc = -1;
		goto req_remove;
	}

	do {
		FUSE_HEADER_INIT(&write_in->hdr, FUSE_WRITE, nodeid,
				 sizeof(struct fuse_write_in) + write_size);

		write_in->write.fh = fh;
		write_in->write.offset = off + *bytes_transferred;
		write_in->write.size = write_size;
		memcpy(write_in->buf, (char *) in_buf + *bytes_transferred,
		       write_size);

		req->in_buffer = write_in;
		req->in_buffer_size = write_in->hdr.len;
		req->out_buffer = &write_out;
		req->out_buffer_size = sizeof(write_out);

		if ((rc = send_and_wait(dev, req)))
			goto free;

		*bytes_transferred += write_out.write.size;
		length -= write_out.write.size;
		write_size = MIN(length, dev->max_write);


	} while (length > 0);

free:
	uk_free(dev->a, write_in);
req_remove:
	uk_fusedev_req_remove(dev, req);
	return rc;

}

/**
 * @brief
 *
 * This is the converse of FUSE_OPEN and FUSE_OPENDIR
 * respectively.  The daemon may now free any resources
 * associated with the file handle fh as the kernel will no
 * longer refer to it. There is no reply data associated
 * with this request, but a reply still needs to be issued
 * once the request has been completely processed.
 *
 *
 *
 * @param dev
 * @param fh file handle from the uk_fuse_request_open
 * @return int
 */
int uk_fuse_request_release(struct uk_fuse_dev *dev, bool is_dir,
			    uint64_t nodeid, uint64_t fh)
{
	int rc = 0;
	FUSE_RELEASE_IN release_in = {0};
	FUSE_RELEASE_OUT release_out = {0};
	struct uk_fuse_req *req;

	UK_ASSERT(dev);

	uk_pr_debug("Release request issued: fh %" __PRIu64 ", nodeid %" __PRIu64
		    "\n", fh, nodeid);

	FUSE_HEADER_INIT(&release_in.hdr, is_dir ? FUSE_RELEASEDIR :
		FUSE_RELEASE, nodeid, sizeof(release_in.release));

	release_in.release.fh = fh;

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	req->in_buffer = &release_in;
	req->in_buffer_size = sizeof(release_in);
	req->out_buffer = &release_out;
	req->out_buffer_size = sizeof(release_out);

	if ((rc = send_and_wait(dev, req)))
		goto free;

	uk_fusedev_req_remove(dev, req);
	return 0;

free:
	uk_fusedev_req_remove(dev, req);
	return rc;
}


int uk_fuse_request_setattr(struct uk_fuse_dev *dev, uint64_t nodeid,
			    bool is_dir, uint64_t fh, uint32_t mode,
			    uint64_t last_access_time, uint64_t last_write_time,
			    uint64_t change_time)
{
	int rc = 0;
	FUSE_SETATTR_IN setattr_in = {0};
	FUSE_SETATTR_OUT setattr_out = {0};
	// uint64_t modification_time;
	struct uk_fuse_req *req;


	FUSE_HEADER_INIT(&setattr_in.hdr, FUSE_SETATTR,
		nodeid, sizeof(setattr_in.setattr));

	if (!is_dir && fh != INVALID_FILE_HANDLE) {
		setattr_in.setattr.valid |= FATTR_FH;
		setattr_in.setattr.fh = fh;
	}

	if (mode != INVALID_MODE_ATTRIBUTES) {
		setattr_in.setattr.valid |= FATTR_MODE;
		setattr_in.setattr.mode = mode;
	}


	(void) last_access_time;
	(void) last_write_time;
	(void) change_time;
	/*
	if (last_access_time)
		setattr_in.setattr.valid |= FATTR_ATIME;
		TODOFS: convert time to UNIX time and assign to
		   setattr_in.setattr.atime & setattr_in.setattr.atimesec
	*/

	/* Change time is updated whenever last_write_time is, but also on,
	   e.g. metadata changes */
	/*
	if (change_time || last_write_time) {
		if (change_time)
			modification_time = change_time;
		else
			modification_time = last_write_time;

		setattr_in.setattr.valid |= FATTR_MTIME;
		TODOFS: convert time to UNIX time and assign to
		   setattr_in.setattr.mtime & setattr_in.setattr.mtimesec
	}
	*/

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	req->in_buffer = &setattr_in;
	req->in_buffer_size = sizeof(setattr_in);
	req->out_buffer = &setattr_out;
	req->out_buffer_size = sizeof(setattr_out);

	if ((rc = send_and_wait(dev, req)))
		goto free;

	uk_fusedev_req_remove(dev, req);
	return 0;

free:
	uk_fusedev_req_remove(dev, req);
	return rc;

}

/**
 * @brief flush any pending changes to the indicated file handle
 *
 * The requested action is to flush any pending changes to
 * the indicated file handle.  No reply data is expected.
 * However, an empty reply message still needs to be issued
 * once the flush operation is complete.
 *
 * @param dev
 * @param fh
 * @param nodeid
 * @return int
 */
int uk_fuse_request_flush(struct uk_fuse_dev *dev, uint64_t nodeid, uint64_t fh)
{
	int rc = 0;
	FUSE_FLUSH_IN flush_in = {0};
	FUSE_FLUSH_OUT flush_out = {0};
	struct uk_fuse_req *req;


	UK_ASSERT(dev);

	FUSE_HEADER_INIT(&flush_in.hdr, FUSE_FLUSH, nodeid,
		sizeof(flush_in.flush));

	flush_in.flush.fh = fh;

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	req->in_buffer = &flush_in;
	req->in_buffer_size = sizeof(flush_in);
	req->out_buffer = &flush_out;
	req->out_buffer_size = sizeof(flush_out);

	if ((rc = send_and_wait(dev, req)))
		goto free;

	uk_fusedev_req_remove(dev, req);
	return 0;

free:
	uk_fusedev_req_remove(dev, req);
	return rc;
}


/* TODOFS: add uk_fuse_dev fid? */
/* TODOFS: how to set flags and mode? Probably comes from the VFS */
/**
 * @brief creates and opens a file.
 *
 * See uk_fuse_request_mkdir for creating a directory.
 *
 * Fails if a file with the given pathname already exists (because of O_EXCL).
 *
 * @param dev
 * @param parent parent directory nodeid
 * @param file_name
 * @param flags as in open(2)
 * @param mode as in open(2) file type (mask: 0170000) + set-user-ID,
 * set-group-ID, sticky bit (mask: 0007000) + permissions (mask: 000777)
 * https://man7.org/linux/man-pages/man7/inode.7.html
 * @param[out] nodeid
 * @param[out] fh
 * @param[out] nlookup
 * @return int
 */
int uk_fuse_request_create(struct uk_fuse_dev *dev, uint64_t parent,
			   const char *file_name, uint32_t flags,
			   uint32_t mode, uint64_t *nodeid, uint64_t *fh,
			   uint64_t *nlookup)
{
	int rc = 0;
	FUSE_CREATE_IN create_in = {0};
	FUSE_CREATE_OUT create_out = {0};
	struct uk_fuse_req *req;

	UK_ASSERT(dev);
	UK_ASSERT(file_name);
	UK_ASSERT(nodeid);
	UK_ASSERT(fh);
	UK_ASSERT(nlookup);

	if (strlen(file_name) > NAME_MAX) {
		uk_pr_err("provided filename is too long: %zu characters. Max "
		"length is %d\n", strlen(file_name), NAME_MAX);
		return -1;
	}
	if ((flags & (O_RDONLY | O_WRONLY | O_RDWR)) == 0) {
		uk_pr_err("The argument flags must include one of the "
		"following access modes: O_RDONLY, O_WRONLY, O_RDWR\n");
		return -1;
	}

	FUSE_HEADER_INIT(&create_in.hdr, FUSE_CREATE, parent,
		sizeof(struct fuse_create_in) + strlen(file_name) + 1);

	create_in.hdr.uid = dev->owner_uid;
	create_in.hdr.gid = dev->owner_gid;
	strcpy(create_in.name, file_name);

	create_in.create.mode = mode;
	create_in.create.umask = 0;
	/* TODOFS: think where to put the assignment */
	create_in.create.flags = O_CREAT | O_EXCL | flags;

	uk_pr_debug("create_in.create.flags: 0x%08x\n", create_in.create.flags);
	uk_pr_debug("create_in.create.mode: 0x%08x\n", create_in.create.mode);

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	req->in_buffer = &create_in;
	req->in_buffer_size = sizeof(create_in);
	req->out_buffer = &create_out;
	req->out_buffer_size = sizeof(create_out);

	if ((rc = send_and_wait(dev, req)))
		goto free;

	*nodeid = create_out.entry.nodeid;
	*fh     = create_out.open.fh;
	*nlookup = 1; // newly created file has nloookup = 1

	uk_fusedev_req_remove(dev, req);
	return 0;
free:
	uk_fusedev_req_remove(dev, req);
	return rc;
}

/**
 * @brief
 *
 * Creation flags (e.g. O_CREAT) are not allowed. A file is created with
 * FUSE_CREATE or FUSE_MKNOD.
 *
 * @param dev
 * @param is_dir
 * @param nodeid nodeid of the object (file/dir) to open
 * @param flags flags as in open(2)
 * @param[out] fh file handle
 * @return int
 */
int uk_fuse_request_open(struct uk_fuse_dev *dev, bool is_dir,
			 uint64_t nodeid, uint32_t flags, uint64_t *fh)
{
	int rc = 0;
	FUSE_OPEN_IN open_in = {0};
	FUSE_OPEN_OUT open_out = {0};
	struct uk_fuse_req *req;

	UK_ASSERT(dev);
	UK_ASSERT(fh);

	FUSE_HEADER_INIT(&open_in.hdr, is_dir ? FUSE_OPENDIR : FUSE_OPEN,
		nodeid, sizeof(open_in.open));

	/* TODOFS: think of flags */
	open_in.open.flags = is_dir ? (O_RDONLY | O_DIRECTORY)
		: flags;

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	req->in_buffer = &open_in;
	req->in_buffer_size = sizeof(open_in);
	req->out_buffer = &open_out;
	req->out_buffer_size = sizeof(open_out);

	if ((rc = send_and_wait(dev, req)))
		goto free;

	*fh = open_out.open.fh;

	uk_fusedev_req_remove(dev, req);
	return 0;

free:
	uk_fusedev_req_remove(dev, req);
	return rc;
}

int uk_fuse_request_lookup(struct uk_fuse_dev *dev, uint64_t dir_nodeid,
		   const char *filename, uint64_t *nodeid)
{
	int rc = 0;
	FUSE_LOOKUP_IN lookup_in = {0};
	FUSE_LOOKUP_OUT lookup_out = {0};
	struct uk_fuse_req *req;
	struct fuse_attr *attr;

	UK_ASSERT(dev);
	UK_ASSERT(filename);

	FUSE_HEADER_INIT(&lookup_in.hdr, FUSE_LOOKUP,
		 dir_nodeid, strlen(filename) + 1);

	strcpy(lookup_in.name, filename);

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	/* TODOFS: macro this */
	req->in_buffer = &lookup_in;
	req->in_buffer_size = sizeof(lookup_in);
	req->out_buffer = &lookup_out;
	req->out_buffer_size = sizeof(lookup_out);

	if ((rc = send_and_wait(dev, req)))
		goto free;


	attr = &lookup_out.entry.attr;
	uk_pr_debug("FUSE_LOOKUP: attr = {nodeid=%" __PRIu64 " ino=%" __PRIu64
		" size=%" __PRIu64 "}\n",
		lookup_out.entry.nodeid, attr->ino, attr->size);

	*nodeid = lookup_out.entry.nodeid;

	// TODOFS: increment nlookup

	uk_fusedev_req_remove(dev, req);
	return 0;

free:
	uk_fusedev_req_remove(dev, req);
	return rc;
}

/**
 * @brief
 *
 * The requested operation is to compute the attributes to be
 * returned by stat(2) and similar operations for the given
 * filesystem object.
 *
 * The object for which the attributes
 * should be computed is indicated either by header->nodeid
 * or, if the FUSE_GETATTR_FH flag is set, by the file handle fh.
 * The latter case of operation is analogous to fstat(2).
 *
 * @param dev
 * @param nodeid
 * @param file_handle
 * @param[out] fuse_attr
 * @return int
 */
int uk_fuse_request_get_attr(struct uk_fuse_dev *dev, uint64_t nodeid,
		     uint64_t file_handle, struct fuse_attr *attr)
{
	int rc = 0;
	FUSE_GETATTR_IN getattr_in = {0};
	FUSE_GETATTR_OUT getattr_out = {0};
	struct uk_fuse_req *req;

	UK_ASSERT(dev);
	UK_ASSERT(attr);

	FUSE_HEADER_INIT(&getattr_in.hdr, FUSE_GETATTR, nodeid,
			  sizeof(getattr_in.getattr));

	if (file_handle != INVALID_FILE_HANDLE) {
		getattr_in.getattr.fh = file_handle;
		getattr_in.getattr.getattr_flags |= FUSE_GETATTR_FH;
	}

	/* TODOFS: macro this request creation */
	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	req->in_buffer = &getattr_in;
	req->in_buffer_size = sizeof(getattr_in);
	req->out_buffer = &getattr_out;
	req->out_buffer_size = sizeof(getattr_out);

	if ((rc = send_and_wait(dev, req)))
		goto free;

	*attr = getattr_out.attr.attr;

	uk_fusedev_req_remove(dev, req);
	return 0;

free:
	uk_fusedev_req_remove(dev, req);
	return rc;
}

int uk_fuse_request_init(struct uk_fuse_dev *dev)
{
	struct uk_fuse_req *req;
	FUSE_INIT_IN init_in = {0};
	FUSE_INIT_OUT init_out = {0};
	int rc = 0;

	UK_ASSERT(dev);

	FUSE_HEADER_INIT(&init_in.hdr, FUSE_INIT, FUSE_ROOT_ID,
sizeof(init_in.init));

	init_in.init.major = FUSE_KERNEL_VERSION;
	init_in.init.minor = FUSE_KERNEL_MINOR_VERSION;
	init_in.init.max_readahead = 0;
	init_in.init.flags = FUSE_DO_READDIRPLUS | FUSE_MAX_PAGES
		| FUSE_MAP_ALIGNMENT;

	req = uk_fusedev_req_create(dev);
	if (PTRISERR(req))
		return PTR2ERR(req);

	req->in_buffer = &init_in;
	req->in_buffer_size = sizeof(init_in);
	req->out_buffer = &init_out;
	req->out_buffer_size = sizeof(init_out);

	if ((rc = send_and_wait(dev, req)))
		goto free;

	dev->max_write = init_out.init.max_write;
	dev->max_pages = init_out.init.max_pages ?
		init_out.init.max_pages : FUSE_DEFAULT_MAX_PAGES_PER_REQ;
	if (init_out.init.flags & FUSE_MAP_ALIGNMENT)
		dev->map_alignment = bpow(2, init_out.init.map_alignment);
	else
		dev->map_alignment = 4096; /*TODOFS: what would be the correct
					     value here? */



	uk_fusedev_req_remove(dev, req);
	return 0;

free:
	uk_fusedev_req_remove(dev, req);
	return rc;
}

int test_method() {
	int rc = 0;
	struct uk_fusedev_trans *trans;
	struct uk_fuse_dev *dev;
	struct fuse_attr attr = {0};
	// FUSE_LOOKUP_OUT lookup_out = {0};
	// uint64_t fh;
	fuse_file_context fc = {
		.is_dir = false,
		.name = "sometext.txt",
		.mode = S_IFREG | S_IRWXU | S_IRWXG | S_IRWXO,
		.flags = O_RDWR | O_CREAT | O_EXCL | O_NONBLOCK | O_LARGEFILE,
		.parent_nodeid = 1
	};
	fuse_file_context dc = {.is_dir = true};
	const char *send_message = "Hello, host\n";
	uint32_t bytes_transferred;
	char *read_message;
	size_t read_size;
/* uk_fuse_request_readdirplus */
	fuse_file_context rootdir = {0};
	struct fuse_dirent dirents[10];
	size_t num_dirents;
/* uk_fuse_request_lseek */
	char *read_message_lseek;
	size_t read_size_lseek;
	off_t lseek_off;

	read_size = sizeof(char) * (strlen(send_message) + 1 - 3);
	read_message = malloc(read_size);
	if (!read_message) {
		uk_pr_err("Could not allocate a read buffer.\n");
		return -1;
	}

	read_size_lseek = sizeof(char) * (strlen(send_message) + 1 - 3);
	read_message_lseek = malloc(read_size_lseek);
	if (!read_message_lseek) {
		uk_pr_err("Could not allocate a read buffer.\n");
		return -1;
	}

	trans = uk_fusedev_trans_get_default();
	dev = uk_fusedev_connect(trans, "myfs", NULL);


	if ((rc = uk_fuse_request_init(dev))) {
		uk_pr_err("uk_fuse_init has failed \n");
		goto free;
	}

	if ((rc = uk_fuse_request_get_attr(dev, 1, -1, &attr))) {
		uk_pr_err("uk_fuse_get_attr has failed \n");
		goto free;
	}

	// if ((rc = uk_fuse_request_lookup(dev, 1,
	//      "dogcat", &lookup_out))) {
	// 	uk_pr_err("uk_fuse_lookup has failed \n");
	// 	goto free;
	// }

	// dev->owner_uid = lookup_out.entry.attr.uid;
	// dev->owner_gid = lookup_out.entry.attr.gid;

	if ((rc = uk_fuse_request_create(dev, fc.parent_nodeid,
			fc.name, fc.flags, fc.mode,
			&fc.nodeid, &fc.fh,
			&fc.nlookup))) {
		uk_pr_err("uk_fuse_create_file_request has failed \n");
		goto free;
	}

	// if ((rc = uk_fuse_request_flush(dev, ffc.nodeid, ffc.fh))) {
	// 	uk_pr_err("uk_fuse_request_flush has failed \n");
	// 	goto free;
	// }

	// if ((rc = uk_fuse_request_open(dev, false, entry.nodeid,
	// 		       O_RDONLY, &fh))) {
	// 	uk_pr_err("uk_fuse_request_open has failed \n");
	// 	goto free;
	// }

	// if ((rc = uk_fuse_request_read(dev, ffc.nodeid, ffc.fh, 0,
	// 		       recv_size, recv_message,
	// 		       &bytes_transferred))) {
	// 	uk_pr_err("uk_fuse_request_read has failed \n");
	// 	goto free;
	// }

	if ((rc = uk_fuse_request_write(dev, fc.nodeid, fc.fh,
		send_message, (uint32_t) strlen(send_message)+1,
		0, &bytes_transferred))) {
		uk_pr_err("uk_fuse_request_write has failed \n");
		goto free;
	}

	if ((rc = uk_fuse_request_flush(dev, fc.nodeid, fc.fh))) {
		uk_pr_err("uk_fuse_request_flush has failed \n");
		goto free;
	}

	bytes_transferred = 0;

	if ((rc = uk_fuse_request_read(dev, fc.nodeid, fc.fh, 3,
			       read_size, read_message,
			       &bytes_transferred))) {
		uk_pr_err("uk_fuse_request_read has failed \n");
		goto free;
	}
	uk_pr_debug("Read %" __PRIu32 " bytes: '%s'\n", bytes_transferred,
		    read_message);

	add_fusedev(dev);

	// if ((rc = uk_fuse_request_lseek(dev, fc.nodeid, fc.fh,
	// 		3, SEEK_SET, &lseek_off))) {
	// 	uk_pr_err("uk_fuse_request_read has failed \n");
	// 	goto free;
	// }

	// bytes_transferred = 0;
	// if ((rc = uk_fuse_request_read(dev, fc.nodeid, fc.fh, 0,
	// 		       read_size_lseek, read_message_lseek,
	// 		       &bytes_transferred))) {
	// 	uk_pr_err("uk_fuse_request_read has failed \n");
	// 	goto free;
	// }
	// uk_pr_debug("Read %" __PRIu32 " bytes with lseek: '%s'\n",
	// 	    bytes_transferred, read_message_lseek);

	if ((rc = uk_fuse_request_unlink(dev, fc.name, false, 2, 1,
		1))) {
		uk_pr_err("uk_fuse_delete_request has failed \n");
		goto free;
	}

	if ((rc = uk_fuse_request_mkdir(dev, 1,
		"Documents", 0777, &dc.nodeid, &dc.nlookup))) {
		uk_pr_err("uk_fuse_request_mkdir has failed \n");
		goto free;
	}

	// memset(&fc.file_name, 0,
	// 	sizeof(fc.file_name)/sizeof(fc.file_name[0]));
	// strcpy(fc.file_name, "document1.txt");
	// if ((rc = uk_fuse_request_create(dev, dc.nodeid,
	// 	fc.file_name, fc.flags, fc.mode,
	// 	&fc.nodeid, &fc.fh,
	// 	&fc.nlookup))) {
	// 	uk_pr_err("uk_fuse_create_file_request has failed \n");
	// 	goto free;
	// }
	// memset(&fc.file_name, 0,
	// 	sizeof(fc.file_name)/sizeof(fc.file_name[0]));
	// strcpy(fc.file_name, "document2.txt");
	// if ((rc = uk_fuse_request_create(dev, dc.nodeid,
	// 	fc.file_name, fc.flags, fc.mode,
	// 	&fc.nodeid, &fc.fh,
	// 	&fc.nlookup))) {
	// 	uk_pr_err("uk_fuse_create_file_request has failed \n");
	// 	goto free;
	// }
	// memset(&fc.file_name, 0,
	// 	sizeof(fc.file_name)/sizeof(fc.file_name[0]));
	// strcpy(fc.file_name, "document3.txt");
	// if ((rc = uk_fuse_request_create(dev, dc.nodeid,
	// 	fc.file_name, fc.flags, fc.mode,
	// 	&fc.nodeid, &fc.fh,
	// 	&fc.nlookup))) {
	// 	uk_pr_err("uk_fuse_create_file_request has failed \n");
	// 	goto free;
	// }

	// if ((rc = uk_fuse_request_open(dev, true, 2, &rootdir.fh, 0))) {
	// 	uk_pr_err("uk_fuse_request_open has failed \n");
	// 	goto free;
	// }

	// if ((rc = uk_fuse_request_readdirplus(dev, 500, 2, rootdir.fh,
	// 	dirents, &num_dirents))) {
	// 	uk_pr_err("uk_fuse_request_readdirplus has failed \n");
	// 	goto free;
	// }

	// if ((rc = uk_fuse_request_release(dev, true, 2, rootdir.fh))) {
	// 	uk_pr_err("uk_fuse_request_release has failed\n");
	// 	goto free;
	// }

	// if ((rc = uk_fuse_request_flush(dev, ffc.nodeid, ffc.fh))) {
	// 	uk_pr_err("uk_fuse_request_flush has failed \n");
	// 	goto free;
	// }

	// if ((rc = uk_fuse_request_setattr(dev, ffc.nodeid, false, ffc.fh,
	// 	S_IFREG | S_IRWXU | S_IRWXG | S_IROTH, 0, 0, 0))) {
	// 	uk_pr_err("uk_fuse_request_setattr has failed \n");
	// 	goto free;
	// }

	// if ((rc = uk_fuse_request_release(dev, false, ffc.fh, ffc.nodeid))) {
	// 	uk_pr_err("uk_fuse_request_setattr has failed \n");
	// 	goto free;
	// }

	// if ((rc = uk_fusedev_disconnect(dev)))
	// 	goto free;


free:
	free(read_message);
	free(read_message_lseek);
	return rc;
}
/* TODOFS: remove */

