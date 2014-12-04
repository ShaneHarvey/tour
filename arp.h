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
/* program headers */
#include "get_hw_addrs.h"
#include "api.h"
#include "common.h"
#include "debug.h"
#include "cache.h"

/* Used as the ethernet frame type */
#define ETH_P_ODR 0xF31F

int create_unix_domain(void);
int create_pf_socket(void);
int run_arp(int unix_domain, int pf_socket, struct hwa_info *devices);
ssize_t recv_frame(int pf_socket, struct ethhdr *eh, struct arpreq *recvmsg,
                   struct sockaddr_ll *src);
void ntoh_msg(struct arpreq *msg);

bool isDestination(struct hwa_info *devices, Cache *cache);

int maxfd(int pf_socket, int unix_domain, Cache *cache);

/* Signal handling for cleanup */
void cleanup(int signum);
void set_sig_cleanup(void);

#endif
