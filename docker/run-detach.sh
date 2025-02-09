#!/bin/bash

sudo podman run \
	--rm \
	-d \
	--replace \
	--privileged \
	--name qlearning-builder \
	-v ../:/opt/qlearning \
	-it localhost/qlearning-builder
