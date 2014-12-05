#ifndef ARP_H
#define ARP_H
#include <signal.h>
/* System headers */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
/* net headers */
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
/* program headers */
#include "get_hw_addrs.h"
#include "api.h"
#include "common.h"
#include "debug.h"
#include "cache.h"

/* Used as the ethernet frame type */
#define ETH_P_ARP_SH 0xF31F
#define ARP_ID 0xDEAD

struct arp_hdr {
        short id;
        short hard_type;
        short prot_type;
        char hard_len;
        char prot_len;
        short op;
        char send_eth[6];
        char send_ip[4];
        char target_eth[6];
        char target_ip[4];
};

int create_unix_domain(void);
int create_pf_socket(void);
int run_arp(int unix_domain, int pf_socket, struct hwa_info *devices);
ssize_t recv_frame(int pf_socket, struct ethhdr *eh, struct areq *recvmsg,
                   struct sockaddr_ll *src);
void ntoh_msg(struct areq *msg);
bool isDestination(struct hwa_info *devices, struct sockaddr *addr);
int send_frame(int sock, void *payload, int size, unsigned char *dstmac, unsigned char *srcmac, int ifi_index);

int maxfd(int pf_socket, int unix_domain, Cache *cache);

void build_arp(struct arp_hdr *hdr, struct areq *req);

/* Signal handling for cleanup */
void cleanup(int signum);
void set_sig_cleanup(void);

#endif
