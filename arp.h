#ifndef ARP_H
#define ARP_H
#include "get_hw_addrs.h"
#include "api.h"

/* Signal handling for cleanup */
void cleanup(int signum);
void set_sig_cleanup(void);

#endif
