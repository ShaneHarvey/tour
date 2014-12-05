#ifndef API_H
#define API_H
/* libc headers */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
/* System headers */
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <net/if_arp.h>
/* Program headers */
#include "debug.h"
#include "api.h"
#include "common.h"

/* ARP request that the API sends to ARP */
struct areq {
    struct sockaddr addr;
    socklen_t addrlen;
};

/* ARP Response that ARP sends back to API */
struct hwaddr {
    int             sll_ifindex;    /* Interface number */
    unsigned short  sll_hatype;     /* Hardware type */
    unsigned char   sll_halen;      /* Length of address */
    unsigned char   sll_addr[8]; /* Physical layer address */
};

int areq(struct sockaddr *ipa, socklen_t len, struct hwaddr *hwa);

#endif
