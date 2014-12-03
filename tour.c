#include "tour.h"
#include "get_hw_addrs.h"
#include "ping.h"

static struct hwa_info *hwas;  /* List of Hardware Addresses      */
char host[HOST_NAME_MAX];      /* Hostname running ODR, eg vm2    */

int main(int argc, char **argv) {
    int rt, udp, opt, status = EXIT_FAILURE, binded = 0;
    /* Parse cmdline options */
    if(!valid_args(argc, argv)) {
        fprintf(stderr, "Usage: %s [hostname1 hostname2 ...] \n", argv[0]);
        return EXIT_FAILURE;
    }
    /* Create the IP raw socket used to send and receive Tour messages */
    if((rt = socket(AF_INET, SOCK_RAW, IPPROTO_TOUR)) < 0) {
        error("failed to create raw socket: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    /* Set the header include option */
    opt = 1;
    if(setsockopt(rt, IPPROTO_IP, IP_HDRINCL, &opt, sizeof(opt)) < 0) {
        error("failed to set IP_HDRINCL option: %s\n", strerror(errno));
        goto CLOSE_RT;
    }
    /* Create the UDP socket used to send and receive multicast messages */
    if((udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        error("failed to create UDP socket: %s\n", strerror(errno));
        goto CLOSE_RT;
    }
    /* Get our interface info */
    if((hwas = get_hw_addrs()) == NULL) {
        error("Failed to find system interfaces: %s\n", strerror(errno));
        goto CLOSE_UDP;
    }
    /* Lookup our hostname */
    if(gethostname(host, sizeof(host)) < 0) {
        error("gethostname failed: %s\n", strerror(errno));
        goto CLOSE_UDP;
    } else {
        debug("Tour started on node %s\n", host);
    }

    if(argc > 1) {
        /* Creating a multicast group and sending the TOUR packet */
        if(!start_tour(rt, udp, argc - 1, argv + 1)) {
            error("failed to start tour: %s\n", strerror(errno));
            goto FREE_HWA;
        }
        binded = 1;
    }
    /* run the tour program */
    if(!run_tour(rt, udp, binded)) {
        error("Tour failed: %s\n", strerror(errno));
        goto FREE_HWA;
    }

    status = EXIT_SUCCESS; /* Normal cleanup */
FREE_HWA:
    free_hwa_info(hwas);
CLOSE_UDP:
    close(udp);
CLOSE_RT:
    close(rt);
    return status;
}

int valid_args(int argc, char **argv) {
    int i;

    if(argc - 1 > TOUR_MAXIPS) {
        error("Too many hostnames to fit in an IP packet: num=%d, max=%lu\n", argc - 1, TOUR_MAXIPS);
        return 0;
    }
    for(i = 2; i < argc; ++i) {
        if(strcmp(argv[i-1], argv[i]) == 0) {
            error("Consecutive hostnames (args %d and %d) cannot be the same\n", i-1, i);
            return 0;
        }
    }
    return 1;
}

int start_tour(int rt, int udp, int numhosts, char **hosts) {
    struct sockaddr_in mcastaddr;
    char tourmsg[TOUR_MAXPACKET];
    struct tourhdr *tour = (struct tourhdr *) tourmsg;
    int port, i;

    if((port = bind_port(udp, 0)) < 0)  {
        return 0;
    }

    memset(&mcastaddr, 0, sizeof(mcastaddr));
    mcastaddr.sin_family = AF_INET;
    inet_aton("224.3.3.3", &mcastaddr.sin_addr);
    /* Join the multicast group */
    if (!mcast_join(udp, &mcastaddr, 0)) {
        return 0;
    }

    /* Create the initial tour message */
    memset(tourmsg, 0, sizeof(tourmsg));
    tour->mcastip.s_addr = mcastaddr.sin_addr.s_addr;
    tour->mcastport = htons(port);
    tour->len = numhosts;

    /* IPs are in reverse order */
    for(i = numhosts - 1; i >= 0; --i) {
        if(!getipbyhost(hosts[i], tour->ips + i)) {
            return 0;
        }
    }
    return forward_tour(rt, tour);
}

int run_tour(int rt, int udp, int binded) {
    fd_set rset;
    int max_fd, err;
    pthread_t recvthread;

    /* start the ping recv thread */
    if((err = pthread_create(&recvthread, NULL, run_ping_recv, NULL)) != 0) {
        error("pthread_create failed: %s\n", strerror(err));
        return 0;
    }

    max_fd = rt > udp ? rt + 1: udp + 1;
    while(1) {
        FD_ZERO(&rset);
        FD_SET(rt, &rset);
        FD_SET(udp, &rset);
        if(select(max_fd, &rset, NULL, NULL, NULL) < 0) {
            error("select failed: %s\n", strerror(errno));
            return 0;
        }

        if(FD_ISSET(udp, &rset)) {
            /* Halt all pinging activity */
            break;
        }

        if(FD_ISSET(rt, &rset)) {
            /* Forward Tour message or end the tour if this is last node */
            char packet[IP_MAXPACKET];
            struct ip *iph = (struct ip *)packet;
            struct tourhdr *tour = (struct tourhdr *)iph + sizeof(struct ip);
            struct sockaddr_in src;
            socklen_t srclen;
            int nread;

            memset(packet, 0, sizeof(packet));
            memset(&src, 0, sizeof(src));

            nread = recvfrom(rt, packet, sizeof(packet), 0, (struct sockaddr *)&src, &srclen);
            if(nread < 0) {
                error("raw socket recvfrom failed: %s\n", strerror(errno));
                return 0;
            } else if(nread < sizeof(struct ip) + sizeof(struct tourhdr)) {
                debug("Ignoring %d byte tour message: too short\n", nread);
            } else if(iph->ip_id != htons(IPID_TOUR)) {
                debug("Ignoring %d byte tour message: wrong IP id field\n", nread);
            } else {
                tour->len = ntohs(tour->len);
                if(tour->len > 0) {
                    if(!forward_tour(rt, tour)) {
                        return 0;
                    }
                } else {
                    info("Tour reached final host\n");
                    /* Spend 5 seconds pinging */
                    sleep(5);
                    /* multicast end of tour message */
                }
            }
        }
    }

    /* Halt all pinging activity */
    if((err = pthread_cancel(recvthread)) != 0){
        error("pthread_cancel: %s\n", strerror(err));
        return 0;
    }

    if(!end_tour(udp)) {
        error("Failed to end tour %s\n", strerror(errno));
        return 0;
    }
    return 1;
}

int end_tour(int udp) {

    while(1) {
        struct timeval tv = {5L, 0L};
        int rv;
        fd_set rset;
        /* Print out multicast messages until we reach 5 second timeout */
        FD_ZERO(&rset);
        FD_SET(udp, &rset);
        rv = select(udp + 1, &rset, NULL, NULL, &tv);
        if(rv < 0) {
            error("select failed: %s\n", strerror(errno));
            return 0;
        } else if(rv == 0) {
            /* Reached 5 second timeout */
            info("Shutting down Tour application\n");
            break;
        } else {
            char data[IP_MSS];
            int nread;
            struct sockaddr_in source;
            socklen_t slen;
            memset(data, 0, sizeof(data));
            /* UDP socket is readable, print multicast message */
            nread = recvfrom(udp, data, sizeof(data), 0, (struct sockaddr *)
                    &source, &slen);
            if(nread < 0) {
                error("UDP recvfrom failed: %s\n", strerror(errno));
                return 0;
            }
            info("Node %s. Received: %s\n", host, data);
        }
    }
    return 1;
}

int forward_tour(int rt, struct tourhdr *tour) {
    struct in_addr nextip;
    size_t size;

    nextip.s_addr = tour->ips[tour->len - 1].s_addr;
    /* Remove the next IP */
    tour->len--;
    size = TOUR_SIZE(tour);

    debug("Forwarding %zu byte tour packet with %d IPs\n", size, tour->len);

    tour->len = htons(tour->len);
    return send_ip(rt, tour, size, nextip);
}

int send_ip(int rt, void *data, size_t len, struct in_addr dstip) {
    char packet[sizeof(struct ip) + len];
    struct ip *iph = (struct ip *)packet;
    struct sockaddr_in dstaddr;
    struct sockaddr_in *srcaddr;
    int nsent;

    /* Fill in required IP header fields */
    srcaddr = (struct sockaddr_in *)hwas->ip_addr;
    memset(packet, 0, sizeof(packet));
    iph->ip_src.s_addr = srcaddr->sin_addr.s_addr;
    iph->ip_dst.s_addr = dstip.s_addr;
    iph->ip_p = IPPROTO_TOUR;
    iph->ip_id = htons(IPID_TOUR);
    iph->ip_hl = sizeof(struct ip) / 4;
    iph->ip_len = htons(sizeof(struct ip) + len);
    iph->ip_v = IPVERSION;
    iph->ip_ttl = htons(128);

    /* Copy data into packet */
    memcpy(packet + sizeof(struct ip), data, len);
    /* Fill in required sockaddr_in fields */
    memset(&dstaddr, 0, sizeof(dstaddr));
    dstaddr.sin_family = AF_INET;
    dstaddr.sin_addr.s_addr = dstip.s_addr;

    nsent = sendto(rt, packet, sizeof(packet), 0, (struct sockaddr *)&dstaddr, sizeof(dstaddr));
    if(nsent < 0) {
        error("raw socket failed to send: %s\n", strerror(errno));
        return 0;
    }
    return nsent;
}

int mcast_join(int sockfd, struct sockaddr_in *mcastaddr, int ifindex) {
    struct group_req req;

    memset(&req, 0, sizeof(req));
    memcpy(&req.gr_group, mcastaddr, sizeof(struct sockaddr_in));
    req.gr_interface = ifindex;

    if(setsockopt(sockfd, IPPROTO_IP, MCAST_JOIN_GROUP, &req, sizeof(req)) < 0) {
        error("failed join multicast group: %s\n", strerror(errno));
        return 0;
    }
    return 1;
}

int bind_port(int sockfd, int port) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    /* Bind to INADDR_ANY */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    if(bind(sockfd, (struct sockaddr *)&addr, len) < 0) {
        error("Failed to bind udp socket: %s\n", strerror(errno));
        return -1;
    }
    /* Get the port we are bound to */
    if(getsockname(sockfd, (struct sockaddr *)&addr, &len) < 0) {
        error("Failed to getsockname: %s\n", strerror(errno));
        return -1;
    }
    return ((int)addr.sin_port) & 0xFFFF;
}

/*
 * Determine the ip corresponding to the hostname
 *
 * @param hostname    string hostname
 * @param hostip      Pointer to an in_addr to store the IP of hostname
 * @return 1 if succeeded, 0 if failed
 */
int getipbyhost(char *hostname, struct in_addr *hostip) {
    struct hostent *he;
    struct in_addr **addr_list;

    if(hostname == NULL || hostip == NULL) {
        return 0;
    }
    if ((he = gethostbyname(hostname)) == NULL) {
        error("gethostbyname failed on %s: %s\n", hostname, hstrerror(h_errno));
        return 0;
    }
    addr_list = (struct in_addr **)he->h_addr_list;
    *hostip = *addr_list[0];
    return 1;
}
