#ifndef TOUR_H
#define TOUR_H
#include "get_hw_addrs.h"
#include <sys/select.h>
#include <time.h>
#include <net/ethernet.h>
#include <sys/stat.h>
#include <inttypes.h>

/* Used as the ethernet frame type */
#define ETH_P_ODR 0xF31F

/* Signal handling for cleanup */
void cleanup(int signum);
void set_sig_cleanup(void);

#endif
