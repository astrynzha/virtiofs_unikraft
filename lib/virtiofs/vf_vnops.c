#include "uk/vf_vnops.h"
#include "uk/vf_bench_tests.h"
#include "uk/assert.h"
#include "uk/fusedev_core.h"
#include "uk/print.h"
#include <virtio/virtio_bus.h>
#include "uk/vfdev.h"
#include <uk/fuse.h>
#include <uk/fuse_i.h>
#include <stdint.h>
#include <uk/assert.h>
#include <stdlib.h>

static struct virtio_dev *vdev_for_dax = NULL;
static struct uk_fuse_dev *fusedev_for_dax = NULL;

static inline int uk_vf_write_dax(struct uk_vfdev *vfdev, uint64_t nodeid,
				  uint64_t fh, uint32_t len, uint64_t off,
				  void *in_buf)
{
	int rc = 0;
	UK_ASSERT(vfdev);
	UK_ASSERT(vfdev->fuse_dev);

	rc = uk_fuse_request_setupmapping(vfdev->fuse_dev, nodeid, fh, off, len,
		FUSE_SETUPMAPPING_FLAG_WRITE, 0);
	if (rc) {
		uk_pr_err("%s: failed setting up a mapping\n", __func__);
		return -1;
	}

	memcpy((void *) vfdev->dax_addr, in_buf, 10);
	uk_pr_info("Wrote through DAX \n");

	return 0;
}

static inline int uk_vf_read_dax(struct uk_vfdev *vfdev, uint64_t nodeid,
				 uint64_t fh, uint32_t len, uint64_t off,
				 void *out_buf)
{
	int rc = 0;
	UK_ASSERT(vfdev);
	UK_ASSERT(vfdev->fuse_dev);

	rc = uk_fuse_request_setupmapping(vfdev->fuse_dev, nodeid, fh, off,
		len, FUSE_SETUPMAPPING_FLAG_READ, 0);
	if (rc) {
		uk_pr_err("%s: failed setting up a mapping\n", __func__);
		return -1;
	}

	memcpy(out_buf, (void *) vfdev->dax_addr, 10);
	uk_pr_info("Read through DAX \n"
		   "out_buf: %s\n", (char *) out_buf);

	return 0;

}

static inline int uk_vf_write_fuse(struct uk_vfdev *vfdev, uint64_t nodeid,
				  uint64_t fh, uint32_t len, uint64_t off,
				  void *out_buf)
{
	return 0;
}

static inline int uk_vf_read_fuse(struct uk_vfdev *vfdev, uint64_t nodeid,
				  uint64_t fh, uint32_t len, uint64_t off,
				  void *out_buf)
{
	return 0;
}

int uk_vf_write(struct uk_vfdev *vfdev, uint64_t nodeid, uint64_t fh,
		       uint32_t len, uint64_t off, void *in_buf)
{
	int rc = 0;

	if (vfdev->dax_enabled) {
		rc = uk_vf_write_dax(vfdev, nodeid, fh, len, off, in_buf);
	} else {
		rc = uk_vf_write_fuse(vfdev, nodeid, fh, len, off, in_buf);
	}

	return rc;
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
int uk_vf_read(struct uk_vfdev *vfdev, uint64_t nodeid, uint64_t fh,
		      uint32_t len, uint64_t off, void *out_buf)
{
	int rc = 0;

	if (vfdev->dax_enabled) {
		rc = uk_vf_read_dax(vfdev, nodeid, fh, len, off, out_buf);
	} else { /* Fallback to normal FUSE requests, if DAX is not enabled */
		rc = uk_vf_read_fuse(vfdev, nodeid, fh, len, off, out_buf);
	}

	return rc;
}

void add_vdev_for_dax(struct virtio_dev *vdev)
{
	vdev_for_dax = vdev;
}

void add_fusedev(struct uk_fuse_dev *fusedev)
{
	fusedev_for_dax = fusedev;
}

struct uk_vfdev uk_vf_connect(void) {
	struct uk_vfdev vfdev = {0};

	if (vdev_for_dax) {
		vfdev.dax_enabled =
			vdev_for_dax->cops->shm_present(vdev_for_dax, 0);
		vfdev.dax_addr =
			vdev_for_dax->cops->get_shm_addr(vdev_for_dax, 0);
		vfdev.dax_len =
			vdev_for_dax->cops->get_shm_length(vdev_for_dax, 0);
	}
	if (fusedev_for_dax)
		vfdev.fuse_dev = fusedev_for_dax;
	else
		uk_pr_info("%s: No fuse device found. \n", __func__);

	return vfdev;
}

void vf_test_method(void) {
	struct uk_vfdev vfdev = {0};
	void *outbuf;



	if (vdev_for_dax) {
		vfdev.dax_enabled =
			vdev_for_dax->cops->shm_present(vdev_for_dax, 0);
		vfdev.dax_addr =
			vdev_for_dax->cops->get_shm_addr(vdev_for_dax, 0);
		vfdev.dax_len =
			vdev_for_dax->cops->get_shm_length(vdev_for_dax, 0);
	}
	if (fusedev_for_dax)
		vfdev.fuse_dev = fusedev_for_dax;
	else
		uk_pr_err("%s: No fuse device found. \n", __func__);

	outbuf = calloc(11, 1);
	if (!outbuf) {
		uk_pr_err("%s: malloc failed \n", __func__);
		return;
	}
	uk_vf_write(&vfdev, 2, 0, 10,
		0, "0123456789");

	// uk_vf_read(&vfdev, 2, 0,
	// 	10, 0,outbuf);

	free(outbuf);
}