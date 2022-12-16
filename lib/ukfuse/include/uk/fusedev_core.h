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
 * Adapted from Unikraft 9pdev_core.h
 * unikraft/lib/uk9p/include/uk/9pdev_core.h
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
 * Parts adapted from virtio-win virtiofs.cpp
 * kvm-guest-drivers-windows/viofs/svc/virtiofs.cpp
 * Commit-id: d4713c5
 */

#ifndef __UK_FUSEDEV_CORE__
#define __UK_FUSEDEV_CORE__

#include "uk/fusereq.h"
#include <stdint.h>
#include <uk/arch/spinlock.h>
#include <uk/wait_types.h>
#include <uk/list.h>

#ifdef __cplusplus
extern "C" {
#endif

struct uk_fuse_dev;

/**
 * Function type used for connecting to a device on a certain transport.
 *
 * @param dev
 *   The Unikraft FUSE Device.
 */
typedef int (*uk_fuse_connect_t)(struct uk_fuse_dev *dev,
				const char *device_identifier);

/**
 * Function type used for disconnecting from the device.
 *
 * @param dev
 *   The Unikraft FUSE Device.
 */
typedef int (*uk_fuse_disconnect_t)(struct uk_fuse_dev *dev);

/**
 * Function type used for sending a request to the FUSE device.
 *
 * @param dev
 *   The Unikraft FUSE device.
 */
typedef int (*uk_fuse_request_t)(struct uk_fuse_dev *fuse_dev,
				 struct uk_fuse_req *fuse_req);

struct uk_fusedev_trans_ops {
	uk_fuse_connect_t			connect;
	uk_fuse_disconnect_t			disconnect;
	uk_fuse_request_t			request;
};

enum uk_fuse_dev_trans_state {
	UK_FUSEDEV_CONNECTED,
	UK_FUSEDEV_DISCONNECTING
};

/**
 * @internal
 * A structure used for fuse requests' management.
 */
struct uk_fusedev_req_mgmt {
	/* Spinlock protecting this data. */
	__spinlock				spinlock;
	/* List of requests allocated and not yet removed. */
	struct uk_list_head			req_list;
	/* Free-list of requests. */
	struct uk_list_head			req_free_list;
};

/**
 * FUSE_DEV
 * A structure used to interact with a device that uses FUSE as a protocol
 */
struct uk_fuse_dev {
	/* Underlying transport operations. */
	const struct uk_fusedev_trans_ops	*ops;
	/* Allocator used by this device. */
	struct uk_alloc				*a;
	/* Transport allocated data */
	void					*priv;
	/* Transport state. */
	enum uk_fuse_dev_trans_state		state;
	/* Number of pages that can be used per request */
	uint32_t				max_pages;
	/* Maximum number of bytes in a single write operation */
	uint32_t				max_write;
	/* foffset and moffset parameters of FUSE_SETUPMAPPING have to be
	   a multiple of this value */
	uint32_t				map_alignment;

	/* Uid/Gid used to describe files' owner on the host side. */
	uint32_t				owner_uid;
	uint32_t				owner_gid;
	/* @internal Request management. */
	struct uk_fusedev_req_mgmt		_req_mgmt;
#if CONFIG_LIBUKSCHED
	/*
	 * Slept on by threads waiting for their turn for enough space to send
	 * the request.
	 */
	struct uk_waitq                 xmit_wq;
#endif
};

#define INIT_FUSE_DEV(dev) *dev = (struct uk_fuse_dev) \
	{.owner_uid = 0, .owner_gid = 0}

#ifdef __cplusplus
}
#endif

#endif /* __UK_FUSEDEV_CORE__ */
