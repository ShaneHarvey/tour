#include "api.h"

int areq(struct sockaddr *ipa, socklen_t len, struct hwaddr *hwa) {
    int sock, rv;
    struct sockaddr_un arpaddr;
    socklen_t arplen;
    struct arpreq req;
    fd_set rset;
    struct timeval tv;

    if((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        error("failed to create unix socket: %s\n", strerror(errno));
        return 0;
    }

    /* Init the ARP address */
    memset(&arpaddr, 0, sizeof(arpaddr));
    arpaddr.sun_family = AF_UNIX;
    strcpy(arpaddr.sun_path, ARP_WELL_KNOWN_PATH);
    arplen = sizeof(arpaddr.sun_family) + strlen(ARP_WELL_KNOWN_PATH);

    /* Connect to ARP */
    if(connect(sock, (struct sockaddr *)&arpaddr, arplen) < 0) {
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
        error("ARP request timed out.");
        goto CLOSE_SOCK;
    } else {
        /* The socket is readable */
        if(recv(sock, hwa, sizeof(struct hwaddr), 0)) {
            error("failed to connect to ARP: %s\n", strerror(errno));
            goto CLOSE_SOCK;
        }
    }

    close(sock);
    return 1;
CLOSE_SOCK:
    close(sock);
    return 0;
}
