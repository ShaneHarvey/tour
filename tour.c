#include "tour.h"
#include "ping.h"

char host[64];  /* Hostname running ODR, eg vm2 */
struct in_addr hostip;     /* IP of this host */

int main(int argc, char **argv) {
    int rt, udp_recv, udp_send, opt, status = EXIT_FAILURE, binded = 0;
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
    /* Create the UDP socket used to receive multicast messages */
    if((udp_recv = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        error("failed to create UDP socket: %s\n", strerror(errno));
        goto CLOSE_RT;
    }
    /* Create the UDP socket used to send multicast messages */
    if((udp_send = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        error("failed to create UDP socket: %s\n", strerror(errno));
        goto CLOSE_UDP_RECV;
    }
    /* Lookup our hostname */
    if(gethostname(host, sizeof(host)) < 0) {
        error("gethostname failed: %s\n", strerror(errno));
        goto CLOSE_UDP_SEND;
    } else {
        getipbyhost(host, &hostip);
        info("Tour running on node %s, IP %s\n", host, inet_ntoa(hostip));
    }

    if(argc > 1) {
        /* Creating a multicast group and sending the TOUR packet */
        if(!start_tour(rt, udp_recv, udp_send, argc - 1, argv + 1)) {
            error("failed to start tour: %s\n", strerror(errno));
            goto CLOSE_UDP_SEND;
        }
        binded = 1;
    }
    /* run the tour program */
    if(!run_tour(rt, udp_recv, udp_send, binded)) {
        error("Tour failed: %s\n", strerror(errno));
        goto CLOSE_UDP_SEND;
    }

    status = EXIT_SUCCESS; /* Normal cleanup */
CLOSE_UDP_SEND:
    close(udp_send);
CLOSE_UDP_RECV:
    close(udp_recv);
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

int start_tour(int rt, int udp_recv, int udp_send, int numhosts, char **hosts) {
    struct in_addr mcastip;
    char tourmsg[TOUR_MAXPACKET];
    struct tourhdr *tour = (struct tourhdr *) tourmsg;
    struct sockaddr_in mcastaddr;
    int port, i, j;

    if((port = bind_port(udp_recv, 0)) < 0)  {
        return 0;
    }

    inet_aton(MCAST_ADDR, &mcastip);
    /* Join the multicast group */
    if (!mcast_join(udp_recv, mcastip, 0)) {
        return 0;
    }
    /* Connect to the multicast address */
    memset(&mcastaddr, 0, sizeof(mcastaddr));
    mcastaddr.sin_family = AF_INET;
    mcastaddr.sin_addr.s_addr = mcastip.s_addr;
    mcastaddr.sin_port = port;
    if(connect(udp_send, (struct sockaddr*)&mcastaddr, sizeof(mcastaddr)) < 0) {
        error("UDP failed to connect: %s\n", strerror(errno));
        return 0;
    }

    /* Create the initial tour message */
    memset(tourmsg, 0, sizeof(tourmsg));
    tour->mcastip.s_addr = mcastip.s_addr;
    tour->mcastport = port;
    tour->len = numhosts;

    /* IPs are in reverse order */
    for(i = 0, j = numhosts - 1; i < numhosts; ++i, --j) {
        if(!getipbyhost(hosts[i], tour->ips + j)) {
            return 0;
        }
        debug("Host %d: %s, IP=%s\n", i + 1, hosts[i], inet_ntoa(tour->ips[j]));
    }
    return forward_tour(rt, tour);
}

int run_tour(int rt, int udp_recv, int udp_send, int binded) {
    fd_set rset;
    int max_fd, err;
    pthread_t recvthread;
    struct head_pingt head;

    /* Initialize the list of pinging threads */
    LIST_INIT(&head);

    /* start the ping recv thread */
    if((err = pthread_create(&recvthread, NULL, run_ping_recv, NULL)) != 0) {
        error("pthread_create failed: %s\n", strerror(err));
        return 0;
    }

    max_fd = rt > udp_recv ? rt + 1: udp_recv + 1;
    while(1) {
        FD_ZERO(&rset);
        FD_SET(rt, &rset);
        FD_SET(udp_recv, &rset);
        if(select(max_fd, &rset, NULL, NULL, NULL) < 0) {
            error("select failed: %s\n", strerror(errno));
            goto CLEANUP_THREADS;
        }

        if(FD_ISSET(udp_recv, &rset)) {
            /* Halt all pinging activity */
            char buf[1024];
            struct sockaddr_in src;
            socklen_t srclen;
            int nread;

            memset(buf, 0, sizeof(buf));
            memset(&src, 0, sizeof(src));
            srclen = sizeof(src);

            nread = recvfrom(udp_recv, buf, sizeof(buf), 0, (struct sockaddr *)&src, &srclen);
            if(nread < 0) {
                error("UDP socket recvfrom failed: %s\n", strerror(errno));
                goto CLEANUP_THREADS;
            }
            info("Node %s. Received: %s\n", host, buf);
            break;
        }

        if(FD_ISSET(rt, &rset)) {
            char packet[IP_MAXPACKET];
            struct ip *iph = (struct ip *)packet;
            struct tourhdr *tour = (struct tourhdr *)(packet + sizeof(struct ip));
            struct sockaddr_in src;
            socklen_t srclen;
            int nread;

            memset(packet, 0, sizeof(packet));
            memset(&src, 0, sizeof(src));
            srclen = sizeof(src);

            nread = recvfrom(rt, packet, sizeof(packet), 0, (struct sockaddr *)&src, &srclen);
            if(nread < 0) {
                error("raw socket recvfrom failed: %s\n", strerror(errno));
                goto CLEANUP_THREADS;
            } else if(nread < sizeof(struct ip) + sizeof(struct tourhdr)) {
                debug("Ignoring %d byte tour message: too short\n", nread);
            } else if(ntohs(iph->ip_len) < sizeof(struct ip) + sizeof(struct tourhdr)) {
                debug("Ignoring %d byte tour message: too short\n", nread);
            } else if(iph->ip_id != htons(IPID_TOUR)) {
                debug("Ignoring %d byte tour message: wrong IP id field\n", nread);
            } else {
                debug("Recv %d byte raw ip packet.\n", nread);
                print_ip(iph);
                print_tour(tour);
                tour->len = ntohs(tour->len);
                info("Node %s. Received tour packet: from %s, remaining hops "
                        "%d\n", host, inet_ntoa(iph->ip_src),  tour->len);
                if(!binded) {
                    int port;
                    struct sockaddr_in mcastaddr;
                    /* Bind to the multicast port */
                    if((port = bind_port(udp_recv, tour->mcastport)) < 0)  {
                        goto CLEANUP_THREADS;
                    }
                    /* Connect to the multicast address */
                    memset(&mcastaddr, 0, sizeof(mcastaddr));
                    mcastaddr.sin_family = AF_INET;
                    mcastaddr.sin_addr.s_addr = tour->mcastip.s_addr;
                    mcastaddr.sin_port = port;
                    if(connect(udp_send, (struct sockaddr*)&mcastaddr, sizeof(mcastaddr)) < 0) {
                        error("UDP failed to connect: %s\n", strerror(errno));
                        goto CLEANUP_THREADS;
                    }
                    /* Join the multicast group */
                    if (!mcast_join(udp_recv, tour->mcastip, 0)) {
                        goto CLEANUP_THREADS;
                    }
                    info("Node %s. Binded to port %hu, joined multicast "
                            "address %s\n", host, ntohs(port),
                            inet_ntoa(tour->mcastip));
                    binded = 1;
                }
                /* Start pinging */
                if(!create_pingt(&head, src.sin_addr)) {
                    goto CLEANUP_THREADS;
                }

                /* Forward Tour message or end the tour if this is last node */
                if(tour->len > 0) {
                    if(!forward_tour(rt, tour)) {
                        goto CLEANUP_THREADS;
                    }
                } else {
                    char buf[1024];
                    info("Tour reached final host\n");
                    /* Spend 5 seconds pinging */
                    sleep(5);

                    snprintf(buf, sizeof(buf), "<<<<< This is node %s. Tour "
                            "has ended. Group members please identify "
                            "yourselves. >>>>>", host);
                    info("Node %s. Sending: %s\n", host, buf);
                    if(send(udp_send, buf, strlen(buf), 0) < 0) {
                        error("UDP socket sendto failed: %s\n", strerror(errno));
                        goto CLEANUP_THREADS;
                    }
                }
            }
        }
    }

    /* Halt all pinging activity */
    if((err = pthread_cancel(recvthread)) ||(err = pthread_join(recvthread, NULL))){
        error("failed to cancel ping recv thread: %s\n", strerror(err));
        goto CLEANUP_TLIST;
    }
    /* Clean up all the pinging threads */
    if(!destroy_pingt(&head)) {
        return 0;
    }

    if(!end_tour(udp_recv, udp_send)) {
        error("Failed to end tour %s\n", strerror(errno));
        return 0;
    }
    return 1;
CLEANUP_THREADS:
    pthread_cancel(recvthread);
    pthread_join(recvthread, NULL);
CLEANUP_TLIST:
    destroy_pingt(&head);
    return 0;
}

int end_tour(int udp_recv, int udp_send) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "<<<<< Node %s. I am a member of the group. "
            ">>>>>", host);
    info("Node %s. Sending: %s\n", host, buf);
    if(send(udp_send, buf, strlen(buf), 0) < 0) {
        error("UDP socket sendto failed: %s\n", strerror(errno));
        return 0;
    }
    while(1) {
        struct timeval tv = {5L, 0L};
        int rv;
        fd_set rset;
        /* Print out multicast messages until we reach 5 second timeout */
        FD_ZERO(&rset);
        FD_SET(udp_recv, &rset);
        rv = select(udp_recv + 1, &rset, NULL, NULL, &tv);
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
            socklen_t slen = sizeof(source);
            memset(data, 0, sizeof(data));
            /* UDP socket is readable, print multicast message */
            nread = recvfrom(udp_recv, data, sizeof(data), 0, (struct sockaddr *)
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
    print_tour(tour);
    return send_ip(rt, tour, size, nextip);
}

int send_ip(int rt, void *data, size_t len, struct in_addr dstip) {
    char packet[sizeof(struct ip) + len];
    struct ip *iph = (struct ip *)packet;
    struct sockaddr_in dstaddr;
    int nsent;

    /* Fill in required IP header fields */
    memset(packet, 0, sizeof(packet));
    iph->ip_src.s_addr = hostip.s_addr;
    iph->ip_dst.s_addr = dstip.s_addr;
    iph->ip_p = IPPROTO_TOUR;
    iph->ip_off = htons(IP_DF);
    iph->ip_id = htons(IPID_TOUR);
    iph->ip_hl = sizeof(struct ip) / 4;
    iph->ip_len = htons(sizeof(struct ip) + len);
    iph->ip_v = IPVERSION;
    iph->ip_ttl = 10;

    /* Copy data into packet */
    memcpy(packet + sizeof(struct ip), data, len);
    /* Fill in required sockaddr_in fields */
    memset(&dstaddr, 0, sizeof(dstaddr));
    dstaddr.sin_family = AF_INET;
    dstaddr.sin_addr.s_addr = dstip.s_addr;

    print_ip(iph);

    nsent = sendto(rt, packet, sizeof(packet), 0, (struct sockaddr *)&dstaddr, sizeof(dstaddr));
    if(nsent < 0) {
        error("raw socket failed to send: %s\n", strerror(errno));
        return 0;
    }
    debug("Sent %d byte raw ip packet.\n", nsent);
    return nsent;
}

int mcast_join(int sockfd, struct in_addr mcastip, int ifindex) {
    struct group_req req;
    struct sockaddr_in mcastaddr;
    memset(&mcastaddr, 0, sizeof(mcastaddr));
    mcastaddr.sin_family = AF_INET;
    mcastaddr.sin_addr.s_addr = mcastip.s_addr;

    memset(&req, 0, sizeof(req));
    memcpy(&req.gr_group, &mcastaddr, sizeof(struct sockaddr_in));
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
    addr.sin_port = port;
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

/*
 *@hdr The tour packet in Network byte order
 */
void print_tour(struct tourhdr *hdr) {
    int i;

    printf("TOUR: mcastaddr:%s mcastport:%hu len:%hu ips:",
            inet_ntoa(hdr->mcastip), ntohs(hdr->mcastport), ntohs(hdr->len));
    for(i = 0; i < ntohs(hdr->len); ++i) {
        printf(" <-%s", inet_ntoa(hdr->ips[i]));
    }
    printf("\n");
}

/*
 *@hdr The ip header in Network byte order
 */
void print_ip(struct ip *hdr) {

    printf("IP: tos:0x%02hhX len:0x%04hX id:0x%04hX off:0x%04hX "
            "ttl:0x%02hhX p:0x%02hhX sum:0x%04hX src:%s ", hdr->ip_tos,
            hdr->ip_len, hdr->ip_id, hdr->ip_off, hdr->ip_ttl, hdr->ip_p,
            hdr->ip_sum,inet_ntoa(hdr->ip_src));
    printf("dst:%s\n", inet_ntoa(hdr->ip_dst));
}
