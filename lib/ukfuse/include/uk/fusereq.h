#ifndef __LINUX_FUSEREQ_H__
#define __LINUX_FUSEREQ_H__

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
 * Adapted from virtio-win fusereq.h
 * viofs/svc/fusereq.h
 * Commit-id: d4713c5
 */


#include "limits.h"
#include "uk/fuse_i.h"
#include <stdint.h>
#include <limits.h>
#include <uk/arch/types.h>
#include <uk/refcount.h>
#include <uk/list.h>
#include <uk/wait_types.h>

#define FUSE_DEFAULT_MAX_PAGES_PER_REQ  32

#define INVALID_FILE_HANDLE (((uint64_t) (-1)))
#define INVALID_MODE_ATTRIBUTES (((uint32_t) (-1)))


/**
 * Describes the possible states in which a request may be.
 *
 * - NONE: Right after allocating.
 * - INITIALIZED: Request is ready to receive serialization data.
 * - READY: Request is ready to be sent.
 * - RECEIVED: Transport layer has received the reply and important data such
 *   as the tag, type and size have been validated.
 */
enum uk_fusereq_state {
	UK_FUSEREQ_NONE = 0,
	UK_FUSEREQ_INITIALIZED,
	UK_FUSEREQ_READY,
	UK_FUSEREQ_SENT,
	UK_FUSEREQ_RECEIVED
};


struct uk_fuse_req {
	void 				*in_buffer;
	uint32_t 			in_buffer_size;
	void 				*out_buffer;
	uint32_t 			out_buffer_size;
	/* State of the request. See the state enum for details. */
	enum uk_fusereq_state		state;
	/* @internal Entry into the list of requests (API-internal). */
	struct uk_list_head		_list;
	/* @internal FUSE device  request belongs to. */
	struct uk_fuse_dev		*_dev;
	/* @internal Allocator used to allocate this request. */
	struct uk_alloc			*_a;
	/* Tracks the number of references to this structure. */
	__atomic 			refcount;
#if CONFIG_LIBUKSCHED
	/* Wait-queue for state changes. */
	struct uk_waitq			wq;
#endif
};

typedef struct
{
	struct fuse_in_header		hdr;
	struct fuse_init_in		init;

} FUSE_INIT_IN;

typedef struct
{
	struct fuse_out_header		hdr;
	struct fuse_init_out		init;

} FUSE_INIT_OUT;

typedef struct
{
	struct fuse_in_header	hdr;
	struct fuse_create_in	create;
	char			name[NAME_MAX + 1];
} FUSE_CREATE_IN;

typedef struct
{
	struct fuse_out_header	hdr;
	struct fuse_entry_out	entry;
	struct fuse_open_out	open;
} FUSE_CREATE_OUT;

typedef struct
{
	struct fuse_in_header	hdr;
	struct fuse_getattr_in	getattr;
} FUSE_GETATTR_IN;

typedef struct
{
	struct fuse_out_header	hdr;
	struct fuse_attr_out	attr;
} FUSE_GETATTR_OUT;

typedef struct
{
	struct fuse_in_header	hdr;
	char			name[NAME_MAX + 1];
} FUSE_LOOKUP_IN;

typedef struct
{
	struct fuse_out_header	hdr;
	struct fuse_entry_out	entry;
} FUSE_LOOKUP_OUT;

typedef struct
{
	struct fuse_in_header	hdr;
	struct fuse_open_in	open;

} FUSE_OPEN_IN;

typedef struct
{
	struct fuse_out_header	hdr;
	struct fuse_open_out	open;

} FUSE_OPEN_OUT;

typedef struct
{
	struct fuse_in_header	hdr;
	struct fuse_flush_in	flush;

} FUSE_FLUSH_IN;

typedef struct
{
	struct fuse_out_header	hdr;
} FUSE_FLUSH_OUT;

typedef struct
{
	struct fuse_in_header	hdr;
	struct fuse_setattr_in	setattr;

} FUSE_SETATTR_IN;

typedef struct
{
	struct fuse_out_header	hdr;
	struct fuse_attr_out	attr;

} FUSE_SETATTR_OUT;

typedef struct
{
	struct fuse_in_header	hdr;
	struct fuse_release_in	release;

} FUSE_RELEASE_IN;

typedef struct
{
	struct fuse_out_header	hdr;

} FUSE_RELEASE_OUT;

typedef struct
{
	struct fuse_in_header	hdr;
	struct fuse_write_in	write;
	char			buf[];

} FUSE_WRITE_IN;

typedef struct
{
	struct fuse_out_header	hdr;
	struct fuse_write_out	write;

} FUSE_WRITE_OUT;

typedef struct
{
	struct fuse_in_header	hdr;
	struct fuse_read_in	read;

} FUSE_READ_IN;

typedef struct
{
	struct fuse_out_header	hdr;
	char			buf[];

} FUSE_READ_OUT;

typedef struct
{
	struct fuse_in_header	hdr;
	char			name[NAME_MAX + 1];

} FUSE_UNLINK_IN;

typedef struct
{
	struct fuse_out_header	hdr;

} FUSE_UNLINK_OUT;

typedef struct
{
	struct fuse_in_header	hdr;
	struct fuse_forget_in	forget;

} FUSE_FORGET_IN;

typedef struct
{
	struct fuse_out_header	hdr;

} FUSE_FORGET_OUT;

typedef struct
{
	struct fuse_in_header	hdr;
	struct fuse_mkdir_in	mkdir;
	char			name[NAME_MAX + 1];

} FUSE_MKDIR_IN;

typedef struct
{
	struct fuse_out_header	hdr;
	struct fuse_entry_out	entry;

} FUSE_MKDIR_OUT;

typedef struct
{
	struct fuse_in_header	hdr;
	struct fuse_lseek_in	lseek;

} FUSE_LSEEK_IN;

typedef struct
{
	struct fuse_out_header	hdr;
	struct fuse_lseek_out	lseek;

} FUSE_LSEEK_OUT;

typedef struct
{
	struct fuse_in_header hdr;
	struct fuse_setupmapping_in setupmapping;

} FUSE_SETUPMAPPING_IN;

typedef struct
{
	struct fuse_out_header hdr;
} FUSE_SETUPMAPPING_OUT;

typedef struct
{
	struct fuse_in_header hdr;
	struct fuse_removemapping_in removemapping_in;
	/* TODOFS: use FUSE_REMOVEMAPPING_MAX_ENTRY from fuse_i.h as the array
	   size. */
	struct fuse_removemapping_one removemapping_one[4096
		/ sizeof(struct fuse_removemapping_one)];

} FUSE_REMOVEMAPPING_IN;


typedef struct
{
	struct fuse_in_header hdr;
	struct fuse_removemapping_in_legacy removemapping_in;

} FUSE_REMOVEMAPPING_IN_LEGACY;

typedef struct
{
	struct fuse_out_header hdr;
} FUSE_REMOVEMAPPING_OUT;


typedef struct
{
	struct fuse_in_header hdr;
	struct fuse_fsync_in fsync;
} FUSE_FSYNC_IN;

typedef struct
{
	struct fuse_out_header hdr;
} FUSE_FSYNC_OUT;




void uk_fusereq_init(struct uk_fuse_req *req);
void uk_fusereq_get(struct uk_fuse_req *req);
int uk_fusereq_put(struct uk_fuse_req *req);
int uk_fusereq_receive_cb(struct uk_fuse_req *req, uint32_t len __unused);
int uk_fusereq_waitreply(struct uk_fuse_req *req);



#endif /* __LINUX_FUSEREQ_H__ */