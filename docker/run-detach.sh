#!/bin/bash

sudo podman run \
	--rm \
	-d \
	--replace \
	--privileged \
	--name pastrami-builder \
	-v ../:/opt/pastrami \
	-it localhost/pastrami-builder
