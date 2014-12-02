#ifndef PING_H
#define PING_H
#include <time.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <pthread.h>
#include "get_hw_addrs.h"

struct pingarg {
    struct hwa_info src;
    struct in_addr tgtip;
};
void *run_ping(void *arg);
int send_frame(int sock, void *payload, int size, unsigned char *dst_mac,
        unsigned char *src_mac, int ifi_index);
#endif
