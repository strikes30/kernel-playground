#!/bin/bash

sudo podman run \
	--rm \
	--replace \
	--name kernel-builder \
	-v ../:/opt/kernel \
	-t localhost/kernel-builder \
	bash -c "cd /opt/kernel && ./init.sh"
