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
            equal = c1->sll_ifindex == c2->sll_ifindex &&
                    !memcmp(c1->if_haddr, c2->if_haddr, IFHWADDRLEN);
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
                node->domain_socket = entry->domain_socket;
                node->sll_ifindex = entry->sll_ifindex;
                node->sll_hatype = entry->sll_hatype;
                memcpy(node->if_haddr, entry->if_haddr, IFHWADDRLEN);
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
                    node->next->prev = node->prev;
                }
                // Should I free this?
                free(entry);
                success = true;
                break;
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
            if(node->sll_ifindex == entry->sll_ifindex &&
               memcmp(node->if_haddr, entry->if_haddr, IFHWADDRLEN)) {
                   found = node;
                   break;
            } else {
                node = node->next;
            }
        }
    }
    return found;
}
