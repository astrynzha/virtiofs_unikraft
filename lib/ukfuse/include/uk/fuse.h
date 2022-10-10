#ifndef __LINUX_FUSE_H__
#define __LINUX_FUSE_H__

#include "limits.h"
#include "uk/fusereq.h"
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>

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

int uk_fuse_setupmapping(struct uk_fuse_dev *dev, uint64_t nodeid, uint64_t fh,
			 uint64_t foffset, uint64_t len, uint64_t flags,
			 uint64_t moffset);

int uk_fuse_lseek_request(struct uk_fuse_dev *dev, uint64_t nodeid, uint64_t fh,
			  uint64_t offset, uint32_t whence,
			  off_t *offset_out);

int uk_fuse_readdirplus_request(struct uk_fuse_dev *dev, uint64_t buff_len,
				uint64_t nodeid, uint64_t fh,
				struct fuse_dirent *dirents,
				size_t *num_dirents);

int uk_fuse_mkdir_request(struct uk_fuse_dev *dev, uint64_t parent_nodeid,
			  const char *dir_name, uint32_t mode,
			  uint64_t *nodeid, uint64_t *nlookup);

int uk_fuse_forget_request(struct uk_fuse_dev *dev, uint64_t nodeid,
			   uint64_t nlookup);

int uk_fuse_unlink_request(struct uk_fuse_dev *dev, const char *filename,
			   bool is_dir, uint64_t nodeid, uint64_t nlookup,
			   uint64_t parent_nodeid);

int uk_fuse_read_request(struct uk_fuse_dev *dev, uint64_t nodeid, uint64_t fh,
			 uint64_t file_off, uint32_t length,
			 void *out_buf, uint32_t *bytes_transferred);

int uk_fuse_write_request(struct uk_fuse_dev *dev, uint64_t nodeid, uint64_t fh,
			  const void *in_buf, uint32_t length, uint64_t off,
			  uint32_t *bytes_transferred);

int uk_fuse_release_request(struct uk_fuse_dev *dev, bool is_dir,
			    uint64_t nodeid, uint64_t fh);

int uk_fuse_setattr_request(struct uk_fuse_dev *dev, uint64_t nodeid,
			    bool is_dir, uint64_t fh, uint32_t mode,
			    uint64_t last_access_time, uint64_t last_write_time,
			    uint64_t change_time);

int uk_fuse_flush_request(struct uk_fuse_dev *dev, uint64_t nodeid,
			  uint64_t fh);

int uk_fuse_create_request(struct uk_fuse_dev *dev, uint64_t parent,
			   const char *file_name, uint32_t flags,
			   uint32_t mode, uint64_t *nodeid, uint64_t *fh,
			   uint64_t *nlookup);

int uk_fuse_open_request(struct uk_fuse_dev *dev, bool is_dir,
			 uint64_t nodeid, uint64_t *fh, uint32_t flags);

int uk_fuse_lookup_request(struct uk_fuse_dev *dev, uint64_t dir_nodeid,
		   const char *filename, FUSE_LOOKUP_OUT *lookup_out);

int uk_fuse_get_attr_request(struct uk_fuse_dev *dev, uint64_t nodeid,
		     uint64_t file_handle, struct fuse_attr *attr);

int uk_fuse_init_reqeust(struct uk_fuse_dev *dev);

int test_method();

#endif /* __LINUX_FUSE_H__ */