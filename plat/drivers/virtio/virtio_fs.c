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

#include <virtio/virtio_bus.h>
#include <virtio/virtio_fs.h>
#include <virtio/virtio_ids.h>
#include <virtio/virtio_config.h>
#include <virtio/virtio_types.h>

#define DRIVER_NAME	"virtio-fs"

struct virtio_fs_device {
	/* Virtio device. */
	struct virtio_dev *vdev;
};

static int virtio_fs_drv_init(struct uk_alloc *drv_allocator)
{
	uk_pr_err("init()\n");
	return 0;
}

static int virtio_fs_add_dev(struct virtio_dev *vdev)
{
	uk_pr_err("add()\n");
	return 0;
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