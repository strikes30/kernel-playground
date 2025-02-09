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

#ifndef TC_ACT_OK
#define TC_ACT_OK 0
#endif

/* statistics provided to TC-eBPF programs
 * TODO: put this structure in a .h file shared between eBPF and kernel
 */
struct bpf_netem_qstats {
        __u32 qlen;
        __u32 backlog;
} __attribute__((preserve_access_index));

int bpf_netem_qstats_read(struct __sk_buff *skb, u32 handler,
                          struct bpf_netem_qstats *qstats) __ksym;

#define HANDLER	0x310000
SEC("tcf")
int tcf_prog(struct __sk_buff *skb)
{
	struct bpf_netem_qstats qstats = { 0, };
	u32 ifindex = skb->ifindex;
	int rc;

	rc = bpf_netem_qstats_read(skb, HANDLER, &qstats);
	if (rc) {
		bpf_printk("bpf_netem_qstats_read err:%d %u@netem handler 0x%x",
			   rc, ifindex, HANDLER);
		goto out;
	}

	bpf_printk("bpf_netem_qstats: %u@netem handler 0x%x, backlog=%u, qlen=%u",
		   ifindex, HANDLER, qstats.backlog, qstats.qlen);
out:
        return TC_ACT_OK;
}

char _license[] SEC("license") = "GPL";
