#ifndef __LINUX_FUSE_H__
#define __LINUX_FUSE_H__

#include "limits.h"
#include "uk/fusereq.h"
#include <stdint.h>
#include <stdbool.h>

#define PAGE_SIZE_4k 4096

typedef struct
{
	bool is_dir;

	char file_name[NAME_MAX + 1];
	uint64_t nodeid;
	uint64_t fh; /* file handle */
	uint64_t parent_nodeid;

	/* TODOFS: think about race conditions, when updating this? */
	uint64_t nlookup;

	uint32_t flags;
	uint32_t mode;
} fuse_file_context;

int uk_fusereq_receive_cb(struct uk_fuse_req *req, uint32_t len);
int test_method();

#endif /* __LINUX_FUSE_H__ */