#ifndef PING_H
#define PING_H
#include <time.h>
#include <sys/queue.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <pthread.h>
#include "get_hw_addrs.h"

#define ICMP_ID 0xDEAC

#define ICMP_DATA_LEN 8
#define ICMP_ECHO_DATA "PINGPONG"

struct pingarg {
    struct hwa_info src;
    struct in_addr tgtip;
};

/* Used to manage a list of pinging threads */
LIST_HEAD(head_pingt, pingt);

struct pingt {
    struct pingarg arg;     /* Arguments for the pinging thread */
    pthread_t tid;          /* Ping thread id */
    LIST_ENTRY(pingt) list; /* Doubly linked list */
};

void *run_ping_send(void *arg);
void *run_ping_recv(void *unused);

int send_frame(int sock, void *payload, int size, unsigned char *dst_mac,
        unsigned char *src_mac, int ifi_index);
uint16_t in_cksum(uint16_t *addr, int len);

int create_pingt(struct head_pingt *head, struct in_addr tgtip);
int destroy_pingt(struct head_pingt *head);

void print_mac(unsigned char *mac);
#endif
