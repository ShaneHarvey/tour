#ifndef CACHE_H
#define CACHE_H
#include <net/if.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// (i) IP address ;  (ii) HW address ;
// (iii) sll_ifindex (the interface to be used for reaching the matching pair
// <(i) , (ii)>) ;  (iv) sll_hatype ;  and
// (v) a Unix-domain connection-socket descriptor for a connected client

typedef struct Cache {
    struct sockaddr *ipaddress;
    int domain_socket;
    int sll_ifindex;
    unsigned char if_haddr[IFHWADDRLEN];
    unsigned short sll_hatype;
    struct Cache *next;
    struct Cache *prev;
} Cache;

bool addToCache(Cache **list, Cache *entry);
bool updateCache(Cache *list, Cache *entry);
bool removeFromCache(Cache **list, Cache *entry);
bool isSameCache(Cache *c1, Cache *c2);
Cache *getFromCache(Cache *list, Cache *entry);

#endif
