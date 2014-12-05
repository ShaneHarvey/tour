#ifndef CACHE_H
#define CACHE_H
#include <net/if.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "api.h"

// (i) IP address ;  (ii) HW address ;
// (iii) sll_ifindex (the interface to be used for reaching the matching pair
// <(i) , (ii)>) ;  (iv) sll_hatype ;  and
// (v) a Unix-domain connection-socket descriptor for a connected client

#define STATE_CONNECTION 0
#define STATE_INCOMPLETE 1
#define STATE_COMPLETE 2

typedef struct Cache {
    struct sockaddr ipaddress;
    int domain_socket;
    // int sll_ifindex;
    // unsigned char if_haddr[IFHWADDRLEN];
    // unsigned short sll_hatype;
    int state;
    struct hwaddr hw;
    struct Cache *next;
    struct Cache *prev;
} Cache;

bool addToCache(Cache **list, Cache *entry);
bool updateCache(Cache *list, Cache *entry);
bool removeFromCache(Cache **list, Cache *entry);
bool isSameCache(Cache *c1, Cache *c2);
Cache *getFromCache(Cache *list, Cache *entry);
Cache *getCacheBySocket(Cache *list, int sock);
Cache *getCacheByHWAddr(Cache *list, unsigned char *if_haddr);
Cache *getCacheByIpAddr(Cache *list, struct sockaddr *ipaddress);

#endif
