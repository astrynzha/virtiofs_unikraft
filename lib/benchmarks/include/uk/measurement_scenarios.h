#ifndef MEASUREMENT_SCENARIOS_H
#define MEASUREMENT_SCENARIOS_H

#include <stdio.h>

#include "helper_functions.h"
#include "time_functions.h"

/* ukfuse */
#include "uk/vfdev.h"
#include "uk/fusedev_core.h"

__nanosec create_files(struct uk_fuse_dev *fusedev, FILES amount);
__nanosec remove_files(struct uk_fuse_dev *fusedev, FILES amount);
__nanosec list_dir(struct uk_fuse_dev *fusedev, FILES file_amount,
		   uint64_t parent);


__nanosec write_seq_fuse(struct uk_fuse_dev *fusedev, BYTES bytes,
		    BYTES buffer_size, uint64_t dir);
__nanosec write_seq_dax(struct uk_fuse_dev *fusedev, struct uk_vfdev *vfdev,
			BYTES bytes, BYTES buffer_size);
__nanosec write_randomly_fuse(struct uk_fuse_dev *fusedev,
			      BYTES remaining_bytes, BYTES buffer_size,
			      BYTES lower_write_limit, BYTES upper_write_limit);
__nanosec write_randomly_dax(struct uk_fuse_dev *fusedev,
			     struct uk_vfdev *vfdev, BYTES remaining_bytes,
			     BYTES buffer_size, BYTES lower_write_limit,
			     BYTES upper_write_limit);
// __nanosec write_randomly(FILE *file, BYTES bytes, BYTES buffer_size, BYTES lower_write_limit, BYTES upper_write_limit);

// __nanosec read_seq(FILE *file, BYTES bytes, BYTES buffer_size);
__nanosec read_seq(struct uk_fuse_dev *fusedev, BYTES bytes, BYTES buffer_size);
__nanosec read_seq_dax(struct uk_fuse_dev *fusedev, struct uk_vfdev *vfdev,
		       BYTES bytes, BYTES buffer_size);

// __nanosec read_randomly(FILE *file, BYTES bytes, BYTES buffer_size, BYTES lower_read_limit, BYTES upper_read_limit);

#endif
