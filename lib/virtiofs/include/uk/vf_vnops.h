#ifndef __UK_VF_VNOPS__
#define __UK_VF_VNOPS__

#include <virtio/virtio_bus.h>
#include "uk/fusedev_core.h"
#include "uk/vfdev.h"

int uk_vf_read(struct uk_vfdev *vfdev, uint64_t nodeid, uint64_t fh,
		      uint32_t len, uint64_t off, void *out_buf);

void add_fusedev(struct uk_fuse_dev *fusedev);
void add_vdev_for_dax(struct virtio_dev *vdev);
void vf_test_method();


#endif /* __UK_VFDEV__*/