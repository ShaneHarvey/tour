#ifndef TOUR_H
#define TOUR_H
#include <time.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <arpa/inet.h>

/* Used as the tour IP protocol type */
#define IPPROTO_TOUR 237

struct tourhdr {
    struct in_addr mcastip; /* The multicast IP to join */
    short mcastport;        /* The multicast port to join */
    short len;              /* Number of IPs in the ips list */
    struct in_addr ips[];   /* Variable size list of IP addresses */
};
int valid_args(int argc, char **argv);
int start_tour(int rt, int udp, int argc, char **argv);
int run_tour(int rt, int pg, int udp, int binded);
int end_tour(int udp);

#endif
