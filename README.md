# Virtiofs Implementation in Unikraft
This is a fork of the [Unikraft](https://github.com/unikraft/unikraft) OS with an implemented kernel layer for the [virtiofs](https://virtio-fs.gitlab.io/) shared-file system.

# Background (Virtiofs, Guest-Host File-Sharing)
Virtiofs is a file-system technology that allows a guest to mount a file-system physically located on the host. The host also has parallel access to the file-system in parallel. Overall, virtiofs allows multiple guests and the host to securely share a single file-system, located on the host, with a parallel read-write access.

This is an alternative to the remote file-sharing mechanism like NFS or SMTP that can be used by the guests and the host over a virtualized network. However, the virtiofs is more efficient in many ways. A significant one is the use of shared-memory for communication since the guests and the host run on a single machine.

A common use-case for the general guest-host file-sharing is **Development**. During development it is often useful to start the software in a dedicated virtual environment which can be a VM. Then the files have to be brought from the host into the VM. Additionally, some test and log data might need to get transfered between the environment VM and the development host.

# Implementation
Here we describe the two main parts of work that we performed on Unikraft to enable the virtiofs support.

### Virtiofs Kernel Layer

The architecture of the kernel layer is as follows:
<br>
<img src="https://user-images.githubusercontent.com/48807494/214492146-06c6f749-d3f3-499f-b375-eb0187907195.svg" width="700">
<br>
*Figure 1: red - components of the virtiofs kernel layer. Blue - components that existed previously.*


As part of the thesis the ``virtiofs driver`` (``plat/drivers/virtio/virtio_fs.c``) and the ``ukfuse`` (``lib/ukfuse/``) components have been implemented (see Figure 1).

> For a more in-depth explanation and analysis, see the thesis paper [here](https://drive.google.com/file/d/1453lly-Q2c3RjfbIDkTUd-Knvk4T6n8k/view?usp=share_link).

### Virtiofs Subsystem Upgrade

The second major part of the work has been an upgrade of the ``virtio subsystem`` (Figure 1) to support the modern virtio standard for PCI devices, which is what virtiofs is presented as by the hypervisor and seen by Unikraft.

The architectural diagram for these changes is as follows:
<br>
<img src="https://user-images.githubusercontent.com/48807494/214496284-516653e7-06d6-411c-8d87-aa052194df7a.svg" width="700">
<br>
*Figure 2: red color denotes changes made to the 'virtio subsystem'.*

The ``virtio_pci_modern.c`` (``plat/drivers/virtio/virtio_pci_modern.c``) component has been added and is used instead of the legacy ``virtio_pci.c`` for modern virtio PCI devices. <br>
Furthermore, additional functionality for scanning of PCI capability lists has been added to the ``pci_bus_x86.c`` (``plat/common/x86/pci_bus_x86.c``), in order to work with the modern virtio devices.

> For a more in-depth explanation and analysis, see the thesis paper [here](https://drive.google.com/file/d/1453lly-Q2c3RjfbIDkTUd-Knvk4T6n8k/view?usp=share_link).

# Results (Performance Improvement)
We have implemented a custom set of benchmarks ([here](https://github.com/astrynzha/unikraft_9p_measure) and in this repo at ``lib/benchmarks/``) for common file-system operations to evaluate the virtiofs performance. These operations are: sequential/random **read** and **write**; file **creation**, **deletion** and directory **listing**.

With these benchmarks, we have measured the speed of our virtiofs implementation and compared it against the 9pfs file-system (it is a virtiofs alternative that had previously been implemented in Unikraft) and the native Linux host (here, the file operations have been measured on the Linux host directly rather than from the guest through virtiofs/9pfs). Virtiofs has two ways of performing the read and write operations - 'FUSE' and 'DAX' (in the plots).

The results are as follows.

Read:
<br>
<img src="https://user-images.githubusercontent.com/48807494/214696865-b30e1ba9-ae38-4c66-a57b-5735642121f3.svg" width="1000">
<br>

For reads and writes the 'buffer sizes' on the X axis is the amount of data we gave to each POSIX `read` or `write` request. The less data we give to each request, the larger the overall number of reqeusts. This drives down the performance because each request entails an expensive guest-host context switch.

Write:
<br>
<img src="https://user-images.githubusercontent.com/48807494/214696892-61fa7252-8fd8-457d-96af-567861fbab51.svg" width="1000">
<br>


Create/Remove/List:
<br>
<img src="https://user-images.githubusercontent.com/48807494/214696909-e5042053-a91f-4559-9bd8-724c822788f0.svg" width="1000">
<br>


#### The main takeaways are:
- Virtiofs through DAX is significantly faster than 9pfs for buffer sizes < 128 KiB.
  - For example, with 4 KiB buffers virtiofs is faster than 9pfs
    - ~17 times for sequential reads (from 73 MiB/s to 1287 MiB/s).
    - ~106 times for sequential writes (from 12 MiB/s to 947 MiB/s).
- Virtiofs with DAX is faster than the native Linux for smaller buffers
  - This is only due to the fact that the guest is a unikernel, where system calls (fs operations) have less overhead than in a conventional OS like the host's Linux.
- 9pfs is to prefer for:
  - removing operations.
  - reading with buffers > 128 KiB.
- In all other cases virtiofs provides better performance.

> For a more in-depth analysis and discussion of the result, see the thesis paper [here](https://drive.google.com/file/d/1453lly-Q2c3RjfbIDkTUd-Knvk4T6n8k/view?usp=share_link).
___
### Resources
- [Thesis Paper](https://drive.google.com/file/d/1453lly-Q2c3RjfbIDkTUd-Knvk4T6n8k/view?usp=share_link)
- [Custom Benchmarks Used to Measure the Results](https://github.com/astrynzha/unikraft_9p_measure)
- [Unikraft Github Repo](https://github.com/unikraft/unikraft)
- [Unikraft Official Website](https://unikraft.io/)
- [Virtiofs Official Website](https://virtio-fs.gitlab.io/)
