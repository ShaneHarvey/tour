#ifndef ARP_H
#define ARP_H
#include <signal.h>
/* System headers */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include "get_hw_addrs.h"
#include "api.h"
#include "common.h"
#include "debug.h"

/* Used as the ethernet frame type */
#define ETH_P_ODR 0xF31F

int create_unix_domain(void);
int create_pf_socket(void);
int run_arp(int unix_domain, int pf_socket, struct hwa_info *devices);

/* Signal handling for cleanup */
void cleanup(int signum);
void set_sig_cleanup(void);

#endif
