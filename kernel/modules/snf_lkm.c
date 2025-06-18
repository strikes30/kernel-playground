#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/atomic.h>
#include <linux/tcp.h>
#include <linux/udp.h>

static atomic_t tcp_counter = ATOMIC_INIT(0);
static atomic_t udp_counter = ATOMIC_INIT(0);
static atomic_t port_counter[65536];
static atomic_t total_counter = ATOMIC_INIT(0);

static struct nf_hook_ops nfho;

static struct proc_dir_entry *proc_tcp;
static struct proc_dir_entry *proc_udp;

static unsigned int packet_counter_hook(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
    struct iphdr *iph;
    int dest_port = 0;

    if (!skb)
	{
        return NF_ACCEPT;
	}

    iph = ip_hdr(skb);
    
	if (!iph)
	{
        return NF_ACCEPT;
	}
    
	if (iph->protocol == IPPROTO_TCP) 
	{   //pacchetto TCP
        struct tcphdr *tcph = (struct tcphdr *)((__u8 *)iph + iph->ihl * 4);   //header, altrimenti non può prendere la porta
        dest_port = ntohs(tcph->dest);
        atomic_inc(&tcp_counter);
        printk(KERN_INFO "TCP dest port: %u\n", dest_port);
    } 
	else if (iph->protocol == IPPROTO_UDP) 
	{    //pacchetto UDP
        struct udphdr *udph = (struct udphdr *)((__u8 *)iph + iph->ihl * 4);   //header, altrimenti non può prendere la porta
        dest_port = ntohs(udph->dest);
        atomic_inc(&udp_counter);
        printk(KERN_INFO "UDP dest port: %u\n", dest_port);
    }

    if (dest_port < 65536) 
	{
        atomic_inc(&port_counter[dest_port]);
	}

	int total = atomic_inc_return(&total_counter);
	if (total % 100 == 0) 
	{
        printk(KERN_INFO "packet_counter: raggiunti %d pacchetti totali\n", total);
    }

    return NF_ACCEPT;
}

static int port_show(struct seq_file *m, void *v)
{
    int i;
    for (i = 0; i < 65536; ++i) 
	{
        int count = atomic_read(&port_counter[i]);
        if (count > 0) 
		{
            seq_printf(m, "Porta %d: %d pacchetti\n", i, count);
		}
    }
    return 0;
}

static int port_open(struct inode *inode, struct file *file)
{
    return single_open(file, port_show, NULL);
}

static int tcp_show(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", atomic_read(&tcp_counter));
    return 0;
}

static int udp_show(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", atomic_read(&udp_counter));
    return 0;
}

static int tcp_open(struct inode *inode, struct file *file)
{
    return single_open(file, tcp_show, NULL);
}

static int udp_open(struct inode *inode, struct file *file)
{
    return single_open(file, udp_show, NULL);
}

static const struct proc_ops tcp_proc_fops = {
    .proc_open    = tcp_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static const struct proc_ops udp_proc_fops = {
    .proc_open    = udp_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static const struct proc_ops port_proc_fops = {
    .proc_open    = port_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static int __init packet_counter_init(void)
{
    // Configura il Netfilter hook
    nfho.hook = packet_counter_hook;
    nfho.hooknum = NF_INET_PRE_ROUTING;  // intercetta i pacchetti prima del routing
    nfho.pf = PF_INET;                   // famiglia IPv4
    nfho.priority = NF_IP_PRI_FIRST;    // priorità alta

    // Registra il Netfilter hook
    nf_register_net_hook(&init_net, &nfho);

    // Crea le entry in /proc
    proc_tcp = proc_create("tcp_packets", 0444, NULL, &tcp_proc_fops);
    proc_udp = proc_create("udp_packets", 0444, NULL, &udp_proc_fops);
    proc_create("port_packets", 0444, NULL, &port_proc_fops);

    printk(KERN_INFO "packet_counter: modulo caricato\n");
    return 0;
}

static void __exit packet_counter_exit(void)
{
    // Rimuove il Netfilter hook
    nf_unregister_net_hook(&init_net, &nfho);

    // Rimuove le entry in /proc
    if (proc_tcp) 
	{
        proc_remove(proc_tcp);
	}
    if (proc_udp) 
	{
        proc_remove(proc_udp);
	}
		remove_proc_entry("port_packets", NULL);

    printk(KERN_INFO "packet_counter: modulo rimosso\n");
}

module_init(packet_counter_init);
module_exit(packet_counter_exit);

MODULE_LICENSE("GPL");



