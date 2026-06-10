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


Player players[NUM_PLAYERS];


typedef struct messClient{
	char direzione;
	bool movimento;
}MessClient;
typedef struct messRicevuto{
       Mappa mappaPlayer;
       Player p;
       Player players[NUM_PLAYERS];
       MsgType type;
       Statistiche statistics[NUM_PLAYERS];

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

static ssize_t readn_all(int fd, void *buf, size_t len)
{
    size_t off = 0;

    while (off < len) {


        ssize_t r = recv(fd, ((char*)buf) + off, len - off, 0);

        if (r == 0)
            return 0;

        if (r < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        off += r;
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

    printf("Benvenuto nel gioco!");  fflush(stdout);
   
    MessClient messIniziale;
    messIniziale.direzione = 'X'; // Carattere fittizio
    messIniziale.movimento = false; 
    Statistiche ultimeStatistiche[NUM_PLAYERS]; 

    if (writen_all(sockfd, &messIniziale) < 0) {
        perror("Invio richiesta iniziale fallito");
        close(sockfd);
        return 1;
    }

    // ---- multiplex loop ----
    int stdin_open = 1;
    fd_set rset;
    char carattere[32];
    int running = 1;
    bool globalUpdate = false;

    Mappa mappaGlobale;
    Mappa mappaLocale;

    memset(&mappaGlobale, ' ', sizeof(Mappa));
    memset(&mappaLocale, ' ', sizeof(Mappa));
    memset(ultimeStatistiche, 0, sizeof(ultimeStatistiche));
    memset(players, 0, sizeof(players));

    while(running) {

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

        // stdin pronto: leggi e invia al server
        if (stdin_open && FD_ISSET(STDIN_FILENO, &rset)) {
            if (fgets(carattere, sizeof(carattere), stdin) == NULL) {
                if (shutdown(sockfd, SHUT_WR) < 0) {
                    perror("shutdown");
                    break;
                }
                stdin_open = 0;
            } else {
                // Se l'utente ha premuto solo INVIO (stringa vuota o solo \n), lo ignoriamo
                if (carattere[0] == '\n' || carattere[0] == '\0') {
                    continue; 
                }

                carattere[0] = toupper((unsigned char)carattere[0]);

                if (carattere[0] != 'W' && carattere[0] != 'A' && carattere[0] != 'S' && carattere[0] != 'D') {
                    printf("Carattere non valido! Inserire A, D, W o S.\n");
                    continue;
                }

                MessClient mess;
                mess.direzione = carattere[0];
                mess.movimento = true; // Questo è un vero movimento

    if (writen_all(sockfd, &mess) < 0) {
        perror("send");
        break;
    }
            }
        }

        

        if (FD_ISSET(sockfd, &rset)) {
            MessRicevuto messRicevuto;
            ssize_t n = readn_all(sockfd, &messRicevuto, sizeof(messRicevuto));

            if (n < 0) { perror("recv"); break; }
            if (n == 0) { break; }
            if (messRicevuto.type == MSG_GAME_OVER) { printf("GAME OVER\n"); break; }

            for(int k = 0; k < NUM_PLAYERS; k++) {
                players[k] = messRicevuto.players[k];
            }

            if(messRicevuto.type == MSG_UPDATE) {
                mappaLocale = messRicevuto.mappaPlayer;
            }
    
            if(messRicevuto.type == MSG_GLOBAL_UPDATE) {
                mappaGlobale = messRicevuto.mappaPlayer;
                globalUpdate = true; // Diventa true SOLO se è un update globale

               for(int k = 0; k < NUM_PLAYERS; k++) {
                    ultimeStatistiche[k] = messRicevuto.statistics[k];
                }
            }
    
            stampaMappa(mappaLocale, mappaGlobale, globalUpdate, ultimeStatistiche);
        }
    
}
    close(sockfd);
    return 0;
}


Colore getColoreCasella(int i, int j, char mappaPlayer[N][N], char mappa[N][N]) {

    if(mappa[i][j] == ' ')
        return GRIGIO;

    else if(mappa[i][j] == MURO)
        return BIANCO;

    else{
        for(int k = 0; k < NUM_PLAYERS; k++) {
            if(mappaPlayer[i][j] == players[k].lettera)
                return players[k].colorePlayer; 
        }
    
        return BLACK;
    }
}

void stampaMappa(Mappa mappaLocale, Mappa mappaGlobale, bool globalUpdate, Statistiche statistics[NUM_PLAYERS]) {

    //printf("\033[2J\033[H");
    system("clear");
    
    if(globalUpdate) {
        
        for(int i = 0; i < N; i++) {
            for(int j = 0; j < N; j++) {
                Colore colore = getColoreCasella(i, j, mappaGlobale.mappaPlayer, mappaGlobale.mappa);
               
                printf("%s %-2c%s", colori[colore], mappaGlobale.mappa[i][j], colori[RESET_COLOR]);
            }
            printf("\n");
        }

        printf("\nStatistiche:\n");
        for(int k = 0; k < NUM_PLAYERS; k++) {
            if(players[k].lettera != '\0') {
                printf("Giocatore %c: %d celle conquistate\n", players[k].lettera, statistics[k].celleConquistate);
            }
        }
   }
    
   printf("\n");
    for(int i = 0; i < N; i++) {
        for(int j = 0; j < N; j++) {
            Colore colore = getColoreCasella(i, j, mappaLocale.mappaPlayer, mappaLocale.mappa);
            printf("%s %-2c%s", colori[colore], mappaLocale.mappa[i][j], colori[RESET_COLOR]);
        }
        printf("\n");
    }
        
    fflush(stdout);
}
