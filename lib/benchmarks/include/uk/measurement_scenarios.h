#ifndef MEASUREMENT_SCENARIOS_H
#define MEASUREMENT_SCENARIOS_H

#include <stddef.h>
#include <stdio.h>

#include "helper_functions.h"
#include "time_functions.h"

/* ukfuse */
#include "uk/vfdev.h"
#include "uk/fusedev_core.h"

__nanosec create_files(struct uk_fuse_dev *fusedev, FILES amount);
__nanosec remove_files(struct uk_fuse_dev *fusedev, FILES amount,
		       int measurement);
__nanosec list_dir(struct uk_fuse_dev *fusedev, FILES file_amount,
		   int measurement);


__nanosec write_seq_fuse(struct uk_fuse_dev *fusedev, BYTES bytes,
		    BYTES buffer_size);
__nanosec write_seq_dax(struct uk_fuse_dev *fusedev, struct uk_vfdev *vfdev,
			BYTES bytes, BYTES buffer_size);
__nanosec write_seq_dax_2(struct uk_fuse_dev *fusedev, struct uk_vfdev *vfdev,
			BYTES bytes, BYTES buffer_size);
__nanosec write_randomly_fuse(struct uk_fuse_dev *fusedev,
			      BYTES bytes, BYTES buffer_size,
			      BYTES interval_len);
__nanosec write_randomly_dax(struct uk_fuse_dev *fusedev,
			     struct uk_vfdev *vfdev, BYTES bytes,
			     BYTES buffer_size, BYTES interval_len);
__nanosec write_randomly_dax_2(struct uk_fuse_dev *fusedev,
			     struct uk_vfdev *vfdev, BYTES bytes,
			     BYTES buffer_size, BYTES interval_len);
// __nanosec write_randomly(FILE *file, BYTES bytes, BYTES buffer_size, BYTES lower_write_limit, BYTES upper_write_limit);

// __nanosec read_seq(FILE *file, BYTES bytes, BYTES buffer_size);
__nanosec read_seq_fuse(struct uk_fuse_dev *fusedev, BYTES bytes, BYTES buffer_size);
__nanosec read_seq_dax(struct uk_fuse_dev *fusedev, struct uk_vfdev *vfdev,
		       BYTES bytes, BYTES buffer_size);
__nanosec read_seq_dax_2(struct uk_fuse_dev *fusedev, struct uk_vfdev *vfdev,
		       BYTES bytes, BYTES buffer_size);
__nanosec read_randomly_fuse(struct uk_fuse_dev *fusedev,
			     BYTES size, BYTES buffer_size,
			     BYTES interval_len);
__nanosec read_randomly_dax(struct uk_fuse_dev *fusedev,
			    struct uk_vfdev *vfdev, BYTES size,
			    BYTES buffer_size, BYTES interval_len);
__nanosec read_randomly_dax_2(struct uk_fuse_dev *fusedev,
			    struct uk_vfdev *vfdev, BYTES size,
			    BYTES buffer_size, BYTES interval_len);
// __nanosec read_randomly(FILE *file, BYTES bytes, BYTES buffer_size, BYTES lower_read_limit, BYTES upper_read_limit);

#endif
