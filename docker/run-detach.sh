#!/bin/bash

sudo podman run \
	--rm \
	-d \
	--replace \
	--privileged \
	--name kernel-builder \
	-v ../:/opt/kernel \
	-it localhost/kernel-builder
