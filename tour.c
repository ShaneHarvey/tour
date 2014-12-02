#include <lber.h>
#include "tour.h"
#include "get_hw_addrs.h"
#include "ping.h"

int main(int argc, char **argv) {
    int rt, pg, udp, opt, status = EXIT_FAILURE, binded = 0;
    struct hwa_info *eth0hwa;
    /* Parse cmdline options */
    if(!valid_args(argc, argv)) {
        fprintf(stderr, "Usage: %s [hostname1 hostname2 ...] \n", argv[0]);
        return EXIT_FAILURE;
    }
    /* Create the IP raw socket used to receive ICMP echo responses */
    if((pg = socket(AF_INET, SOCK_RAW, htons(IPPROTO_ICMP))) < 0) {
        error("failed to create raw socket: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    /* Create the IP raw socket used to send and receive Tour messages */
    if((rt = socket(AF_INET, SOCK_RAW, htons(ETH_P_IP))) < 0) {
        error("failed to create raw socket: %s\n", strerror(errno));
        goto CLOSE_PG;
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

    if((eth0hwa = get_hw_addrs()) == NULL) {
        error("Failed to find system interfaces: %s\n", strerror(errno));
        goto CLOSE_UDP;
    }

    if(argc > 1) {
        /* Creating a multicast group and sending the TOUR packet */
        if(!start_tour(rt, udp, argc, argv)) {
            error("failed to start tour: %s\n", strerror(errno));
            goto FREE_HWA;
        }
        binded = 1;
    }
    /* run the tour program */
    if(!run_tour(rt, pg, udp, binded)) {
        error("Tour failed: %s\n", strerror(errno));
        goto FREE_HWA;
    }

    status = EXIT_SUCCESS; /* Normal cleanup */
FREE_HWA:
    free_hwa_info(eth0hwa);
CLOSE_UDP:
    close(udp);
CLOSE_RT:
    close(rt);
CLOSE_PG:
    close(pg);
    return status;
}

int valid_args(int argc, char **argv) {
    int i;

    for(i = 2; i < argc; ++i) {
        if(strcmp(argv[i-1], argv[i]) == 0) {
            error("Consecutive hostnames (args %d and %d) cannot be the same\n", i-1, i);
            return 0;
        }
    }
    return 1;
}

int start_tour(int rt, int udp, int argc, char **argv) {

    /* Join the multicast group */
    return 0;
}


int run_tour(int rt, int pg, int udp, int binded) {
    fd_set rset;
    int max_fd = rt > pg ? rt : pg;
    max_fd = max_fd > udp ? max_fd + 1 : udp + 1;
    while(1) {
        FD_ZERO(&rset);
        FD_SET(rt, &rset);
        FD_SET(pg, &rset);
        FD_SET(udp, &rset);
        if(select(max_fd, &rset, NULL, NULL, NULL) < 0) {
            error("select failed: %s\n", strerror(errno));
            return 0;
        }

        if(FD_ISSET(rt, &rset)) {
            /* Forward Tour message or end the tour if this is last node */
        }

        if(FD_ISSET(pg, &rset)) {
            /* Print received ping */
        }

        if(FD_ISSET(udp, &rset)) {
            /* Halt all pinging activity */
            break;
        }
    }

    /* Halt all pinging activity */

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
            info("Node %s. Received: %s\n", "TODO", data);
        }
    }
    return 1;
}