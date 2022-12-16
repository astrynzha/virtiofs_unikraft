#ifndef __UK_FUSEDEV_TRANS__
#define __UK_FUSEDEV_TRANS__

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
 * Adapted from Unikraft 9pdev_trans.h
 * unikraft/lib/uk9p/include/uk/9pdev_trans.h
 */

#include "uk/fusedev_core.h"
#include <uk/list.h>
#include <uk/alloc.h>

#ifdef __cplusplus
extern "C" {
#endif

/** A structure used to describe a transport. */
struct uk_fusedev_trans {
	const char				*name;
	/* Supported operations. */
	const struct uk_fusedev_trans_ops	*ops;
	/* Allocator used for devices which use this transport layer. */
	struct uk_alloc				*a;
	/* @internal Entry in the list of available transports. */
	struct uk_list_head			_list;
};

int uk_fusedev_trans_register(struct uk_fusedev_trans *trans);
struct uk_fusedev_trans *uk_fusedev_trans_get_default(void);


#ifdef __cplusplus
}
#endif

#endif /* __UK_FUSEDEV_TRANS__ */
