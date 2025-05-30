# Environment Setup and Kernel Compiling Instructions

This guide explains how to build the container image, set up the VM infrastructure, compile a custom kernel and modules, and run the environment using Podman.

---

## Building the Container Image

First, build the Podman image that will be used to setup the environment:

```bash
# ./container-build.sh
```

This script creates a container image containing all necessary tools for the setup process.

---

## Setting Up the Infrastructure

Once the image is built, run the following script to set up the entire environment:

```bash
# ./setup-all.sh
```

This script automates the following steps:

1. **Create the VM with root filesystem**
   Sets up a virtual machine environment with a minimal root filesystem.

2. **Configure and compile the kernel and custom module**
   Applies kernel configuration, then compiles both the kernel and the custom modules.

3. **Link the compiled kernel into the VM submodule**
   Creates a soft link so the VM can use the freshly compiled kernel during the setup.

4. **Copy the custom kernel module into shared VM folder**
   Places the compiled kernel module into the shared folder accessible from within the VM at `/mnt/shared`.

---

## Running the Container

To start the container in detached mode:

```bash
# ./run-detach.sh
```

This will run the environment in the background, allowing you to interact with it later.

---

## Accessing the Container

To enter the running container's shell:

```bash
# podman exec -it kernel-builder bash
```
---

*Note:* Make sure you have Podman installed and properly configured on your system before starting.
