// SPDX-License-Identifier: GPL-2.0-only
/* Goodix netlink ABI: protocol 25, 32-byte payload, event in byte zero.
 * Derived from the GPL Goodix/Oplus reference, Copyright 2018-2020 Oplus.
 */
#include <linux/module.h>
#include <net/sock.h>
#include <net/netlink.h>
#include "gf_spi.h"
#define MAX_MSGSIZE 32
static u32 portid;
static struct sock *nl_sk;

int sendnlmsg(char *msg)
{
    struct sk_buff *skb;
    struct nlmsghdr *nlh;
    u32 dest = READ_ONCE(portid);
    int ret;
    if (!msg || !nl_sk || !dest)
        return -ENODEV;
    skb = nlmsg_new(MAX_MSGSIZE, GFP_KERNEL);
    if (!skb)
        return -ENOMEM;
    nlh = nlmsg_put(skb, 0, 0, 0, MAX_MSGSIZE, 0);
    if (!nlh) {
        kfree_skb(skb);
        return -EMSGSIZE;
    }
    /* The reference copied one byte into uninitialised payload memory. */
    memset(nlmsg_data(nlh), 0, MAX_MSGSIZE);
    *(char *)nlmsg_data(nlh) = *msg;
    NETLINK_CB(skb).portid = 0;
    NETLINK_CB(skb).dst_group = 0;
    ret = netlink_unicast(nl_sk, skb, dest, MSG_DONTWAIT);
    return ret < 0 ? ret : 0;
}
static void nl_data_ready(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    if (skb->len < NLMSG_HDRLEN)
        return;
    nlh = nlmsg_hdr(skb);
    if (!nlmsg_ok(nlh, skb->len))
        return;
    /* The caller (netlink_unicast_kernel) consumes this skb after input.
     * We do not take skb_get(), so freeing it here would double-release it.
     * Bind to the actual sending socket, not a spoofed header.
     */
    WRITE_ONCE(portid, NETLINK_CB(skb).portid);
}
int netlink_init(void)
{
    struct netlink_kernel_cfg cfg = { .input = nl_data_ready };
    nl_sk = netlink_kernel_create(&init_net, NETLINK_TEST, &cfg);
    return nl_sk ? 0 : -ENOMEM;
}
void netlink_exit(void)
{
    if (nl_sk)
        netlink_kernel_release(nl_sk);
    nl_sk = NULL;
    WRITE_ONCE(portid, 0);
}
