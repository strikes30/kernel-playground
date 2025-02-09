#!/bin/bash

set -e
set -x
set -u

TMUX=ipv6
IPP=ip

# Kill tmux previous session
tmux kill-session -t $TMUX 2>/dev/null || true

# Clean up previous network namespaces
$IPP -all netns delete

$IPP netns add h0
$IPP netns add h1
$IPP netns add r0


$IPP link add veth0 type veth peer name veth1
$IPP link add veth2 type veth peer name veth3

$IPP link set veth0 netns h0
$IPP link set veth1 netns r0
$IPP link set veth2 netns r0
$IPP link set veth3 netns h1

###################
#### Node: h0 #####
###################
echo -e "\nNode: h0"
$IPP netns exec h0 $IPP link set dev lo up
$IPP netns exec h0 $IPP link set dev veth0 up
$IPP netns exec h0 $IPP addr add 10.0.0.1/24 dev veth0
$IPP netns exec h0 $IPP addr add cafe::1/64 dev veth0

$IPP netns exec h0 $IPP -6 route add default via cafe::254 dev veth0
$IPP netns exec h0 $IPP -4 route add default via 10.0.0.254 dev veth0

###################
#### Node: r0 #####
###################
echo -e "\nNode: r0"

$IPP netns exec r0 sysctl -w net.ipv4.ip_forward=1
$IPP netns exec r0 sysctl -w net.ipv6.conf.all.forwarding=1
$IPP netns exec r0 sysctl -w net.ipv4.conf.all.rp_filter=0
$IPP netns exec r0 sysctl -w net.ipv4.conf.veth1.rp_filter=0
$IPP netns exec r0 sysctl -w net.ipv4.conf.veth2.rp_filter=0

$IPP netns exec r0 $IPP link set dev lo up
$IPP netns exec r0 $IPP link set dev veth1 up
$IPP netns exec r0 $IPP link set dev veth2 up

$IPP netns exec r0 $IPP addr add cafe::254/64 dev veth1
$IPP netns exec r0 $IPP addr add 10.0.0.254/24 dev veth1

$IPP netns exec r0 $IPP addr add dead::254/64 dev veth2
$IPP netns exec r0 $IPP addr add 10.0.2.254/24 dev veth2

# TC
# ===
$IPP netns exec r0 tc qdisc add dev veth2 root handle 1: prio
$IPP netns exec r0 tc qdisc add dev veth2 \
	parent 1:3 handle 30: tbf rate 100mbit buffer 1600 limit 3000
$IPP netns exec r0 tc qdisc add dev veth2 \
	parent 30:1 handle 31: netem delay 10ms rate 10mbit

$IPP netns exec r0 tc filter add dev veth2 \
	protocol ip parent 1: u32 \
	match ip protocol 17 0xff \
	action bpf obj netprog.bpf.o sec tcf \
	flowid 1:3

# HTB
#$IPP netns exec r0 tc qdisc add dev veth2 root handle 1: htb default 10
#$IPP netns exec r0 tc class add dev veth2 \
#	parent 1: classid 1:1 htb rate 100mbit
#$IPP netns exec r0 tc class add dev veth2 \
#	parent 1:1 classid 1:10 htb rate 50mbit ceil 50mbit
#$IPP netns exec r0 tc class add dev veth2 \
#	parent 1:1 classid 1:20 htb rate 30mbit ceil 30mbit

# SFQ under HTB classes
#$IPP netns exec r0 tc qdisc add dev veth2 \
#	parent 1:10 handle 10: sfq
#$IPP netns exec r0 tc qdisc add dev veth2 \
#	parent 1:20 handle 20: sfq

# Step 3: Add the filter for IP protocol, TCP, and destination port 5555
#$IPP netns exec r0 tc filter add dev veth2 \
#	parent 1: u32 \
#	match ip protocol 6 0xff \
#	match ip dport 5555 0xffff \
#	action bpf obj kernel.bpf.o sec __tc_pass flowid 1:20

# Filters
#$IPP netns exec r0 tc filter add dev veth2 \
#	parent 1: protocol ip matchall flowid 1:1
#$IPP netns exec r0 tc filter add dev veth2 \
#	parent 1:1 bpf da obj kernel.bpf.o sec __tc_pass

# XXX: why does it drop packet when we add a filter with tc-ebf to a sub-qdisc?
#$IPP netns exec r0 tc filter add dev veth2 \
#	parent 10: bpf obj kernel.bpf.o sec __tc_pass

set +e
read -r -d '' r0_env <<-EOF
        mount -t tracefs nodev /sys/kernel/tracing

        # It allows to load maps with many entries without failing
        ulimit -l unlimited

        /bin/bash
EOF
set -e

###################
#### Node: h1 #####
###################
echo -e "\nNode: h1"
$IPP netns exec h1 $IPP link set dev lo up
$IPP netns exec h1 $IPP link set dev veth3 up
$IPP netns exec h1 $IPP addr add 10.0.2.1/24 dev veth3
$IPP netns exec h1 $IPP addr add dead::1/64 dev veth3

$IPP netns exec h1 $IPP -4 route add default via 10.0.2.254 dev veth3
$IPP netns exec h1 $IPP -6 route add default via dead::254 dev veth3

## Create a new tmux session
tmux new-session -d -s $TMUX -n h0 $IPP netns exec h0 bash
tmux new-window -t $TMUX -n r0 $IPP netns exec r0 bash -c "${r0_env}"
tmux new-window -t $TMUX -n h1 $IPP netns exec h1 bash
tmux select-window -t :0
tmux set-option -g mouse on
tmux attach -t $TMUX
