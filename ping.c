#include "ping.h"
#include "debug.h"
#include "api.h"

extern char *host;

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
void *run_ping_send(void *arg) {
    int sock = -1;
    pthread_t self;
    short seq = 0;
    struct pingarg args;
    char packet[sizeof(struct ip) + sizeof(struct icmphdr)];
    struct ip *iph = (struct ip *)(packet);
    struct icmphdr *icmph = (struct icmphdr *)(packet + sizeof(struct ip));

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
    icmph->type = ICMP_ECHO;
    icmph->un.echo.id = htons((short)self);
    while(1) {
        struct sockaddr_in tgt;
        struct hwaddr dst;
        /* Increment echo sequence number */
        icmph->un.echo.sequence = htons(seq++);
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

/**
* Start routine of a pthread create call. Receives ICMP echo responses and
* prints out a message.
*
* @unused  Unused
*/
void *run_ping_recv(void *unused) {
    int pg_sock, nread;
    struct sockaddr_in src;
    socklen_t len;
    char packet[IP_MSS];
    struct ip *iph = (struct ip *)(packet);
    struct icmphdr *icmph = (struct icmphdr *)(packet + sizeof(struct ip));
    /* char *icmpdata = (char *)(icmph + sizeof(struct icmphdr)); */

    /* Ensures the socket is closed before it is cancelled */
    pthread_cleanup_push(close_sock, &pg_sock);
    /* Create the IP raw socket used to receive ICMP echo responses */
    if((pg_sock = socket(AF_INET, SOCK_RAW, htons(IPPROTO_ICMP))) < 0) {
        error("failed to create raw socket: %s\n", strerror(errno));
        pthread_exit(NULL);
    }
    /* Listen for echo responses forever */
    while(1) {
        len = sizeof(src);
        memset(&src, 0, len);
        memset(packet, 0, sizeof(packet));
        nread = recvfrom(pg_sock, packet, sizeof(packet), 0, (struct sockaddr*)&src, &len);
        if(nread < 0) {
            error("ping socket recvfrom failed: %s\n", strerror(errno));
            pthread_exit(NULL);
        } else if(icmph->type != ICMP_ECHOREPLY) {
            debug("Node %s. Ignoring non-ping ICMP message.\n", host);
        } else if(nread < sizeof(struct ip) + sizeof(struct icmphdr)) {
            debug("Node %s. Received %d bytes. Too small to be ICMP ping.\n", host, nread);
        } else {
            info("Node %s. Received ping from: %s\n", host, inet_ntoa(iph->ip_src));
        }
    }
    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}
