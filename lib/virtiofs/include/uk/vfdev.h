#ifndef __UK_VFDEV__
#define __UK_VFDEV__

#include <uk/fusedev_core.h>
#include <stdint.h>
#include <stdbool.h>

struct uk_vfdev;

/**
 * @brief retrieves wether the DAX window is enabled
 *
 */
typedef bool (*uk_vfdev_dax_enabled)(struct uk_vfdev dev);

/**
 * @brief retrieves the address of the beginning of the DAX window.
 *
 */
typedef uint64_t (*uk_vfdev_get_dax_addr)(struct uk_vfdev dev);

struct uk_vfdev_trans_ops {
	uk_vfdev_dax_enabled	dax_enabled;
	uk_vfdev_get_dax_addr	get_dax_addr;
};

/* TODOFS: one option of how a vfdev device might be modeled.
 * The idea is that the @p ops contains the above two functions, which come
 * from a transport. However, since the transport can be not only pci
 * (but also, MMIO, for example), these operations have to be implemented by
 * a transport. And supplied to vfdev device through @p ops.
 * The question is, how to supply them. This has to be built.
 * One thing to consider is that the transporzt that supplies the @p ops, has
 * to match the transport, that is used by the @p fuse_dev.
 *
 * plat/driversvirtio/virtio_pci_modern.c */
/* virtiofs device */
struct uk_vfdev {
	const struct uk_vfdev_trans_ops *ops;
	/* FUSE device, through which this vfdev send FUSE requests */
	struct uk_fuse_dev		*fuse_dev;

	/* Whether the DAX window can be used.
	   If not, vfdev fallbacks to FUSE_WRITE and FUSE_READ requests. */
	bool				dax_enabled;
	/* Valid only if dax_enabled==TRUE */
	uint64_t			dax_addr;
	/* Length of the DAX window. Valid only if dax_enabled==TRUE */
	uint64_t			dax_len;
};


#endif /* __UK_VFDEV__ */