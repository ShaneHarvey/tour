#include "ping.h"
#include "debug.h"
#include "api.h"

extern char host[64];
extern struct in_addr hostip;

char *macs[10] = {
        "\x00\x0c\x29\xde\x6a\x62", /* vm10  ..20 */
        "\x00\x0c\x29\x49\x3f\x5b", /* vm1  ..21 */
        "\x00\x0c\x29\xd9\x08\xec", /* vm2  ..22 */
        "\x00\x0c\x29\xa3\x1f\x19", /* vm3  ..23 */
        "\x00\x0c\x29\x9e\x80\x73", /* vm4  ..24 */
        "\x00\x0c\x29\xa5\x4b\x46", /* vm5  ..25 */
        "\x00\x0c\x29\xb5\x32\x3d", /* vm6  ..26 */
        "\x00\x0c\x29\x64\xe3\xd4", /* vm7  ..27 */
        "\x00\x0c\x29\xe1\x54\xd1", /* vm8  ..28 */
        "\x00\x0c\x29\xbb\x12\xaa"  /* vm9  ..29 */
};

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
    int sock = -1, icmplen;
    pthread_t self;
    ushort seq = 0;
    struct pingarg args;
    char packet[sizeof(struct ip) + ICMP_MINLEN + ICMP_DATA_LEN];
    struct ip *iph = (struct ip *)(packet);
    struct icmp *icmph = (struct icmp *)(packet + sizeof(struct ip));

    icmplen =  ICMP_MINLEN + ICMP_DATA_LEN;

    memcpy(&args, arg, sizeof(struct pingarg));
    self = pthread_self();
    /* Ensures the socket is closed before it is cancelled */
    pthread_cleanup_push(close_sock, &sock);

    /* Create packet socket used to send ICMP echo requests */
    if((sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_IP))) < 0) {
        error("failed to create packet socket: %s\n", strerror(errno));
        pthread_exit(NULL);
    }
    memset(packet, 0, sizeof(packet));
    iph->ip_src.s_addr = hostip.s_addr;
    iph->ip_dst.s_addr = args.tgtip.s_addr;
    iph->ip_p = IPPROTO_ICMP;
    iph->ip_off = htons(IP_DF);
    iph->ip_id = htons((short)self);
    iph->ip_hl = sizeof(struct ip) / 4;
    iph->ip_len = htons(sizeof(packet));
    iph->ip_v = IPVERSION;
    iph->ip_ttl = 10;
    /* Set checksum for IP header */
    iph->ip_sum = in_cksum((uint16_t *)iph, sizeof(struct ip));
    /* Fill out ICMP echo request */
    icmph->icmp_type = ICMP_ECHO;
    icmph->icmp_code = 0;
    icmph->icmp_id = htons(ICMP_ID);
    memcpy(icmph->icmp_data, ICMP_ECHO_DATA, ICMP_DATA_LEN);

    while(1) {
        struct sockaddr_in tgt;
        struct hwaddr dst;
        /* Increment echo sequence number */
        icmph->icmp_seq = htons(seq++);
        /* Recalculate the checksum */
        icmph->icmp_cksum = 0;
        icmph->icmp_cksum = in_cksum((uint16_t *)icmph, icmplen);

        memset(&tgt, 0, sizeof(tgt));
        tgt.sin_addr.s_addr = args.tgtip.s_addr;
        tgt.sin_family = AF_INET;
        /* Request MAC address of destination IP */
        if(!areq((struct sockaddr *)&tgt, sizeof(tgt), &dst)) {
            /* error("areq for %s failed: %s\n", inet_ntoa(tgt.sin_addr), strerror(errno)); */
            /* break; */

            /*TODO: AREQ, for now Use temporary macs of vms */
            int offd = (ntohl(args.tgtip.s_addr) & 0xFF) % 10;
            int offs = (ntohl(hostip.s_addr) & 0xFF) % 10;

            unsigned char *srcmac = (unsigned char *)macs[offs],
                    *dstmac = (unsigned char *)macs[offd];
            warn("areq failed for %s using mac: ", inet_ntoa(tgt.sin_addr));
            print_mac(dstmac);
            printf("\n");
            memcpy(dst.sll_addr, dstmac, ETH_ALEN);
            memcpy(args.src.if_haddr, srcmac, ETH_ALEN);

            //memset(dst.sll_addr, 0, ETH_ALEN);
            //memset(args.src.if_haddr, 0, ETH_ALEN);
            args.src.if_index = 2; /* eth0 if index */
        }
        /* Send ECHO request */
        if(!send_frame(sock, packet, sizeof(packet), dst.sll_addr, dst.sll_ifaddr, dst.sll_ifindex)) {
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
    int pg_sock, nread, iplen, icmplen;
    struct sockaddr_in src;
    socklen_t len;
    char packet[ETH_FRAME_LEN];
    struct ip *iph = (struct ip *)(packet);

    /* Ensures the socket is closed before it is cancelled */
    pthread_cleanup_push(close_sock, &pg_sock);
    /* Create the IP raw socket used to receive ICMP echo responses */
    if((pg_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
        error("failed to create raw socket: %s\n", strerror(errno));
        pthread_exit(NULL);
    }
    /* Listen for echo responses forever */
    debug("Ping receive thread listening\n");
    while(1) {
        len = sizeof(src);
        struct icmp *icmph;
        memset(&src, 0, len);
        memset(packet, 0, sizeof(packet));
        nread = recvfrom(pg_sock, packet, sizeof(packet), 0, (struct sockaddr*)&src, &len);
        iplen = iph->ip_hl * 4;
        icmplen = nread - iplen;
        icmph = (struct icmp *)(packet + iplen);
        if(nread < 0) {
            error("ping socket recvfrom failed: %s\n", strerror(errno));
            pthread_exit(NULL);
        } else if(nread < iplen + ICMP_MINLEN) {
            debug("Node %s. Received %d bytes. Too small to be ICMP ping.\n", host, nread);
        } else if(icmph->icmp_type != ICMP_ECHOREPLY) {
            debug("Node %s. Ignoring non-ping ICMP message.\n", host);
        } else if(icmph->icmp_id != htons(ICMP_ID)) {
            debug("Node %s. Received ICMP ping from other procces.\n", host);
        } else if(in_cksum((uint16_t *)(packet + iplen), icmplen) != 0) {
            debug("Node %s. Received currupt ICMP echo response.\n", host);
        } else {
            info("Node %s. Received ping from: %s, data=%s\n", host,
                    inet_ntoa(iph->ip_src), (char *)&icmph->icmp_data);
        }
    }
    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}

uint16_t in_cksum(uint16_t *addr, int len) {
    int				nleft = len;
    uint32_t		sum = 0;
    uint16_t		*w = addr;
    uint16_t		answer = 0;

    /*
     * Our algorithm is simple, using a 32 bit accumulator (sum), we add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */
    while (nleft > 1)  {
        sum += *w++;
        nleft -= 2;
    }

    /* mop up an odd byte, if necessary */
    if (nleft == 1) {
        *(unsigned char *)(&answer) = *(unsigned char *)w ;
        sum += answer;
    }

    /* add back carry outs from top 16 bits to low 16 bits */
    sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
    sum += (sum >> 16);			/* add carry */
    answer = (uint16_t) ~sum;   /* Complement and truncate to 16 bits */
    return(answer);
}

/* Ping thread creation and management */

/**
 * @head  Pointer to a list of pingt structs
 * @tgtip IP address of target node of the pinging thread
 *
 */
int create_pingt(struct head_pingt *head, struct in_addr tgtip) {
    struct pingt *new, *cur;
    int err;

    LIST_FOREACH(cur, head, list) {
        if(cur->arg.tgtip.s_addr == tgtip.s_addr) {
            info("A thread is already pinging %s.\n", inet_ntoa(tgtip));
            return 1;
        }
    }

    if((new = calloc(1, sizeof(struct pingt))) == NULL) {
        error("malloc failed: %s\n", strerror(errno));
        return 0;
    }

    new->arg.tgtip.s_addr = tgtip.s_addr;
    /* Create the thread */
    if((err = pthread_create(&new->tid, NULL, run_ping_send, &new->arg)) != 0) {
        error("failed to create ping thread: %s\n", strerror(err));
        free(new);
        return 0;
    }
    debug("Successfully started thread %u\n", (uint)new->tid);
    /* Insert into the list */
    LIST_INSERT_HEAD(head, new, list);
    return 1;
}

int destroy_pingt(struct head_pingt *head) {
    struct pingt *cur;
    int err, status = 1;


    /* Cancel all the threads, OKAY if this fails */
    LIST_FOREACH(cur, head, list) {
        pthread_cancel(cur->tid);
    }
    /* Join all the threads, NOT okay if this fails */
    LIST_FOREACH(cur, head, list) {
        if ((err = pthread_join(cur->tid, NULL)) != 0) {
            error("failed to join ping thread %u: %s\n", (uint) cur->tid,
                    strerror(err));
            status = 1;
        } else {
            debug("Successfully joined thread %u\n", (uint) cur->tid);
        }
    }
    /* Free the list */
    while (!LIST_EMPTY(head)) {
        cur = LIST_FIRST(head);
        LIST_REMOVE(cur, list);
        free(cur);
    }
    return status;
}

void print_mac(unsigned char *mac) {
    printf("%02hhX:%02hhX:%02hhX:%02hhX:%02hhX:%02hhX", mac[0], mac[1], mac[2],
            mac[3], mac[4], mac[5]);
}
