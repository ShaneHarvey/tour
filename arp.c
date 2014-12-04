#include "arp.h"

static Cache *cache = NULL;

int main(int argc, char **argv) {
    int unix_domain = -1;
    int pf_socket = -1;
    int success = EXIT_SUCCESS;

    struct hwa_info *devices = get_hw_addrs();
    if(devices == NULL) {
        warn("No HW devices found on the system\n");
        success = EXIT_FAILURE;
        // goto NO_HW_DEVICES;
    }

    /* Create the unix domain socket */
    if((unix_domain = create_unix_domain()) < 0) {
        success = EXIT_FAILURE;
        goto UNIX_DOMAIN_FAIL;
    }

    /* Create the PF_PACKET */
    if((pf_socket = create_pf_socket()) < 0) {
        success = EXIT_FAILURE;
        goto PF_PACKET_FAIL;
    }

    /* Set ctrl-c signal handler */
    set_sig_cleanup();

    /* Start the arp infinite loop */
    success = run_arp(unix_domain, pf_socket, devices);

    /* Close open socket fd */
    if(pf_socket > -1) {
        close(pf_socket);
    }
PF_PACKET_FAIL:
    /* Close and remove the unix domain file */
    if(unix_domain > -1) {
        close(unix_domain);
    }
UNIX_DOMAIN_FAIL:
    /* Remove the "well known" file */
    unlink(ARP_WELL_KNOWN_PATH);
// NO_HW_DEVICES:
    return success;
}

int run_arp(int unix_domain, int pf_socket,  struct hwa_info *devices) {
    int success = EXIT_SUCCESS;
    int running = 1;
    // select vars
    fd_set rset;
    int max_fd = unix_domain > pf_socket ? unix_domain + 1 : pf_socket + 1;
    while(running) {
        /* Now wait to receive the transmissions */
        FD_ZERO(&rset);
        FD_SET(unix_domain, &rset);
        FD_SET(pf_socket, &rset);
        select(max_fd, &rset, NULL, NULL, NULL);
        /* Handle unix domain socket communications */
        if(FD_ISSET(unix_domain, &rset)) {
            // Communicate with areq function
            // If you read zero the file was closed (cache entry)
        }
        /* handle PF_PACKET socket comminications */
        if(FD_ISSET(pf_socket, &rset)) {
            /* ARP Request messages caught here */
            struct ethhdr eh;
            struct arpreq recvmsg;
            struct sockaddr_ll llsrc;
            int read = 0;
            // int srcindex = 0;
            /* zero out structs */
            memset(&eh, 0, sizeof(struct ethhdr));
            memset(&llsrc, 0, sizeof(struct sockaddr_ll));
            memset(&recvmsg, 0, sizeof(struct arpreq));
            if((read = recv_frame(pf_socket, &eh, &recvmsg, &llsrc)) < 0) {
                /* Didn't have a successful read */
                success = EXIT_FAILURE;
                break;
            } else {
                /*
                Extract information about this msg
                if the msg was destined for this node:
                the <ip,hw> address should be stored in the cache or updated
                else:
                If the entry already exists update it, but do not add a new
                entry to the cache.
                */
                /* Create a template to search with */
                Cache *cache_template = malloc(sizeof(Cache));
                memset(cache_template, 0, sizeof(Cache));
                /* Set fields */
                cache_template->sll_ifindex = llsrc.sll_ifindex;
                cache_template->ipaddress = recvmsg.addr;
                cache_template->sll_hatype = llsrc.sll_hatype;
                memcpy(cache_template->if_haddr, llsrc.sll_addr, IFHWADDRLEN);
                // TODO: Set Unix domain socket
                /* Try to get this cache entry */
                Cache *ce = getFromCache(cache, cache_template);
                /* Determine where this messages destination is */
                if(ce == NULL && isDestination(devices, cache_template)) {
                    // Add the new entry to the cache
                    if(!addToCache(&cache, cache_template)) {
                        error("Failed to add new cache entry.\n");
                    }
                } else if(ce != NULL){
                    // Update the cache entry
                    if(!updateCache(cache, cache_template)) {
                        error("Failed to update an existing cache entry.\n");
                    }
                    free(cache_template);
                }
            }
        }
    }
    return success;
}

int create_unix_domain(void) {
    int unix_domain = -1;
    struct sockaddr_un addr;
    /* setup the unix domain socket */
    if((unix_domain = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        error("Failed to create unix domain socket.\n");
    } else {
        /* Bind to the "well known" file on disk */
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, ARP_WELL_KNOWN_PATH, sizeof(addr.sun_path) - 1);
        /* Attempt to delete the file incase something abnormal happened */
        unlink(ARP_WELL_KNOWN_PATH);
        if(bind(unix_domain, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
            error("Failed to bind to file %s\n", ARP_WELL_KNOWN_PATH);
            /* set unix domain to -1 since it failed */
            unix_domain = -1;
        } else if(chmod(ARP_WELL_KNOWN_PATH, S_IRUSR | S_IWUSR | S_IXUSR | S_IWGRP | S_IWOTH) < 0) {
            /* chmod the file to world writable (722) so any process can use arp  */
            error("chmod failed: %s\n", strerror(errno));
            /* Close the unix domain socket */
            close(unix_domain);
            /* remove the file */
            unlink(ARP_WELL_KNOWN_PATH);
            /* Set the domain socket to -1 */
            unix_domain = -1;
        }
    }
    return unix_domain;
}

int create_pf_socket(void) {
    int pf_socket = -1;
    if((pf_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ODR))) < 0) {
        error("Failed to create PF_SOCKET\n");
    }
    return pf_socket;
}

void cleanup(int signum) {
    /* remove the UNIX socket file */
    unlink(ARP_WELL_KNOWN_PATH);
    /* 128+n Fatal error signal "n" is the standard Linux exit code */
    _exit(128 + signum);
}

void set_sig_cleanup(void) {
    struct sigaction sigac_int;
    /* Zero out memory */
    memset(&sigac_int, 0, sizeof(sigac_int));
    /* Set values */
    sigac_int.sa_handler = &cleanup;
    /* Set the sigaction */
    if(sigaction(SIGINT, &sigac_int, NULL) < 0) {
        error("sigaction failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

/*
* Receives a ethernet frame and stores the arp message into recvmsg. Converts
* the message into host order as well.
* @Return is the same as recvfrom(2)
*/
ssize_t recv_frame(int pf_socket, struct ethhdr *eh, struct arpreq *recvmsg, struct sockaddr_ll *src) {
    char frame[ETH_FRAME_LEN]; /* MAX ethernet frame length 1514 */
    socklen_t srclen;
    ssize_t nread;

    memset(src, 0, sizeof(struct sockaddr_ll));
    srclen = sizeof(struct sockaddr_ll);
    if((nread = recvfrom(pf_socket, frame, ETH_FRAME_LEN, 0,
        (struct sockaddr *)src, &srclen)) < 0) {
            error("packet socket recv failed: %s\n", strerror(errno));
    } else {
        /* Copy ethernet frame header into eh */
        memcpy(eh, frame, ETH_HLEN);
        /* Copy the frame_data into the odr_msg */
        memcpy(recvmsg, frame + ETH_HLEN, nread - ETH_HLEN);
        /* Convert message from Network to Host order */
        ntoh_msg(recvmsg);
        info("Received ");
    }
    return nread;
}

/*
* Converts arpreq from Network to Host byte order
*/
void ntoh_msg(struct arpreq *msg) {
    // msg->addr = ntohl(msg->addr);
    msg->addrlen = ntohl(msg->addrlen);
}

bool isDestination(struct hwa_info *devices, Cache *cache) {
    bool isdest = false;
    if(devices != NULL && cache != NULL) {
        struct hwa_info *current_device = devices;
        while(current_device != NULL) {
            if(!memcmp(cache->if_haddr, current_device->if_haddr, IFHWADDRLEN) &&
               !memcmp(&(cache->ipaddress), current_device->ip_addr, sizeof(struct sockaddr))) {
                isdest = true;
                break;
            } else {
                current_device = current_device->hwa_next;
            }
        }
    } else {
        if(devices == NULL) {
            warn("Searching empty hwa_info list.\n");
        }
        if(cache == NULL) {
            warn("Searching for null <IP address , HW address> in the hwa_info list.\n");
        }
    }
    return isdest;
}
