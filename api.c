#include "api.h"

int areq(struct sockaddr *ipa, socklen_t len, struct hwaddr *hwa) {
    int sock, rv;
    struct sockaddr_un arpaddr;
    struct areq req;
    fd_set rset;
    struct timeval tv;

    info("AREQ for IP: %s\n", inet_ntoa(((struct sockaddr_in *)ipa)->sin_addr));

    if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        error("failed to create unix socket: %s\n", strerror(errno));
        return 0;
    }

    /* Init the ARP address */
    memset(&arpaddr, 0, sizeof(arpaddr));
    arpaddr.sun_family = AF_UNIX;
    strcpy(arpaddr.sun_path, ARP_WELL_KNOWN_PATH);

    /* Connect to ARP */
    if(connect(sock, (struct sockaddr *)&arpaddr, SUN_LEN(&arpaddr)) < 0) {
        error("failed to connect to ARP: %s\n", strerror(errno));
        goto CLOSE_SOCK;
    }

    /* Init ARP request */
    memcpy(&req.addr, ipa, sizeof(struct sockaddr));
    req.addrlen = len;

    /* Send the ARP request */
    if(send(sock, &req, sizeof(req), 0) < 0) {
        error("failed to connect to ARP: %s\n", strerror(errno));
        goto CLOSE_SOCK;
    }

    /* Wait 2 seconds for ARP response */
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    FD_ZERO(&rset);
    FD_SET(sock, &rset);
    rv = select(sock + 1, &rset, NULL, NULL, &tv);
    if(rv < 0) {
        error("select failed: %s\n", strerror(errno));
        goto CLOSE_SOCK;
    } else if(rv == 0) {
        error("ARP request timed out.\n");
        goto CLOSE_SOCK;
    } else {
        int nread;
        /* The socket is readable */
        if((nread = recv(sock, hwa, sizeof(struct hwaddr), 0)) < 0) {
            error("failed to recv from ARP: %s\n", strerror(errno));
            goto CLOSE_SOCK;
        } else if(nread < sizeof(struct hwaddr)) {
            errno = EPROTO;
            error("ARP failed: %s\n", strerror(errno));
            goto CLOSE_SOCK;
        }
        info("AREQ returned <IP: %s, ETH: ", inet_ntoa(((struct
                sockaddr_in *)ipa)->sin_addr));
        print_hwa(hwa);
        printf(">\n");
    }

    close(sock);
    return 1;
CLOSE_SOCK:
    warn("AREQ for IP: %s failed\n", inet_ntoa(((struct sockaddr_in *)ipa)->sin_addr));
    close(sock);
    return 0;
}



void print_hwa(struct hwaddr *hwa) {
    int i;
    for(i = 0; i < hwa->sll_halen - 1; ++i) {
        printf("%02hhX:", hwa->sll_addr[i]);
    }
    printf("%02hhX", hwa->sll_addr[i]);
}