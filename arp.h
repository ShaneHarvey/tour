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
#include <netinet/if_ether.h>
/* program headers */
#include "get_hw_addrs.h"
#include "api.h"
#include "common.h"
#include "debug.h"
#include "cache.h"

/* Used as the ethernet frame type */
#define ETH_P_ARP_SH 0xF31F
#define ARP_ID 0xDEAD
/* Max length of and arp packet
 * is 10 byte header plus IPv6 mapped to 8 byte hw address
 * [2|2|2|1|1|2|8|16|8|16] = 68 bytes
 */
#define ARP_MAXLEN 68
#define ARP_HDRLEN 10

#define ARP_MAXHWLEN 8
#define ARP_MAXPRLEN 16
#define ARP_MAXDATALEN ((2 * (ARP_MAXHWLEN)) + (2 * (ARP_MAXPRLEN)))

#define ARP_SHA(ptr) ((ptr)->data)
#define ARP_SPA(ptr) ((ptr)->data + (ptr)->hard_len)
#define ARP_THA(ptr) ((ptr)->data + (ptr)->hard_len + (ptr)->prot_len)
#define ARP_TPA(ptr) ((ptr)->data + 2 * (ptr)->hard_len + (ptr)->prot_len)

#define ARP_DATALEN(ptr) (2 * (ptr)->hard_len + 2 * (ptr)->prot_len)

struct arp_hdr {
        uint16_t id;
        uint16_t hard_type;
        uint16_t prot_type;
        u_char hard_len;
        u_char prot_len;
        uint16_t op;
        u_char data[ARP_MAXDATALEN];
#if 0
        /* Example data payload of IPv4 mapped to Ethernet address */
        char send_eth[6];
        char send_ip[4];
        char target_eth[6];
        char target_ip[4];
#endif
#if 0
        /* Example data payload of IPv6 mapped to Ethernet address */
        char send_eth[6];
        char send_ip[16];
        char target_eth[6];
        char target_ip[16];
#endif
};

int create_unix_domain(void);
int create_pf_socket(void);
int run_arp(int unix_domain, int pf_socket, struct hwa_info *devices);
int handle_areq(int pf_socket, Cache *conn_entry, struct hwa_info *devices);

ssize_t recv_frame(int pf_socket, struct ethhdr *eh, struct arp_hdr *recvmsg,
                   struct sockaddr_ll *src);
void ntoh_arp(struct arp_hdr *arp);
bool isDestination(struct hwa_info *devices, struct sockaddr *addr);
int send_frame(int sock, void *payload, int size, unsigned char *dstmac, unsigned char *srcmac, int ifi_index);

int maxfd(int pf_socket, int unix_domain, Cache *cache);

int build_arp(u_char *start, int size, uint16_t hard_type, uint16_t prot_type,
        u_char hard_len, u_char prot_len, uint16_t op, u_char *send_hwa,
        u_char *send_pro, u_char *target_hwa, u_char *target_pro);
int valid_arp(struct arp_hdr *arp);

int handle_req(int pack_fd, struct ethhdr *eh, struct arp_hdr *arp, struct
        sockaddr_ll *src);

int handle_reply(int pack_fd, struct ethhdr *eh, struct arp_hdr *arp, struct
        sockaddr_ll *src);

/* Signal handling for cleanup */
void cleanup(int signum);
void set_sig_cleanup(void);

#endif
