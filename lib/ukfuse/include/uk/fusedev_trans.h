#ifndef __UK_FUSEDEV_TRANS__
#define __UK_FUSEDEV_TRANS__

#include "uk/fusedev_core.h"
#include <uk/list.h>
#include <uk/alloc.h>

#ifdef __cplusplus
extern "C" {
#endif

/** A structure used to describe a transport. */
struct uk_fusedev_trans {
	const char				*name;
	/* Supported operations. */
	const struct uk_fusedev_trans_ops	*ops;
	/* Allocator used for devices which use this transport layer. */
	struct uk_alloc				*a;
	/* @internal Entry in the list of available transports. */
	struct uk_list_head			_list;
};

int uk_fusedev_trans_register(struct uk_fusedev_trans *trans);
struct uk_fusedev_trans *uk_fusedev_trans_get_default(void);


#ifdef __cplusplus
}
#endif

#endif /* __UK_FUSEDEV_TRANS__ */
