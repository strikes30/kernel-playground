#!/bin/bash

sudo podman run \
	--rm \
	--replace \
	--name kernel-builder \
	-v ../:/opt/kernel-playground \
	-t localhost/kernel-builder \
	bash -c "cd /opt/kernel-playground/podman && ./helper-init.sh"
