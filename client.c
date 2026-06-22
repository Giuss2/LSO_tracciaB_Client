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
#include "game.h"

#define MAXLINE 4096
Player players[NUM_PLAYERS];


static ssize_t writen_all(int fd, MessClient *mess) {

    size_t off = 0;

    while (off < sizeof(MessClient)) {

        ssize_t w = send(fd, ((char*)mess) + off, sizeof(MessClient) - off, 0);

        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }

        off += w;
    }

    return off;
}

static ssize_t readn_all(int fd, void *buf, size_t len){
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

    //--- REGISTRAZIONE E/O AUTENTICAZIONE --- 

    printf("Benvenuto nel gioco! \n");  fflush(stdout);
    MessClient messIniziale;
    MessRicevuto messRicevuto;
    bool autenticato = false;
    int running = 0;

    while (!autenticato) {
        char scelta[32];

        do {
            printf("Premi R per registrarti oppure L per fare il login: ");
            fflush(stdout);

            if (fgets(scelta, sizeof(scelta), stdin) == NULL) {
                close(sockfd);
                return 1; 
            }

            scelta[0] = toupper((unsigned char)scelta[0]);

        } while(scelta[0] != 'L' && scelta[0] != 'R');

        memset(&messIniziale, 0, sizeof(messIniziale));
        if(scelta[0] == 'R')
            messIniziale.type = MSG_SUBSCRIBE;
        else
            messIniziale.type = MSG_LOGIN;
       
        messIniziale.movimento = false; 
        
        printf("Inserisci username: ");
        fgets(messIniziale.username, sizeof(messIniziale.username), stdin);
        messIniziale.username[strcspn(messIniziale.username, "\n")] = '\0';
        
        printf("Inserisci password: ");
        fgets(messIniziale.password, sizeof(messIniziale.password), stdin);
        messIniziale.password[strcspn(messIniziale.password, "\n")] = '\0';

        
        if (writen_all(sockfd, &messIniziale) < 0) {
            perror("Invio richiesta iniziale fallito");
            close(sockfd);
            return 1;
        }

        // Aspetta la risposta giusta dal server
        do{
            memset(&messRicevuto, 0, sizeof(messRicevuto));
        
            ssize_t n = readn_all(sockfd, &messRicevuto, sizeof(messRicevuto));

            if (n <= 0) { 
                fprintf(stderr, "Connessione persa o chiusa dal server.\n"); 
                close(sockfd); 
                exit(1); 
            }
            
        }while(messRicevuto.type == MSG_GLOBAL_UPDATE);

        // Controllo l'esito della risposta del server
        if((messRicevuto.type == MSG_SUBSCRIBE || messRicevuto.type == MSG_LOGIN) && (strcmp(messRicevuto.p.username, "FAIL") != 0)){
            printf("Autenticazione completata con successo! Entro in gioco...\n");
            running = 1;
            autenticato = true; // Usciamo dal loop di login ed entriamo nel gioco
        } else {
            if(messRicevuto.type == MSG_SUBSCRIBE){
                printf("\n[ERRORE] Registrazione fallita: lo username esiste gia'. Riprova.\n\n");
            } else if(messRicevuto.type == MSG_LOGIN){
                printf("\n[ERRORE] Login fallito: credenziali errate. Riprova.\n\n");
            }
        }
    }

    
    // Socket UDP
    struct sockaddr_in addr;
    char buffer[1024];
    int sock_broadcast = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_broadcast < 0) {
        perror("socket");
        exit(1);
    }

    int yes = 1;
    setsockopt(sock_broadcast, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    // Bind su tutte le interfacce
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock_broadcast, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind UDP");
        exit(1);
    }

    Statistiche ultimeStatistiche[NUM_PLAYERS]; 


    // ---- multiplex loop ----
    int stdin_open = 1;
    fd_set rset;
    char carattere[32];
    
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
        FD_SET(sock_broadcast, &rset);

       int maxfd = (STDIN_FILENO > sockfd) ? STDIN_FILENO : sockfd;

        if (sock_broadcast > maxfd) {
            maxfd = sock_broadcast;
        }
        maxfd += 1;


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

                if (carattere[0] != 'W' && carattere[0] != 'A' && carattere[0] != 'S' && carattere[0] != 'D' && carattere[0] != 'U') {
                    printf("Carattere non valido! Inserire A, D, W o S. Premere U se si desidera Uscire dal gioco.");
                    continue;
                }

                MessClient mess;
                memset(&mess, 0, sizeof(mess));
                mess.direzione = carattere[0];
                mess.movimento = true; 

                if (writen_all(sockfd, &mess) < 0) {
                    perror("send");
                    break;
                }
            }
        }

        
        //leggi le risposte ricevute dal server
        if (FD_ISSET(sockfd, &rset)) {
            MessRicevuto messRicevuto;
            memset(&messRicevuto, 0, sizeof(messRicevuto));
            ssize_t n = readn_all(sockfd, &messRicevuto, sizeof(messRicevuto));

            if (n <= 0) { perror("Connessione persa o chiusa dal server "); break; }



            memset(players, 0, sizeof(players));
            for(int k = 0; k < NUM_PLAYERS; k++)
                players[k] = messRicevuto.players[k];

            if(messRicevuto.type == MSG_UPDATE)
                mappaLocale = messRicevuto.mappaPlayer;
    
            if(messRicevuto.type == MSG_GLOBAL_UPDATE) {
                mappaGlobale = messRicevuto.mappaPlayer;
                globalUpdate = true; 

               for(int k = 0; k < NUM_PLAYERS; k++) {
                    ultimeStatistiche[k] = messRicevuto.statistics[k];
                }
            }
    
            stampaMappa(mappaLocale, mappaGlobale, globalUpdate, ultimeStatistiche);
        }

        // ascolta messaggi broadcast dal server
        if (FD_ISSET(sock_broadcast, &rset)) {
            MessBroadcast messRicevuto;
            memset(&messRicevuto, 0, sizeof(messRicevuto));
            ssize_t n = recv(sock_broadcast, &messRicevuto, sizeof(messRicevuto), 0);

            if (n <= 0) { perror("Connessione persa o chiusa dal server "); break; }


            memset(players, 0, sizeof(players));
            for(int k = 0; k < NUM_PLAYERS; k++)
                players[k] = messRicevuto.players[k];
        
            if (messRicevuto.type == MSG_GAME_OVER) { 
                system("clear"); 
                if (messRicevuto.p.username[0] == '\0' || strcmp(messRicevuto.p.username, "") == 0) {
                    printf("PARTITA TERMINATA: Nessuno collegato\n"); 
                } else {
                    printf("VINCITORE: %s\n", messRicevuto.p.username); 
                }
                break; 
            }
        
            
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
        if (mappaPlayer[i][j] != ' ' && mappaPlayer[i][j] != '\0'){
            for(int k = 0; k < NUM_PLAYERS; k++) {
                if(mappaPlayer[i][j] == players[k].lettera)
                    return players[k].colorePlayer; 
            }
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
            if(players[k].username[0] != '\0') {
                printf("Giocatore %s: %d celle conquistate\n", players[k].username, statistics[k].celleConquistate);
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
