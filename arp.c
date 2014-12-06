#include "arp.h"

static Cache *cache = NULL;
static struct hwa_info *devices = NULL;

int main(int argc, char **argv) {
    int unix_domain = -1;
    int pf_socket = -1;
    int success = EXIT_SUCCESS;

    devices = get_hw_addrs();
    if(devices == NULL) {
        warn("No HW devices found on the system\n");
        success = EXIT_FAILURE;
        goto NO_HW_DEVICES;
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
    success = run_arp(unix_domain, pf_socket);

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
NO_HW_DEVICES:
    return success;
}

int run_arp(int unix_domain, int pf_socket) {
    int success = EXIT_SUCCESS;
    int running = 1;
    // select vars
    fd_set rset;
    int max_fd;
    Cache *current = NULL;

    while(running) {
        /* Find the max fd */
        max_fd = maxfd(pf_socket, unix_domain, cache);
        /* set all the server fd's */
        FD_ZERO(&rset);
        FD_SET(unix_domain, &rset);
        FD_SET(pf_socket, &rset);
        /* Set the client domain sockets */
        for(current = cache; current != NULL; current = current->next) {
            if(current->state != STATE_COMPLETE) {
                FD_SET(current->domain_socket, &rset);
            }
        }
        /* Now wait to receive the transmissions */
        if(select(max_fd + 1, &rset, NULL, NULL, NULL) < 0) {
            error("Failed to select on socket fd's: %s\n", strerror(errno));
            running = false;
            success = EXIT_FAILURE;
            break;
        }
        /* See if any of the unix domain sockets in the cache are readable */
        current = cache;
        while(current != NULL) {
            /* skip complete entries because they have invalid fd's */
            if(current->state == STATE_COMPLETE) {
                current = current->next;
                continue;
            }
            if(FD_ISSET(current->domain_socket, &rset)) {
                int rv = 0;
                debug("Client UNIX connection is readable\n");
                if(current->state == STATE_CONNECTION) {
                    rv = handle_areq(pf_socket, current, devices);
                }

                if(rv == 0) {
                    /* Invalid: Did not read sizeof(struct areq) bytes */
                    Cache *rm = current;
                    current = current->next;
                    /* If we read zero bytes then the socket closed */
                    close(rm->domain_socket);
                    if(!removeFromCache(&cache, rm)) {
                        error("Failed to remove the cache entry from the cache.");
                    }
                } else {
                    // Get the next node
                    current = current->next;
                }
            } else {
                // Get the next node
                current = current->next;
            }
        }
        /* Handle unix domain socket communications */
        if(FD_ISSET(unix_domain, &rset)) {
            /* Communicate with areq function */
            struct sockaddr_un remote;
            socklen_t addrlen;
            debug("Connection on UNIX socket\n");
            memset(&remote, 0, sizeof(remote));
            addrlen = sizeof(remote);
            /* Accept the incomming connection */
            int sfd = accept(unix_domain, (struct sockaddr*)&remote, &addrlen);
            /* Double check and make sure it doesn't exist */
            Cache *ce = getCacheBySocket(cache, sfd);
            if(ce == NULL) {
                /* Build a partial cache entry */
                ce = calloc(1, sizeof(Cache));
            }
            /* Update the fd */
            ce->domain_socket = sfd;
            ce->state = STATE_CONNECTION;
            /* Add incomplete entry to the cache */
            if(!addToCache(&cache, ce)) {
                error("Failed to add partial entry to the cache.");
                /* Free the entry since we can't add it */
                free(ce);
            }
        }
        /* handle PF_PACKET socket comminications */
        if(FD_ISSET(pf_socket, &rset)) {
            /* ARP Request messages caught here */
            struct ethhdr eh;
            struct arp_hdr arp;
            struct sockaddr_ll llsrc;
            int nread = 0;
            // int srcindex = 0;
            debug("PF_SOCKET is readable\n");
            /* zero out structs */
            memset(&eh, 0, sizeof(struct ethhdr));
            memset(&llsrc, 0, sizeof(struct sockaddr_ll));
            memset(&arp, 0, sizeof(arp));
            if((nread = recv_frame(pf_socket, &eh, &arp, &llsrc)) < 0) {
                /* Didn't have a successful read */
                error("read on pack sock failed: %s\n", strerror(errno));
                success = EXIT_FAILURE;
                break;
            } else if((nread < sizeof(struct ethhdr) + ARP_HDRLEN) || nread < (sizeof(struct ethhdr) + ARP_HDRLEN + ARP_DATALEN(&arp))) {
                /* Message too short */
                int len = sizeof(struct ethhdr) + ARP_HDRLEN;
                int len2 = sizeof(struct ethhdr) + ARP_HDRLEN + ARP_DATALEN(&arp);
                debug("nread < sizeof(struct ethhdr) + ARP_HDRLEN = %d < %d\n", nread, len);
                debug("nread != sizeof(struct ethhdr) + ARP_HDRLEN + ARP_DATALEN(&arp) = %d != %d\n", nread, len2);
                debug("ARP frame len wrong: read bytes:%d, arplen:%d\n", nread, ARP_HDRLEN + ARP_DATALEN(&arp));
            } else if(valid_arp(&arp)) {
                switch(arp.op) {
                    case ARPOP_REQUEST:
                        handle_req(pf_socket, &eh, &arp, &llsrc);
                        break;
                    case ARPOP_REPLY:
                        handle_reply(pf_socket, &eh, &arp, &llsrc);
                        break;
                    default:
                        warn("Unsupported ARP op %hu\n", arp.op);
                        break;
                }
                /*
                Extract information about this msg
                if the msg was destined for this node:
                the <ip,hw> address should be stored in the cache or updated
                else:
                If the entry already exists update it, but do not add a new
                entry to the cache.
                */
                // TODO: This might be messed up
                /* Create a template to search with */
//                Cache *cache_template = malloc(sizeof(Cache));
//                memset(cache_template, 0, sizeof(Cache));
//                /* Set fields */
//                cache_template->hw.sll_ifindex = llsrc.sll_ifindex;
//                cache_template->ipaddress = arp.addr;
//                cache_template->hw.sll_hatype = llsrc.sll_hatype;
//                memcpy(cache_template->hw.sll_addr, llsrc.sll_addr, IFHWADDRLEN);
//                /* Try to get this cache entry */
//                Cache *ce = getFromCache(cache, cache_template);
//                /* Determine where this messages destination is */
//                if(ce == NULL && isDestination(devices, &arp.addr)) {
//                    // Add the new entry to the cache
//                    if(!addToCache(&cache, cache_template)) {
//                        error("Failed to add new cache entry.\n");
//                    }
//                } else if(ce != NULL) {
//                    // Update the cache entry
//                    if(!updateCache(cache, cache_template)) {
//                        error("Failed to update an existing cache entry.\n");
//                        free(cache_template);
//                    }
//                }
            } else {
                /* invalid arp, do nothing */;
            }
        }
    }
    return success;
}

/**
 * Handle a an AREQ when a STATE_CONNECTION cache entry is readable
 *
 * @pf_socket  The packet socket to broadcast ARP requests
 * @conn_entry The connected, readable cache entry.
 * @devices    The list of interfaces
 *
 * @return   1 if ARP request were sent, 0 if the Cache entry should be removed
 */
int handle_areq(int pf_socket, Cache *conn_entry, struct hwa_info *devices) {
    int bytes_read;
    char recvbuf[sizeof(struct areq) + 1];
    struct areq *ar = (struct areq*)recvbuf;
    u_char prot_len;
    uint16_t prot_type;
    struct hwa_info *device;
    Cache *completed;

    if(conn_entry->state != STATE_CONNECTION) {
        error("Cache entry is not in connection state: %d\n", conn_entry->state);
        exit(EXIT_FAILURE);
    }
    /* Receieve the packet */
    if((bytes_read = recv(conn_entry->domain_socket, recvbuf, sizeof(recvbuf), 0)) < 0) {
        error("recv failed on connected socket: %s\n", strerror(errno));
        return 0;
    } else if (bytes_read != sizeof(struct areq)) {
        error("areq message invalid %d bytes should be %zu\n", bytes_read, sizeof(struct areq));
        return 0;
    }
    /* Valid message */

    /* Only works for IPv4 sockaddr_in's  */
    switch(ar->addr.sa_family) {
        case AF_INET:
            prot_len = 4;
            prot_type = PF_INET;
            break;
        default:
            /* Protocol not implemented. TODO: Return an error to Client */
            return 0;
    }

    /* Do we already have cache entry? */
    completed = getCacheByIpAddr(cache, &ar->addr);
    if(completed != NULL && completed->state == STATE_COMPLETE) {
        /* Response with the cached hwaddr struct */
        int nsent;
        nsent = send(conn_entry->domain_socket, &completed->hw, sizeof(struct hwaddr), 0);
        if(nsent < 0) {
            warn("ARP response to API failed: %s\n", strerror(errno));
            return 0;
        }
        /* Return 0 to tell calling function to close socket and remove cache */
        return 0;
    }

    /* Create incomplete cache entry */
    conn_entry->state = STATE_INCOMPLETE;
    memcpy(&conn_entry->ipaddress, &ar->addr, sizeof(struct sockaddr));

    /* Send out ARP packet on every interface */
    for(device = devices; device != NULL; device = device->hwa_next) {
        u_char arp_packet[ARP_MAXLEN]; /* ARP Packet to broadcast */
        struct sockaddr_in *srcip = (struct sockaddr_in *)device->ip_addr;
        struct sockaddr_in *tgtip = (struct sockaddr_in *)&ar->addr;
        int arpsize;
        u_char broadhwa[8];
        memset(broadhwa, 0xFF, sizeof(broadhwa));
        memset(arp_packet, 0, sizeof(arp_packet)); /* Zero memory */
        /* Our devices are all ethernet, but could be filled with the
         * outgoing interface's hardware type.
         */
        arpsize = build_arp(arp_packet, sizeof(arp_packet), ARPHRD_ETHER,
                prot_type, sizeof(device->if_haddr), prot_len, ARPOP_REQUEST,
                device->if_haddr, (u_char *)&srcip->sin_addr.s_addr, NULL,
                (u_char *)&tgtip->sin_addr.s_addr);

        if(!send_frame(pf_socket, arp_packet, arpsize, broadhwa,
                device->if_haddr, device->if_index)) {
            error("failed to broadcast frame: %s\n", strerror(errno));
            return 0;
        }
    }
    return 1;
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
        } else if(listen(unix_domain, 5) < 0) {
            error("Failed to listen on arp unix domain socket.\n");
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
    if((pf_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP_SH))) < 0) {
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
ssize_t recv_frame(int pf_socket, struct ethhdr *eh, struct arp_hdr *arp, struct sockaddr_ll *src) {
    char frame[ETH_FRAME_LEN]; /* MAX ethernet frame length 1514 */
    socklen_t srclen;
    ssize_t nread;

    memset(frame, 0, sizeof(frame));
    memset(src, 0, sizeof(struct sockaddr_ll));
    srclen = sizeof(struct sockaddr_ll);
    if((nread = recvfrom(pf_socket, frame, ETH_FRAME_LEN, 0,(struct sockaddr *)src, &srclen)) < 0) {
        error("packet socket recv failed: %s\n", strerror(errno));
    } else if(nread > sizeof(struct ethhdr) + ARP_HDRLEN) {
        /* Copy ethernet frame header into eh */
        memcpy(eh, frame, ETH_HLEN);
        /* Copy the frame_data into the odr_msg */
        memcpy(arp, frame + ETH_HLEN, nread - ETH_HLEN);
        /* Convert message from Network to Host order */
        ntoh_arp(arp);

        info("Received ARP message\n");
    }
    return nread;
}

/*
* Converts areq from Network to Host byte order
*/
void ntoh_arp(struct arp_hdr *arp) {
    /* ARP header */
    arp->id = ntohs(arp->id);
    arp->hard_type = ntohs(arp->hard_type);
    arp->prot_type = ntohs(arp->prot_type);
    arp->op = ntohs(arp->op);
}

struct hwa_info* isDestination(struct hwa_info *devices, struct in_addr *ip_addr) {
    struct hwa_info *dest = NULL;
    if(devices != NULL && ip_addr != NULL) {
        struct hwa_info *current_device = devices;
        while(current_device != NULL) {
            if(ip_addr->s_addr == ((struct sockaddr_in*)current_device->ip_addr)->sin_addr.s_addr) {
                dest = current_device;
                break;
            } else {
                current_device = current_device->hwa_next;
            }
        }
    } else {
        if(devices == NULL) {
            warn("Searching empty hwa_info list.\n");
        }
        if(ip_addr == NULL) {
            warn("Searching for null HW address in the hwa_info list.\n");
        }
    }
    return dest;
}

int maxfd(int pf_socket, int unix_domain, Cache *cache) {
    int maxfd = pf_socket > unix_domain ? pf_socket : unix_domain;
    if(cache != NULL) {
        Cache *current = cache;
        while(current != NULL) {
            if(current->state != STATE_COMPLETE) {
                maxfd = maxfd > current->domain_socket ? maxfd : current->domain_socket;
            }
            current = current->next;
        }
    }
    return maxfd;
}

/*
* Construct and send an ethernet frame to the dst_hwaddr MAC from src_hwaddr
* MAC going out of interface index ifi_index.
*
* @sock       The packet socket to send the frame
* @payload    The data payload of the ethernet frame
* @size       The size of the payload
* @dstmac     The next hop MAC address
* @srcmac     The outgoing MAC address
* @ifi_index  The outgoing interface index in HOST byte order
* @return 1 if succeeded 0 if failed
*/
int send_frame(int sock, void *payload, int size, unsigned char *dstmac,
                unsigned char *srcmac, int ifi_index) {
    char frame[ETH_FRAME_LEN]; /* MAX ethernet frame length 1514 */
    struct ethhdr *eh = (struct ethhdr *)frame;
    struct sockaddr_ll dest;
    int nsent;
    memset(frame, 0, ETH_FRAME_LEN);

    if(size > ETH_DATA_LEN) {
        error("Frame data too large: %d\n", size);
        return 0;
    }
    /* Initialize ethernet frame */
    memcpy(eh->h_dest, dstmac, ETH_ALEN);
    memcpy(eh->h_source, srcmac, ETH_ALEN);
    eh->h_proto = htons(ETH_P_ARP_SH);

    /* Copy frame data into buffer */
    memcpy(frame + sizeof(struct ethhdr), payload, size);

    /* Initialize sockaddr_ll */
    memset(&dest, 0, sizeof(struct sockaddr_ll));
    dest.sll_family = AF_PACKET;
    dest.sll_ifindex = ifi_index;
    memcpy(dest.sll_addr, dstmac, ETH_ALEN);
    dest.sll_halen = ETH_ALEN;
    dest.sll_protocol = htons(ETH_P_ARP_SH);

    /* print_frame(eh, payload); */

    if((nsent = sendto(sock, frame, size+sizeof(struct ethhdr), 0,
        (struct sockaddr *)&dest, sizeof(struct sockaddr_ll))) < 0) {
            error("packet sendto: %s\n", strerror(errno));
            return 0;
        } else {
            debug("Send %d bytes, payload size:%d\n", nsent, size);
        }
        return 1;
}

/**
 * Build an ARP packet in network byte order
 *
 * @start The starting address for the ARP header
 * @size  The size of the buffer to hold the ARP packet
 *
 * returns the size of the built arp packet
 */
int build_arp(u_char *start, int size, uint16_t hard_type, uint16_t prot_type,
        u_char hard_len, u_char prot_len, uint16_t op, u_char *send_hwa,
        u_char *send_pro, u_char *target_hwa, u_char *target_pro) {
    struct arp_hdr *hdr = (struct arp_hdr *)start;
    u_char *sha = start + ARP_HDRLEN;             /* send hw address */
    u_char *spa = sha + hard_len;                 /* send protocol address */
    u_char *tha = spa + prot_len;                 /* target hw address */
    u_char *tpa = tha + hard_len;                 /* target protocol address */
    int len = ARP_HDRLEN + 2 * hard_len + 2 * prot_len;

    if(start == NULL || size < len) {
        warn("Buffer too small to build arp packet: %d\n", size);
        return 0;
    }
    /* ARP header */
    hdr->id = htons(ARP_ID);
    hdr->hard_type = htons(hard_type);
    hdr->prot_type = htons(prot_type);
    hdr->hard_len = hard_len;
    hdr->prot_len = prot_len;
    hdr->op = htons(op);

    /* ARP variable length data... [ sha | spa | tha | tpa ] */
    if(send_hwa == NULL) {
        memset(sha, 0, hard_len);
    } else {
        memcpy(sha, send_hwa, hard_len);
    }
    if(send_pro == NULL) {
        memset(spa, 0, prot_len);
    } else {
        memcpy(spa, send_pro, prot_len);
    }
    if(target_hwa == NULL) {
        memset(tha, 0, hard_len);
    } else {
        memcpy(tha, target_hwa, hard_len);
    }
    if(target_pro == NULL) {
        memset(tpa, 0, prot_len);
    } else {
        memcpy(tpa, target_pro, prot_len);
    }
    return len;
}

/**
* Validate host order arp packet
*/
int valid_arp(struct arp_hdr *arp) {
    int valid = 1;
    if(arp->id != ARP_ID) {
        debug("INVALID ARP: id %hu should be %hu\n", arp->id, ARP_ID);
        valid = 0;
    }

    if(arp->hard_type != ARPHRD_ETHER) {
        debug("INVALID ARP: hard_type %hhu not supported\n", arp->hard_type);
        valid = 0;
    }
    if(arp->prot_type != PF_INET) {
        debug("INVALID ARP: prot_type %hhu not supported\n", arp->prot_type);
        valid = 0;
    }
    if(arp->hard_len > ARP_MAXHWLEN) {
        debug("INVALID ARP: hard_len %hhu to big\n", arp->hard_len);
        valid = 0;
    }
    if(arp->prot_len > ARP_MAXPRLEN)  {
        debug("INVALID ARP: prot_len %hhu to big\n", arp->prot_len);
        valid = 0;
    }
    if(arp->op != ARPOP_REQUEST && arp->op != ARPOP_REPLY) {
        debug("INVALID ARP: op %hu is not REQ or REP\n", arp->op);
        valid = 0;
    }

    return valid;
}

int handle_req(int pack_fd, struct ethhdr *eh, struct arp_hdr *arp, struct sockaddr_ll *src) {
    struct in_addr tgtip;
    u_char *tpa = ARP_TPA(arp);
    struct hwa_info *this = NULL;

    memcpy(&tgtip.s_addr, tpa, arp->prot_len);
    debug("ARP REQ: asking for target ip: %s\n", inet_ntoa(tgtip));
    /* Loop interfaces to check if we ARE the target, if so send REPLY */
    if((this = isDestination(devices, &tgtip)) != NULL) {
        int hdr_size = 0;
        struct arp_hdr hdr;


        memset(&hdr, 0, sizeof(struct arp_hdr));
        hdr_size = build_arp((u_char*)&hdr, sizeof(struct arp_hdr), arp->hard_type, arp->prot_type,
                arp->hard_len, arp->prot_len, ARPOP_REPLY, eh->h_source,
                (u_char*)&((struct sockaddr_in*)this->ip_addr)->sin_addr, this->if_haddr, tpa);

        send_frame(pack_fd, &hdr, hdr_size, eh->h_source,
                mac_by_ifindex(src->sll_ifindex), src->sll_ifindex);
    }
    return 0;
}

int handle_reply(int pack_fd, struct ethhdr *eh, struct arp_hdr *arp, struct sockaddr_ll *src) {
    /* Loop Cache entries to check if we have any INCOMPLETE entries asking
     * for this IP, if so send struct hwaddr back to connected clients.
     */
    Cache *entry = NULL;
    for(entry = cache; entry != NULL; entry = entry->next) {
        if (entry->state == STATE_INCOMPLETE) {
            /* send struct hwaddr */
        }
    }
    return 0;
}

u_char *mac_by_ifindex(int index) {
    u_char *outmac = NULL;
    struct hwa_info *tmp;

    for(tmp = devices; tmp != NULL; tmp = tmp->hwa_next) {
        if(tmp->if_index == index) {
            outmac = tmp->if_haddr;
            break;
        }
    }
    return outmac;
}