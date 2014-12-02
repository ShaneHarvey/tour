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
/* Program headers */
#include "debug.h"
#include "common.h"

/* ARP request that the API sends to ARP */
struct arpreq {
    struct sockaddr addr;
    socklen_t addrlen;
};

/* ARP Response that ARP sends back to API */
struct hwaddr {
    int             sll_ifindex;    /* Interface number */
    unsigned char   sll_srcaddr[8]; /* Physical layer address of the interface*/
    unsigned short  sll_hatype;     /* Hardware type */
    unsigned char   sll_halen;      /* Length of address */
    unsigned char   sll_dstaddr[8]; /* Physical layer address */
};

int areq(struct sockaddr *ipa, socklen_t len, struct hwaddr *hwa);

#endif
