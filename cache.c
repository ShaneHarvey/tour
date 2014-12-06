#include "cache.h"

bool addToCache(Cache **list, Cache *entry) {
    bool success = false;
    if(entry != NULL && list != NULL) {
        if(*list != NULL) {
            entry->next = *list;
            entry->prev = NULL;
            (*list)->prev = entry;
        } else {
            entry->next = NULL;
            entry->prev = NULL;
        }
        // Add the entry to the head of the list
        *list = entry;
        // got this far so mark the operation as a success
        success = true;
    }
    return success;
}

bool isSameCache(Cache *c1, Cache *c2) {
    bool equal = false;
    if(c1 != NULL && c2 != NULL) {
        if(c1 == c2) {
            equal = true;
        } else {
            // Now we actually have to compare
            // check for same hwaddress and same ifi_index
            equal = c1->hw.sll_ifindex == c2->hw.sll_ifindex &&
                    !memcmp(c1->hw.sll_addr, c2->hw.sll_addr, c2->hw.sll_halen) &&
                    !memcmp(&(c1->ipaddress), &(c2->ipaddress), sizeof(struct sockaddr));
        }
    }
    return equal;
}

bool updateCache(Cache *list, Cache *entry) {
    bool success = false;
    if(list != NULL && entry != NULL) {
        Cache *node = list;
        while(node != NULL) {
            if(isSameCache(node, entry)) {
                node->ipaddress = entry->ipaddress;
                // node->domain_socket = entry->domain_socket;
                node->hw.sll_ifindex = entry->hw.sll_ifindex;
                node->hw.sll_hatype = entry->hw.sll_hatype;
                memcpy(node->hw.sll_addr, entry->hw.sll_addr, entry->hw.sll_halen);
                success = true;
                break;
            } else {
                node = node->next;
            }
        }
    }
    return success;
}

bool removeFromCache(Cache **list, Cache *entry) {
    bool success = false;
    if(list != NULL && entry != NULL) {
        Cache *node = *list;
        while(node != NULL) {
            if(isSameCache(node, entry)) {
                if(node->prev == NULL) {
                    *list = node->next;
                } else {
                    node->prev->next = node->next;
                    if(node->next != NULL) {
                        node->next->prev = node->prev;
                    }
                }
                // Should I free this?
                free(node);
                success = true;
                break;
            } else {
                node = node->next;
            }
        }
    }
    return success;
}

Cache *getFromCache(Cache *list, Cache *entry) {
    Cache *found = NULL;
    if(list != NULL && entry != NULL) {
        Cache *node = list;
        while(node != NULL) {
            if(isSameCache(node, entry)) {
                   found = node;
                   break;
            } else {
                node = node->next;
            }
        }
    }
    return found;
}

Cache *getCacheBySocket(Cache *list, int sock) {
    Cache *found = NULL;
    if(list != NULL) {
        Cache *node = list;
        while(node != NULL) {
            if(node->domain_socket == sock) {
                found = node;
                break;
            } else {
                node = node->next;
            }
        }
    }
    return found;
}

Cache *getCacheByHWAddr(Cache *list, unsigned char *if_haddr) {
    Cache *found = NULL;
    if(list != NULL && if_haddr != NULL) {
        Cache *node = list;
        while(node != NULL) {
            if(!memcmp(node->hw.sll_addr, if_haddr, node->hw.sll_halen)) {
                found = node;
                break;
            } else {
                node = node->next;
            }
        }
    }
    return found;
}

Cache *getCacheByIpAddr(Cache *list, struct sockaddr *ipaddress) {
    Cache *found = NULL;
    if(list != NULL && ipaddress != NULL) {
        Cache *node = list;
        while(node != NULL) {
            if(!memcmp(&(node->ipaddress), ipaddress, sizeof(struct sockaddr))) {
                found = node;
                break;
            } else {
                node = node->next;
            }
        }
    }
    return found;
}
