#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { CONTIGUA = 1, ENLAZADA, INDEXADA };
typedef long long Entero;
typedef struct { int ocupado; } Bloque;
typedef struct { Entero id, bytes; int datos, *ubicacion; } Archivo;
typedef struct { int libres, huecos, mayor; } Estado;

static Bloque *disco;
static Archivo *archivos;
static int bloques, modo, vivos;
static Entero B, creados;
static const char *nombre[] = {"", "contigua", "enlazada", "indexada"};

static void error(const char *mensaje) {
    fprintf(stderr, "Error: %s\n", mensaje);
    exit(EXIT_FAILURE);
}

static int numero(const char *texto, int minimo) {
    char *fin;
    errno = 0;
    long n = strtol(texto, &fin, 10);
    if (fin == texto) error("se esperaba un entero.");
    while (isspace((unsigned char)*fin)) fin++;
    if (errno || *fin || n < minimo || n > INT_MAX) error("entero fuera de rango o invalido.");
    return (int)n;
}

static int pedir(const char *pregunta) {
    char linea[80];
    printf("%s: ", pregunta);
    fflush(stdout);
    if (!fgets(linea, sizeof linea, stdin)) error("faltan datos de entrada.");
    if (!strchr(linea, '\n') && !feof(stdin)) error("entrada demasiado larga.");
    return numero(linea, 1);
}

static void *reservar(size_t n, size_t tam) {
    void *p = calloc(n, tam);
    if (!p) error("memoria RAM insuficiente para la simulacion.");
    return p;
}

static Entero techo(Entero n, Entero divisor) { return 1 + (n - 1) / divisor; }
static Entero azar(Entero n) { return (Entero)(drand48() * (double)n); }

static Estado medir(void) {
    Estado e = {0};
    int racha = 0;
    for (int i = 0; i < bloques; i++) {
        if (disco[i].ocupado) { racha = 0; continue; }
        e.libres++;
        if (racha == 0) e.huecos++;
        if (++racha > e.mayor) e.mayor = racha;
    }
    return e;
}

static void estado(Estado e) {
    double fe = e.libres ? 100.0 * (e.libres - e.mayor) / e.libres : 0;
    printf("  Disco: libres=%d huecos=%d mayor=%d FE=%.2f%%\n",
           e.libres, e.huecos, e.mayor, fe);
}

static void rangos(const int *lista, int n) {
    for (int i = 0; i < n; i++) {
        int inicio = lista[i];
        if (i) putchar(',');
        while (i + 1 < n && lista[i + 1] == lista[i] + 1) i++;
        printf("%d", inicio);
        if (lista[i] != inicio) printf("-%d", lista[i]);
    }
}

static int primer_hueco(int n) {
    int racha = 0;
    for (int i = 0; i < bloques; i++) {
        racha = disco[i].ocupado ? 0 : racha + 1;
        if (racha == n) return i - n + 1;
    }
    return -1;
}

static int crear(Entero bytes, Entero t) {
    Entero d = techo(bytes, B);
    Estado e = medir();
    const char *motivo = NULL;
    if (d > e.libres) motivo = "ESPACIO_INSUFICIENTE";
    else if (d > e.mayor) motivo = "FRAGMENTACION_EXTERNA";
    if (motivo) {
        printf("[t=%lld] FIN %s: %lld B requieren %lld bloques; libres=%d mayor=%d\n",
               t, motivo, bytes, d, e.libres, e.mayor);
        return 0;
    }

    int total = (int)(d), *lista = reservar((size_t)total, sizeof *lista);
    int b = primer_hueco(total);
    for (int i = 0; i < total; i++) {
        lista[i] = b;
        disco[b++].ocupado = 1;
    }
    Archivo a = {++creados, bytes, (int)d, lista};
    archivos[vivos++] = a;
    printf("[t=%lld] CREAR #%lld bytes=%lld datos=", t, a.id, bytes);
    rangos(lista, a.datos);
    putchar('\n');
    estado(medir());
    return 1;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    int rapido = argc >= 3 && strcmp(argv[1], "--rapido") == 0;
    long semilla = rapido ? numero(argv[2], 0) : (long)time(NULL);
    int inicio = rapido ? 3 : 1;
    if (argc != inicio && argc != inicio + 5)
        error("uso: ./simulador [--rapido SEMILLA] [disco_MB bloque_KB modo min_KB max_KB]");
    const char *preguntas[] = {"Disco (MB)", "Bloque (KB)", "Mecanismo (1 contigua, 2 enlazada, 3 indexada)",
                               "Archivo minimo (KB)", "Archivo maximo (KB)"};
    int p[5] = {0};
    for (int i = 0; i < 5; i++) p[i] = argc == inicio ? pedir(preguntas[i]) : numero(argv[inicio + i], 1);
    modo = p[2];
    if (modo > INDEXADA || p[3] > p[4]) error("revise el mecanismo y el rango de archivos.");
    if (modo != CONTIGUA) error("esta version simula solo asignacion contigua.");
    Entero discoKB = (Entero)p[0] * 1024, n = discoKB / p[1];
    if (n < 1 || n > INT_MAX) error("el disco debe contener entre 1 e INT_MAX bloques completos.");
    B = (Entero)p[1] * 1024;
    bloques = (int)n;
    disco = reservar((size_t)bloques, sizeof *disco);
    archivos = reservar((size_t)bloques, sizeof *archivos);
    srand48(semilla);
    printf("Modo=%s bloques=%d bloque=%lld B resto=%lld KB semilla=%ld reloj=%s\n",
           nombre[modo], bloques, B, discoKB % p[1], semilla, rapido ? "rapido" : "real");
    estado(medir());

    struct timespec instante;
    if (!rapido && clock_gettime(CLOCK_MONOTONIC, &instante)) error("fallo del reloj.");
    for (Entero t = 1; ; t++) {
        if (!rapido) {
            instante.tv_sec++;
            int r;
            do { r = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &instante, NULL); } while (r == EINTR);
            if (r) error("fallo de la espera.");
        }
        if (t % 2 == 0) {
            Entero kb = p[3] + azar((Entero)p[4] - p[3] + 1);
            if (!crear(kb * 1024, t)) break;
        }
    }
    for (int i = 0; i < vivos; i++) free(archivos[i].ubicacion);
    free(archivos);
    free(disco);
    return 0;
}