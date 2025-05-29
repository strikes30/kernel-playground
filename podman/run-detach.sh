#!/bin/bash

sudo podman run \
	--rm \
	-d \
	--replace \
	--privileged \
	--name kernel-builder \
	-v ../:/opt/kernel-playground \
	-it localhost/kernel-builder
