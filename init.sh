#!/bin/bash

set -x

git submodule update --init --recursive

pushd bpftool/src && \
	make clean && make -j$(nproc); popd

pushd src/c && \
	make clean && make -j$(nproc); popd
