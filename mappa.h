
#define N 20
#define MURO (char)254

typedef enum {
    RESET_COLOR,
    BIANCO,
    BLACK,
    GRIGIO,
    ROSSO,
    VERDE,
    GIALLO,
    BLU,
    MAGENTA,
    CIANO,
    ROSSO_CHIARO,
    VERDE_CHIARO,
    GIALLO_CHIARO,
    BLU_CHIARO,
    MAGENTA_CHIARO,
    CIANO_CHIARO
} Colore;

const char *colori[] = {
    "\x1b[0m",    // RESET
    "\x1b[47m",   // BIANCO background
    "\x1b[40m",   // BLACK background
    "\x1b[100m",  // GRIGIO background
    "\x1b[41m",   // ROSSO background
    "\x1b[42m",   // VERDE background
    "\x1b[43m",   // GIALLO background
    "\x1b[44m",   // BLU background
    "\x1b[45m",   // MAGENTA background
    "\x1b[46m",   // CIANO background

    "\x1b[101m",  // ROSSO CHIARO background
    "\x1b[102m",  // VERDE CHIARO background
    "\x1b[103m",  // GIALLO CHIARO background
    "\x1b[104m",  // BLU CHIARO background
    "\x1b[105m",  // MAGENTA CHIARO background
    "\x1b[106m"   // CIANO CHIARO background
};

typedef struct player {
    char lettera;
    int riga;
    int colonna;
    Colore colorePlayer;
} Player;

typedef struct mappa {
    char mappa[N][N];
    char mappaPlayer[N][N];
} Mappa;

Colore getColoreCasella(Player p, int i, int j, char mappaPlayer[N][N], char mappa[N][N]);

void rivelaNebbia(Player p, char mappa[N][N], char mappaGlobale[N][N]);

bool verificaMossa(int riga, int colonna, char mappa[N][N]);

void stampaMappa(Player p, char mappa[N][N], char mappaPlayer[N][N]);