#include "tour.h"

int main(int argc, char **argv) {
    int rt, pg, udp, opt;

    /* Parse cmdline options */

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
    /* Join the multicast group */
    goto CLOSE_UDP;
    /* Wait for message on tour socket */

    close(udp);
    close(rt);
    close(pg);
    return EXIT_SUCCESS;
CLOSE_UDP:
    close(udp);
CLOSE_RT:
    close(rt);
CLOSE_PG:
    close(pg);
    return EXIT_FAILURE;
}

int tour(void) {
    return -1;
}
