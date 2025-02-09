#!/bin/bash

sudo podman run \
	--rm \
	--replace \
	--name qlearning-builder \
	-v ../:/opt/qlearning \
	-t localhost/qlearning-builder \
	bash -c "cd /opt/qlearning && ./init.sh"
