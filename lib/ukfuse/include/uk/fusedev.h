#ifndef __UK_FUSE_DEV__
#define __UK_FUSE_DEV__

#ifdef __cplusplus
extern "C" {
#endif

#include "uk/fusedev_core.h"
#include "uk/fusedev_trans.h"
#include "uk/fusereq.h"
#include <stddef.h>
#include <uk/alloc.h>

void uk_fusedev_req_to_freelist(struct uk_fuse_dev *dev,
				struct uk_fuse_req *req);
struct uk_fuse_req *uk_fusedev_req_create(struct uk_fuse_dev *dev);
int uk_fusedev_req_remove(struct uk_fuse_dev *dev, struct uk_fuse_req *req);
int uk_fusedev_request(struct uk_fuse_dev *dev, struct uk_fuse_req *req);
void uk_fusedev_xmit_notify(struct uk_fuse_dev *dev);
struct uk_fuse_dev *uk_fusedev_connect(const struct uk_fusedev_trans *trans,
				       const char *device_identifier,
				       struct uk_alloc *a);
int uk_fusedev_disconnect(struct uk_fuse_dev *dev);



#ifdef __cplusplus
}
#endif

#endif /* __UK_FUSE_DEV__ */
