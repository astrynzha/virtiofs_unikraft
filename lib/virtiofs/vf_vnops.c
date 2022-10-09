#include "uk/vf_vnops.h"
#include "uk/print.h"
#include "uk/virtio_bus.h"
#include "uk/vfdev.h"
#include <uk/fuse.h>
#include <stdint.h>

static struct virtio_dev *vdev_for_dax;

static inline int uk_vf_read_dax(struct uk_vfdev vfdev, uint64_t nodeid,
				 uint64_t fh, uint32_t length, uint64_t off)
{
	/**
	 * 1. FUSE_SETUPMAPPING. Or maybe keep track internally, if already
	 * mapped.
	 * 2. Write stuff directly to memory
	 */
	if (vdev_for_dax->cops->shm_present(vdev_for_dax, 0))
		uk_pr_debug("%s: shm present\n", __func__);

	return 0;

}

static inline int uk_vf_read_fuse(struct uk_vfdev vfdev, uint64_t nodeid,
				  uint64_t fh, uint32_t length, uint64_t off)
{
	return 0;
}

/**
 * @brief
 *
 * TODOFS: completely rework the signature. The one in place is just for testing
 * purposes. A good reference would be:
 * unikraft/lib/9pfs/9pfs_vnops.c:uk_9pfs_write()
 *
 * @param vfdev
 * @param nodeid
 * @param fh
 * @param length
 * @param off
 * @return int
 */
static int uk_vf_read(struct uk_vfdev vfdev, uint64_t nodeid, uint64_t fh,
		      uint32_t length, uint64_t off)
{
	int rc = 0;

	if (vfdev.dax_enabled) {
		rc = uk_vf_read_dax(vfdev, nodeid, fh, length, off);
	} else { /* Fallback to normal FUSE requests, if DAX is not enabled */
		rc = uk_vf_read_fuse(vfdev, nodeid, fh, length, off);
	}

	return rc;
}

void add_vdev_for_dax(struct virtio_dev *vdev)
{
	vdev_for_dax = vdev;
}

void vf_test_method() {
	struct uk_vfdev vfdev = {0};
	uk_vf_read_dax(vfdev, 0, 0, 0, 0);
}