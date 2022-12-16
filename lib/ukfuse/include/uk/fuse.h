#ifndef __LINUX_FUSE_H__
#define __LINUX_FUSE_H__

/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Andrii Strynzha <a.strynzha@gmail.com>
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


#include "limits.h"
#include "uk/fusereq.h"
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>

#define PAGE_SIZE_4k 4096

typedef struct
{
	bool is_dir;

	char name[NAME_MAX + 1];
	uint64_t nodeid;
	uint64_t fh; /* file handle */
	uint64_t parent_nodeid;

	/* TODOFS: think about race conditions, when updating this? */
	uint64_t nlookup;

	uint32_t flags;
	uint32_t mode;
} fuse_file_context;

int uk_fuse_request_fsync(struct uk_fuse_dev *dev, bool is_dir,
			  uint64_t nodeid, uint64_t fh, uint32_t flags);

int uk_fuse_request_removemapping_multiple(struct uk_fuse_dev *dev,
					FUSE_REMOVEMAPPING_IN *removemapping_in,
					size_t removemapping_one_cnt);

int uk_fuse_request_removemapping_legacy(struct uk_fuse_dev *dev,
					 uint64_t nodeid, uint64_t fh,
					 uint64_t moffset, uint64_t len);


int uk_fuse_request_setupmapping(struct uk_fuse_dev *dev, uint64_t nodeid,
				 uint64_t fh, uint64_t foffset, uint64_t len,
				 uint64_t flags, uint64_t moffset);

int uk_fuse_request_lseek(struct uk_fuse_dev *dev, uint64_t nodeid, uint64_t fh,
			  uint64_t offset, uint32_t whence,
			  off_t *offset_out);

int uk_fuse_request_readdirplus(struct uk_fuse_dev *dev, uint64_t buff_len,
				uint64_t nodeid, uint64_t fh,
				struct fuse_dirent *dirents,
				size_t *num_dirents);

int uk_fuse_request_mkdir(struct uk_fuse_dev *dev, uint64_t parent_nodeid,
			  const char *dir_name, uint32_t mode,
			  uint64_t *nodeid, uint64_t *nlookup);

int uk_fuse_request_forget(struct uk_fuse_dev *dev, uint64_t nodeid,
			   uint64_t nlookup);

int uk_fuse_request_unlink(struct uk_fuse_dev *dev, const char *filename,
			   bool is_dir, uint64_t nodeid, uint64_t nlookup,
			   uint64_t parent_nodeid);

int uk_fuse_request_read(struct uk_fuse_dev *dev, uint64_t nodeid, uint64_t fh,
			 uint64_t file_off, uint32_t length,
			 void *out_buf, uint32_t *bytes_transferred);

int uk_fuse_request_write(struct uk_fuse_dev *dev, uint64_t nodeid, uint64_t fh,
			  const void *in_buf, uint32_t length, uint64_t off,
			  uint32_t *bytes_transferred);

int uk_fuse_request_release(struct uk_fuse_dev *dev, bool is_dir,
			    uint64_t nodeid, uint64_t fh);

int uk_fuse_request_setattr(struct uk_fuse_dev *dev, uint64_t nodeid,
			    bool is_dir, uint64_t fh, uint32_t mode,
			    uint64_t last_access_time, uint64_t last_write_time,
			    uint64_t change_time);

int uk_fuse_request_flush(struct uk_fuse_dev *dev, uint64_t nodeid,
			  uint64_t fh);

int uk_fuse_request_create(struct uk_fuse_dev *dev, uint64_t parent,
			   const char *file_name, uint32_t flags,
			   uint32_t mode, uint64_t *nodeid, uint64_t *fh,
			   uint64_t *nlookup);

int uk_fuse_request_open(struct uk_fuse_dev *dev, bool is_dir,
			 uint64_t nodeid, uint32_t flags, uint64_t *fh);

int uk_fuse_request_lookup(struct uk_fuse_dev *dev, uint64_t dir_nodeid,
		   const char *filename, uint64_t *nodeid);

int uk_fuse_request_get_attr(struct uk_fuse_dev *dev, uint64_t nodeid,
		     uint64_t file_handle, struct fuse_attr *attr);

int uk_fuse_request_init(struct uk_fuse_dev *dev);

int test_method();

#endif /* __LINUX_FUSE_H__ */