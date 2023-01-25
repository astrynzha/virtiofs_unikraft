# Virtiofs Implementation in Unikraft
This is a fork of the [Unikraft](https://github.com/unikraft/unikraft) OS with an implemented kernel layer for the [virtiofs](https://virtio-fs.gitlab.io/) shared-file system.

The architecture of the kernel layer is as follows:
<br>
<img src="https://user-images.githubusercontent.com/48807494/214492146-06c6f749-d3f3-499f-b375-eb0187907195.svg" width="700">
<br>
*Figure 1: red - components of the virtiofs kernel layer. Blue - components that existed previously.*


As part of the thesis the ``virtiofs driver`` (``plat/drivers/virtio/virtio_fs.c``) and the ``ukfuse`` (``lib/ukfuse/``) components have been implemented (see Figure 1).

> For a more in-depth explanation and analysis, see the thesis paper [here](https://drive.google.com/file/d/1453lly-Q2c3RjfbIDkTUd-Knvk4T6n8k/view?usp=share_link).

The second major part of the work has been an upgrade of the ``virtio subsystem`` (Figure 1) to support the modern virtio standard for PCI devices, which is what virtiofs is presented by the hypervisor and seen by Unikraft.

The architectural diagram for these changes is as follows:
<br>
<img src="https://user-images.githubusercontent.com/48807494/214496284-516653e7-06d6-411c-8d87-aa052194df7a.svg" width="700">
<br>
*Figure 2: red color denotes changes made to the 'virtio subsystem'.*

The ``virtio_pci_modern.c`` (``plat/drivers/virtio/virtio_pci_modern.c``) component has been added and is used instead of the legacy ``virtio_pci.c`` for modern virtio PCI devices. <br>
Furthermore, additional functionality for scanning of PCI capability lists has been added to the ``pci_bus_x86.c`` (``plat/common/x86/pci_bus_x86.c``), in order to work with the modern virtio devices.

> For a more in-depth explanation and analysis, see the thesis paper [here](https://drive.google.com/file/d/1453lly-Q2c3RjfbIDkTUd-Knvk4T6n8k/view?usp=share_link).

## Benchmarking
Since the integration into the VFS (``libvirtiofs``, Figure 1) has not been implemented yet, we could not use existin benchmark applications. Therefore, we implemented a custom set of benchmarks for common file-system operations to evaluate the virtiofs performance. The benchmark code and results can be found in [this](https://github.com/astrynzha/unikraft_9p_measure) repo. 

___
## Resources
- [Thesis Paper](https://drive.google.com/file/d/1453lly-Q2c3RjfbIDkTUd-Knvk4T6n8k/view?usp=share_link)
- [Benchmarks](https://github.com/astrynzha/unikraft_9p_measure)
- [Unikraft Github Repo](https://github.com/unikraft/unikraft)
- [Unikraft Official Website](https://unikraft.io/)
- [Virtiofs Official Website](https://virtio-fs.gitlab.io/)
