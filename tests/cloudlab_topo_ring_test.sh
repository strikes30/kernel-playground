#!/bin/bash

#                     +------------------+      +------------------+
#                     |        r0        |      |        r1        |
#                     |                  |      |                  |
#                     |       vrf0       |      | xdp_redirect     |
#                  fd00::1/64   *veth0 +----------+ veth0    fd00::2/64
#                     |        xdp_pass  |      |                  |
#                     |        xdp_pass  |      |                  |
#                  fd01::1/64   *veth1 +----------+ veth1    fd01::2/64
#                     |       vrf1       |      | xdp_redirect     |
#                     |                  |      |                  |
#                     +------------------+      +------------------+
#

set -x
set -e

readonly BPFTOOL="../bpftool/src/bpftool"
readonly OBJ_PATH="../src/c/.output/"
readonly BPFFS_PATH="/sys/fs/bpf"

readonly TMUX=ring

if ! command "${BPFTOOL}" &>/dev/null; then
	echo "bpftool program is not available"
	exit 1
fi

# Kill tmux previous session
tmux kill-session -t $TMUX 2>/dev/null | true

# Clean up previous network namespaces
#ip -all netns delete
ip netns del r0 | true
ip netns del r1 | true

ip netns add r0
ip netns add r1

ip link add veth0 netns r0 type veth peer name veth0 netns r1
ip link add veth1 netns r0 type veth peer name veth1 netns r1

###################
#### Node: r0 #####
###################
echo -e "\nNode: r0"
#ip netns exec r0 sysctl -w net.ipv4.ip_forward=1
#ip netns exec r0 sysctl -w net.ipv6.conf.all.forwarding=1

ip netns exec r0 sysctl -w net.ipv4.tcp_l3mdev_accept=1
ip netns exec r0 sysctl -w net.ipv4.udp_l3mdev_accept=1

ip -netns r0 link set dev lo up

ip -netns r0 link add vrf0 type vrf table 100
ip -netns r0 link add vrf1 type vrf table 101
ip -netns r0 link set dev vrf0 up
ip -netns r0 link set dev vrf1 up

ip -netns r0 link set dev veth0 master vrf0
ip -netns r0 link set dev veth1 master vrf1

ip -netns r0 link set dev veth0 up
ip -netns r0 link set dev veth1 up

ip -netns r0 addr add fd00::1/64 dev veth0
ip -netns r0 addr add fd01::1/64 dev veth1

ip -netns r0 link set dev veth0 address 00:00:00:00:01:01
ip -netns r0 link set dev veth1 address 00:00:00:00:11:01

ip -netns r0 route add fd01::/64 via fd00::2 vrf vrf0
ip -netns r0 route add fd00::/64 via fd01::2 vrf vrf1

ip -netns r0 neigh add fd00::2 lladdr 00:00:00:00:01:02 dev veth0
ip -netns r0 neigh add fd01::2 lladdr 00:00:00:00:11:02 dev veth1

set +e
read -r -d '' r0_env <<-EOF
	mount -t bpf bpf "${BPFFS_PATH}"
	mount -t tracefs nodev /sys/kernel/tracing

	# It allows to load maps with many entries without failing
	ulimit -l unlimited

	mkdir "${BPFFS_PATH}"/{progs,maps}

	${BPFTOOL} prog loadall \
		"${OBJ_PATH}/xdp.bpf.o" /sys/fs/bpf/progs \
		pinmaps /sys/fs/bpf/maps \
		type xdp

	# required otherwise redirect (on the other peer) for veth won't work
	${BPFTOOL} net attach xdpdrv \
		pinned "${BPFFS_PATH}/progs/xdp_pass" \
		dev veth0

	# required otherwise redirect (on the other peer) for veth won't work
	${BPFTOOL} net attach xdpdrv \
		pinned "${BPFFS_PATH}/progs/xdp_pass" \
		dev veth1

	# ip vrf exec vrf0 ping fd01::1
	/bin/bash
EOF
set -e

###################
#### Node: r1 #####
###################
echo -e "\nNode: r1"
#ip netns exec r1 sysctl -w net.ipv4.ip_forward=1
ip netns exec r1 sysctl -w net.ipv6.conf.all.forwarding=1

ip -netns r1 link set dev lo up
ip -netns r1 link set dev veth0 up
ip -netns r1 link set dev veth1 up

ip -netns r1 addr add fd00::2/64 dev veth0
ip -netns r1 addr add fd01::2/64 dev veth1

ip -netns r1 link set dev veth0 address 00:00:00:00:01:02
ip -netns r1 link set dev veth1 address 00:00:00:00:11:02

ip -netns r1 neigh add fd00::1 lladdr 00:00:00:00:01:01 dev veth0
ip -netns r1 neigh add fd01::1 lladdr 00:00:00:00:11:01 dev veth1

set +e
read -r -d '' r1_env <<-EOF
	set -x

	mount -t bpf bpf "${BPFFS_PATH}"
	mount -t tracefs nodev /sys/kernel/tracing

	# It allows to load maps with many entries without failing
	ulimit -l unlimited

	mkdir "${BPFFS_PATH}"/{progs,maps}

	${BPFTOOL} prog loadall \
		"${OBJ_PATH}/xdp.bpf.o" /sys/fs/bpf/progs \
		pinmaps /sys/fs/bpf/maps \
		type xdp

	# format
	# key: iif
	# value: oif \ dst mac \ src mac
	${BPFTOOL} map update pinned "${BPFFS_PATH}/maps/fibtable" \
		key hex		02 00 00 00		\
		value hex	03 00 00 00		\
				00 00 00 00 11 01	\
				00 00 00 00 11 02
	# format
	# key: iif
	# value: oif \ dst mac \ src mac
	${BPFTOOL} map update pinned "${BPFFS_PATH}/maps/fibtable" \
		key hex		03 00 00 00		\
		value hex	02 00 00 00		\
				00 00 00 00 01 01	\
				00 00 00 00 01 02

	${BPFTOOL} net attach xdpdrv \
		pinned "${BPFFS_PATH}/progs/xdp_redirect" \
		dev veth0

	${BPFTOOL} net attach xdpdrv \
		pinned "${BPFFS_PATH}/progs/xdp_redirect" \
		dev veth1

	/bin/bash
EOF
set -e

sleep 1

## Create a new tmux session
tmux new-session -d -s $TMUX -n r0 ip netns exec r0 bash -c "${r0_env}"
tmux new-window -t $TMUX -n r1 ip netns exec r1 bash -c "${r1_env}"
tmux set-option -g mouse on
tmux select-window -t r0
tmux attach -t $TMUX
