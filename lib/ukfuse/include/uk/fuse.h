#ifndef __LINUX_FUSE_H__
#define __LINUX_FUSE_H__

#include "uk/fusereq.h"
#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE_4k 4096

typedef struct
{
	bool is_dir;

	uint64_t nodeid;
	/* file handle */
	uint64_t fh;
} fuse_file_context;

int uk_fusereq_receive_cb(struct uk_fuse_req *req, uint32_t len);
int test_method();

#endif /* __LINUX_FUSE_H__ */