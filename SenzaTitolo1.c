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
#include <ctype.h>
#include "mappa.h"

#define MAXLINE 4096
typedef struct messClient{
	char direzione;
	bool movimento;
}MessClient;
typedef struct messRicevuto{
       Mappa mappa;
       Player p;
}MessRicevuto;

static ssize_t writen_all(int fd, MessClient *mess) {

    size_t off = 0;

    while (off < sizeof(MessClient)) {

        ssize_t w = send(fd,
                         ((char*)mess) + off,
                         sizeof(MessClient) - off,
                         0);

        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }

        off += w;
    }

    return off;
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
        printf("Trying to connect to %s:%s...\n", argv[1], argv[2]);
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd < 0) continue;
        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(sockfd); sockfd = -1;
    }
    freeaddrinfo(res);
    if (sockfd < 0) { perror("connect"); return 1; }

    Mappa mappa;
    printf("Benvenuto nel gioco!");  fflush(stdout);

    // ---- multiplex loop ----
    int stdin_open = 1;                // finché non va in EOF
    fd_set rset;
    char sendline[MAXLINE], recvline[MAXLINE], carattere[32];


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
        MessRicevuto messRicevuto;
        if (FD_ISSET(sockfd, &rset)) {
            ssize_t n = recv(sockfd, &messRicevuto, sizeof(messRicevuto), 0);
            if (n < 0) { perror("recv"); break; }
            if (n == 0) {                 // server ha chiuso
                if (stdin_open)
                    fprintf(stderr, "server terminated prematurely\n");

                break;
            }
            stampaMappa(messRicevuto.p, messRicevuto.mappa.mappa, messRicevuto.mappa.mappaPlayer);
        }

        // stdin pronto: leggi e invia al server
       if (stdin_open && FD_ISSET(STDIN_FILENO, &rset)) {

        if (fgets(carattere, sizeof(carattere), stdin) == NULL) {

            if (shutdown(sockfd, SHUT_WR) < 0) {
                perror("shutdown");
                break;
            }

        } else {

            carattere[0] = toupper((unsigned char)carattere[0]);

            if (carattere[0] != 'W' && carattere[0] != 'A' && carattere[0] != 'S' && carattere[0] != 'D') {

                printf("Carattere non valido! Inserire A, D, W o S.\n");
                continue;
            }

            MessClient mess;

            mess.direzione = carattere[0];
            mess.movimento = true;

            if (writen_all(sockfd, &mess) < 0) {
                perror("send");
                break;
            }
        }
    }
}
    
    close(sockfd);
    return 0;
}

Colore getColoreCasella(Player p, int i, int j, char mappaPlayer[N][N], char mappa[N][N]) {

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

    printf("\033[H\033[J");

    for(int i = 0; i < N; i++) {

        for(int j = 0; j < N; j++) {

            Colore colore =
                getColoreCasella(p, i, j,
                                 mappaPlayer,
                                 mappa);

            printf("%s %-2c%s", colori[colore], mappa[i][j], colori[RESET_COLOR]);
        }

        printf("\n");
    }

    fflush(stdout);
}