#ifndef TOUR_H
#define TOUR_H
#include <time.h>
#include <limits.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/ip.h>

/* Used as the tour IP protocol type */
#define IPPROTO_TOUR 237
#define IPID_TOUR 0xF31F

struct tourhdr {
    struct in_addr mcastip; /* The multicast IP to join */
    short mcastport;        /* The multicast port to join */
    short len;              /* Number of IPs in the ips list */
    struct in_addr ips[];   /* Variable size list of IP addresses */
};

#define TOUR_MAXPACKET (IP_MAXPACKET - sizeof(struct ip))
#define TOUR_MAXIPS ((TOUR_MAXPACKET - sizeof(struct tourhdr)) / sizeof(struct in_addr))
#define TOUR_SIZE(ptr) (sizeof(struct tourhdr) + ((ptr)->len)*sizeof(struct in_addr))

int valid_args(int argc, char **argv);
int start_tour(int rt, int udp, int numhosts, char **argv);
int run_tour(int rt, int udp, int binded);
int end_tour(int udp);

/* Helper functions */
int forward_tour(int rt, struct tourhdr *tour);
int send_ip(int rt, void *data, size_t len, struct in_addr dstip);
int mcast_join(int sockfd, struct sockaddr_in *mcastaddr, int ifindex);
int bind_port(int sockfd, int port);
int getipbyhost(char *hostname, struct in_addr *hostip);

#endif
