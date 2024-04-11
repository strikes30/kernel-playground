// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
/* Copyright (c) 2022 Hengqi Chen */

#if 1
#include <vmlinux.h>
#include <errno.h>

#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#define TC_ACT_OK		0
#define TC_ACT_SHOT		2

/*   */
#define CLOCK_BOOTTIME		7

#else
#include <time.h>
#include <errno.h>

#include <linux/bpf.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/pkt_cls.h>

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#endif

#define ETH_P_IP  0x0800 /* Internet Protocol packet	*/

#ifndef memset
# define memset(dest, chr, n)   __builtin_memset((dest), (chr), (n))
#endif

#ifndef memcpy
# define memcpy(dest, src, n)   __builtin_memcpy((dest), (src), (n))
#endif

#ifndef memmove
# define memmove(dest, src, n)  __builtin_memmove((dest), (src), (n))
#endif

struct hmap_elem {
	__u64 init;
	__u64 counter;
	struct bpf_timer timer;
};

#define HMAP_MAX_ENTRIES 256
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, HMAP_MAX_ENTRIES);
	__type(key, __u32);
	__type(value, struct hmap_elem);
} hmap SEC(".maps");

enum {
	KEY_PROTO_UNDEF	= 0,
	KEY_PROTO_IP	= 1,
};

struct test_hmap_elem {
	__u32 hit_counter;
};

/* test map used for checking syscal bpf invokation from userland */
#define TEST_HMAP_MAX_ENTRIES 1024
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, TEST_HMAP_MAX_ENTRIES);
	__type(key, __u32);
	__type(value, struct test_hmap_elem);
} test_hmap SEC(".maps");

static __always_inline int update_test_hmap(__u64 counter)
{
	struct test_hmap_elem *val;
	__u32 key;

	key = counter & (TEST_HMAP_MAX_ENTRIES - 1);

	val = bpf_map_lookup_elem(&test_hmap, &key);
	if (!val) {
		struct test_hmap_elem init = {
			.hit_counter = 1,
		};

		return bpf_map_update_elem(&test_hmap, &key, &init, 0);
	}

	__sync_fetch_and_add(&val->hit_counter, 1);

	return 0;
}

#define BPF_TIMER_TIMEOUT ((__u64)5000000000)

static int timer_cb(void *map, int *key, struct hmap_elem *helem)
{
	bpf_printk("timer_cb called, key=%d, helem->counter=%lu",
		   *key, helem->counter);

	return 0;
}

static __always_inline int timer_init(void *map, struct hmap_elem *helem)
{
	int rc;

	rc = bpf_timer_init(&helem->timer, map, CLOCK_BOOTTIME);
	if (rc) {
		bpf_printk("bpf_timer_init() rc=%d", rc);
		return rc;
	}

	rc = bpf_timer_set_callback(&helem->timer, timer_cb);
	if (rc) {
		bpf_printk("bpf_timer_set_callback() rc=%d", rc);
		return rc;
	}

	return 0;
}

/*   */
static __always_inline
struct hmap_elem *hmap_elem_init_or_get(void *map, __u32 *key)
{
	struct hmap_elem *val, init;
	int rc;

	val = bpf_map_lookup_elem(map, key);
	if (val)
		return val;

	/* we need to initialize the element */
	memset(&init, 0, sizeof(init));
	init.counter = 1;

	/* note that updating an hashtable element is an atomic op */
	rc = bpf_map_update_elem(map, key, &init, BPF_NOEXIST);
	if (rc) {
		if (rc == -EEXIST)
			/* another cpu has just created the entry, give up*/
			goto lookup;

		/* other errors are not tolerated */
		return NULL;
	}

lookup:
	val = bpf_map_lookup_elem(map, key);
	if (!val)
		return NULL;

	/* see https://reviews.llvm.org/D72184 */
	if (__sync_lock_test_and_set(&val->init, 1))
		/* already initializei */
		return val;

	/* only a single CPU can be here */

	/* we want initialize a timer only once.
	 * A timer needs to be defined inside a map element which is already
	 * stored in the map. For this reason, we cannot  use a
	 * publish/subscribe approach - e.g. create a map element, initialize
	 * the timer within it and finally update the map with that element.
	 * Publish allows cpus to see the whole map element fully initialized
	 * or not.
	 *
	 * Our approach in this case, is to allow *only* a  CPU to initialized
	 * the timer when a new map element is createde and pushed into the
	 * map. However, in the mean while the CPU is taking the element lock
	 * and initialize the timer, another CPU could reference to that
	 * element finding that timer is not still initialized. We admit this
	 * corner case as it could happen only the first time a new map element
	 * is created inside the map.
	 */
	rc = timer_init(map, val);
	if (rc)
		return NULL;

	return val;
}

SEC("tc")
int tc_ingress(struct __sk_buff *ctx)
{
	void *data_end = (void *)(__u64)ctx->data_end;
	void *data = (void *)(__u64)ctx->data;
	struct hmap_elem *helem;
	struct ethhdr *l2;
	struct iphdr *l3;
	__u32 key;
	int rc;

	if (ctx->protocol != bpf_htons(ETH_P_IP))
		return TC_ACT_OK;

	l2 = data;
	if ((void *)(l2 + 1) > data_end)
		return TC_ACT_OK;

	l3 = (struct iphdr *)(l2 + 1);
	if ((void *)(l3 + 1) > data_end)
		return TC_ACT_OK;

	/* here only if we are handling IP packets */

	bpf_printk("! Got IP packet: tot_len: %d, ttl: %d",
		   bpf_ntohs(l3->tot_len), l3->ttl);

	key = KEY_PROTO_IP;
	helem = hmap_elem_init_or_get(&hmap, &key);
	if (!helem) {
		bpf_printk("invalid hmap_elem object");
		goto drop;
	}

	/* atomic add (increment) */
	__sync_fetch_and_add(&helem->counter, 1);

	/* start timer */
	rc = bpf_timer_start(&helem->timer, BPF_TIMER_TIMEOUT, 0);
	if (rc) {
		if (rc == -EINVAL) {
			/* This use case can be tolerated, as it is very rare.
			 * If we have arrived at this point, it indicates that
			 * two packets are being processed simultaneously on
			 * two different CPUs, and both are attempting to
			 * initialize the corresponding element. However, the
			 * operation is intended to be performed by only one
			 * CPU.
			 * Therefore, it is possible that while one CPU is
			 * initializing the timer, the completed operation may
			 * not yet be visible on the current CPU.
			 */
			bpf_printk("bpf_timer not started yet.");
			goto out;
		}

		bpf_printk("bpf_timer_start() rc=%d", rc);
		goto drop;
	}
out:
	/* fill test map used for retrieving hash entries from userland */
	update_test_hmap(helem->counter);

	return TC_ACT_OK;

drop:
	bpf_printk("Dropped IP packet");
	return TC_ACT_SHOT;
}

char __license[] SEC("license") = "GPL";
