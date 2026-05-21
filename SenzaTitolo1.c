#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include "../Server/mappa.h"

#define MAXLINE 4096

static ssize_t writen_all(int fd, const void *buf, size_t n) {
    const char *p = (const char *)buf;
    size_t off = 0;
    while (off < n) {
        ssize_t w = send(fd, p + off, n - off, 0);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += (size_t)w;
    }
    return (ssize_t)off;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        return 1;
    }

    // ---- resolve & connect ----
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(argv[1], argv[2], &hints, &res);
    if (err) { fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err)); return 1; }

    int sockfd = -1;
    for (rp = res; rp; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd < 0) continue;
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(sockfd); sockfd = -1;
    }
    freeaddrinfo(res);
    if (sockfd < 0) { perror("connect"); return 1; }

    // ---- multiplex loop ----
    int stdin_open = 1;                // finché non va in EOF
    fd_set rset;
    char sendline[MAXLINE], recvline[MAXLINE];

    for (;;) {
        FD_ZERO(&rset);
        if (stdin_open) FD_SET(STDIN_FILENO, &rset);
        FD_SET(sockfd, &rset);
        int maxfd = (STDIN_FILENO > sockfd ? STDIN_FILENO : sockfd) + 1;

        int nready = select(maxfd, &rset, NULL, NULL, NULL);
        if (nready < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        // socket pronta: leggi risposta server
        if (FD_ISSET(sockfd, &rset)) {
            ssize_t n = recv(sockfd, recvline, sizeof(recvline), 0);
            if (n < 0) { perror("recv"); break; }
            if (n == 0) {                 // server ha chiuso
                if (stdin_open)
                    fprintf(stderr, "server terminated prematurely\n");
                break;
            }
            if (write(STDOUT_FILENO, recvline, (size_t)n) < 0) { perror("write stdout"); break; }
        }

        // stdin pronto: leggi e invia al server
        if (stdin_open && FD_ISSET(STDIN_FILENO, &rset)) {
            if (fgets(sendline, sizeof(sendline), stdin) == NULL) {
                // EOF su stdin: niente più richieste → chiudi solo la scrittura
                stdin_open = 0;
                if (shutdown(sockfd, SHUT_WR) < 0) { perror("shutdown"); break; }
                // non usciamo: continuiamo a leggere eventuale output dal server
            } else {
                size_t len = strlen(sendline);
                if (writen_all(sockfd, sendline, len) < 0) { perror("send"); break; }
            }
        }
    }

    close(sockfd);
    return 0;
}

Colore getColoreCasella(Player p,
                        int i,
                        int j,
                        char mappaPlayer[N][N],
                        char mappa[N][N]) {

    if(mappa[i][j] == ' ')
        return GRIGIO;

    else if(mappaPlayer[i][j] == p.lettera)
        return p.colorePlayer;

    else if(mappa[i][j] == MURO)
        return BIANCO;

    else
        return BLACK;
}

void stampaMappa(Player p,
                 char mappa[N][N],
                 char mappaPlayer[N][N]) {

    for(int i = 0; i < N; i++) {

        for(int j = 0; j < N; j++) {

            Colore colore =
                getColoreCasella(p,
                                 i,
                                 j,
                                 mappaPlayer,
                                 mappa);

            printf("%s%c %s",
                   colori[colore],
                   mappa[i][j],
                   colori[0]);
        }

        printf("\n");
    }
}
