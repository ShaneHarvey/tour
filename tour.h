#ifndef TOUR_H
#define TOUR_H
#include "get_hw_addrs.h"
#include <time.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>

/* Used as the ethernet frame type */
#define ETH_P_ODR 0xF31F

/* Signal handling for cleanup */
void cleanup(int signum);
void set_sig_cleanup(void);

#endif
