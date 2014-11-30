#include "arp.h"

int main(int argc, char **argv) {
    int unix_domain = -1;
    int pf_socket = -1;
    int success = EXIT_SUCCESS;

    struct hwa_info *devices = get_hw_addrs();
    if(devices == NULL) {
        warn("No HW devices found on the system\n");
        success = EXIT_FAILURE;
        // goto NO_HW_DEVICES;
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
    success = run_arp(unix_domain, pf_socket, devices);

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
// NO_HW_DEVICES:
    return success;
}

int run_arp(int unix_domain, int pf_socket,  struct hwa_info *devices) {
    int success = EXIT_SUCCESS;
    int running = 1;
    // select vars
    fd_set rset;
    int max_fd = unix_domain > pf_socket ? unix_domain + 1 : pf_socket + 1;
    while(running) {
        /* Now wait to receive the transmissions */
        FD_ZERO(&rset);
        FD_SET(unix_domain, &rset);
        FD_SET(pf_socket, &rset);
        select(max_fd, &rset, NULL, NULL, NULL);
        /* Handle unix domain socket communications */
        if(FD_ISSET(unix_domain, &rset)) {
        }
        /* handle PF_PACKET socket comminications */
        if(FD_ISSET(pf_socket, &rset)) {
        }
    }
    return success;
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
        }
    }
    return unix_domain;
}

int create_pf_socket(void) {
    int pf_socket = -1;
    if((pf_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ODR))) < 0) {
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
