#ifndef __UK_FUSE_DEV__
#define __UK_FUSE_DEV__

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
 * Adapted from Unikraft 9pdev.h
 * unikraft/lib/uk9p/include/uk/9pdev.h
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "uk/fusedev_core.h"
#include "uk/fusedev_trans.h"
#include "uk/fusereq.h"
#include <stddef.h>
#include <uk/alloc.h>

void uk_fusedev_req_to_freelist(struct uk_fuse_dev *dev,
				struct uk_fuse_req *req);
struct uk_fuse_req *uk_fusedev_req_create(struct uk_fuse_dev *dev);
int uk_fusedev_req_remove(struct uk_fuse_dev *dev, struct uk_fuse_req *req);
int uk_fusedev_request(struct uk_fuse_dev *dev, struct uk_fuse_req *req);
void uk_fusedev_xmit_notify(struct uk_fuse_dev *dev);
struct uk_fuse_dev *uk_fusedev_connect(const struct uk_fusedev_trans *trans,
				       const char *device_identifier,
				       struct uk_alloc *a);
int uk_fusedev_disconnect(struct uk_fuse_dev *dev);



#ifdef __cplusplus
}
#endif

#endif /* __UK_FUSE_DEV__ */
