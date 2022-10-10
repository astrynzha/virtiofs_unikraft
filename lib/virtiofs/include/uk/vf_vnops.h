#ifndef __UK_VF_VNOPS__
#define __UK_VF_VNOPS__

#include <virtio/virtio_bus.h>
#include "uk/fusedev_core.h"

void add_fusedev(struct uk_fuse_dev *fusedev);
void add_vdev_for_dax(struct virtio_dev *vdev);
void vf_test_method();

#endif /* __UK_VFDEV__*/