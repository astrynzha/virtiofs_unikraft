#include "uk/fusedev_trans.h"
#include "uk/fusedev_core.h"
#include <uk/print.h>
#include <uk/assert.h>

static UK_LIST_HEAD(uk_fusedev_trans_list);

static struct uk_fusedev_trans *uk_fusedev_trans_saved_default;

int uk_fusedev_trans_register(struct uk_fusedev_trans *trans)
{
	UK_ASSERT(trans);
	UK_ASSERT(trans->name);
	UK_ASSERT(trans->ops);
	// UK_ASSERT(trans->ops->connect);
	// UK_ASSERT(trans->ops->disconnect);
	UK_ASSERT(trans->ops->request);
	UK_ASSERT(trans->a);

	uk_list_add_tail(&trans->_list, &uk_fusedev_trans_list);

	if (!uk_fusedev_trans_saved_default)
		uk_fusedev_trans_saved_default = trans;

	uk_pr_info("Registered transport %s\n", trans->name);

	return 0;
}

struct uk_fusedev_trans *uk_fusedev_trans_get_default(void)
{
	return uk_fusedev_trans_saved_default;
}