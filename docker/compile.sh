#!/bin/bash

sudo podman run \
	--rm \
	--replace \
	--name pastrami-builder \
	-v ../:/opt/pastrami \
	-t localhost/pastrami-builder \
	bash -c "cd /opt/pastrami && ./init.sh"
