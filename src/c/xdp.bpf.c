#include <vmlinux.h>
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#define ETH_P_IP	0x0800 /* Internet Protocol v4 */
#define ETH_P_IP6	0x86dd /* Internet Protocol v6 */

#ifdef DEBUG
#define BPF_PRINTK_DEBUG(...)		\
do {					\
	bpf_printk(__VA_ARGS__);	\
} while(0)
#else
#define BPF_PRINTK_DEBUG(...)		\
do {					\
	(void)(1);			\
} while(0)
#endif

#ifndef memcpy
#define memcpy(dest, src, n)   __builtin_memcpy((dest), (src), (n))
#endif

#define ETH_ALEN 6
struct fib_elem {
	__u32 ifindex;
	char h_dest[ETH_ALEN];
	char h_source[ETH_ALEN];
};

#define HMAP_MAX_ENTRIES 16
struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, HMAP_MAX_ENTRIES);
	__type(key, __be32);
	__type(value, struct fib_elem);
} fibtable SEC(".maps");

SEC("__xdp_redirect")
int xdp_redirect(struct xdp_md *ctx)
{
	void *data_end = (void *)(__u64)ctx->data_end;
	void *data = (void *)(__u64)ctx->data;
	struct fib_elem *fe;
	struct ethhdr *eth;
	int iif, oif;
	int rc;

	BPF_PRINTK_DEBUG("xdp_redirect: xdp_redirect invoked");

	eth = data;
	if ((void *)(eth + 1) > data_end)
		return XDP_PASS;

	if (eth->h_proto != bpf_htons(ETH_P_IP6))
		/* do not process packets which are not IPv6 */
		return XDP_PASS;

	/* ingress interface */
	iif = ctx->ingress_ifindex;

	fe = bpf_map_lookup_elem(&fibtable, &iif);
	if (!fe) {
		BPF_PRINTK_DEBUG("xdp_redirect: fib for ifname=%d not found", iif);
		return XDP_PASS;
	}

	/* egress interface */
	oif = fe->ifindex;

	/* fib_elem found */
	memcpy(eth->h_dest, fe->h_dest, ETH_ALEN);
	memcpy(eth->h_source, fe->h_source, ETH_ALEN);

	BPF_PRINTK_DEBUG("xdp_redirect: from iif=%d to oif=%d", iif, oif);

	rc = bpf_redirect(oif, 0);
	if (rc != XDP_REDIRECT)
		BPF_PRINTK_DEBUG("xdp_redirect: bpf_redirect rc=%d", rc);

	return rc;
}

SEC("__xdp_pass")
int xdp_pass(struct xdp_md *ctx)
{
	return XDP_PASS;
}

char _license[] SEC("license") = "GPL";
