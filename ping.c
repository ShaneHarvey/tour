#include "ping.h"
#include "debug.h"
#include "api.h"

static void close_sock(void *sockptr) {
    int sock = *(int *)sockptr;
    if(sock >= 0)
        close(sock);
}

/**
 * Start routine of a pthread create call. Sends a ICMP ping to the target IP
 * address every second.
 *
 * @arg  Pointer to a struct in_addr containing the IP address to ping
 */
void *run_ping(void *arg) {
    int sock = -1;
    pthread_t self;
    short seq = 0;
    struct pingarg args;
    char packet[sizeof(struct ip) + sizeof(struct icmphdr)];
    struct ip *iph = (struct ip *)(packet);
    struct icmphdr *imcph = (struct icmphdr *)(packet + sizeof(struct ip));

    memcpy(&args, arg, sizeof(struct pingarg));
    self = pthread_self();
    /* Ensures the socket is closed before it is cancelled */
    pthread_cleanup_push(close_sock, &sock);

    /* Create packet socket used to send ICMP echo requests */
    if((sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0) {
        error("failed to create packet socket: %s\n", strerror(errno));
        pthread_exit(NULL);
    }
    /*TODO: FINISH Init the ICMP (Echo data?) and IP headers */
    memset(packet, 0, sizeof(packet));
    iph->ip_dst.s_addr = args.tgtip.s_addr;
    imcph->type = ICMP_ECHO;
    imcph->un.echo.id = htons((short)self);
    while(1) {
        struct sockaddr_in tgt;
        struct hwaddr dst;
        /* Increment echo sequence number */
        imcph->un.echo.sequence = htons(seq++);
        memset(&tgt, 0, sizeof(tgt));
        tgt.sin_addr.s_addr = args.tgtip.s_addr;
        tgt.sin_family = AF_INET;
        /* Request MAC address of destination IP */
        areq((struct sockaddr *)&tgt, sizeof(tgt), &dst);
        /* Send ECHO request */
        if(!send_frame(sock, packet, sizeof(packet), dst.sll_addr, args.src.if_haddr, args.src.if_index)) {
            break;
        }
        /* Sleep for one second, sleep(2) is a pthread cancellation point */
        sleep(1);
    }
    pthread_cleanup_pop(1);
    pthread_exit(NULL);
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
    eh->h_proto = htons(ETH_P_IP);

    /* Copy frame data into buffer */
    memcpy(frame + sizeof(struct ethhdr), payload, size);

    /* Initialize sockaddr_ll */
    memset(&dest, 0, sizeof(struct sockaddr_ll));
    dest.sll_family = AF_PACKET;
    dest.sll_ifindex = ifi_index;
    memcpy(dest.sll_addr, dstmac, ETH_ALEN);
    dest.sll_halen = ETH_ALEN;
    dest.sll_protocol = htons(ETH_P_IP);

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
