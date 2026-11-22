#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { CONTIGUA = 1, ENLAZADA, INDEXADA };
typedef long long Entero;
typedef struct { int ocupado; } Bloque;

static Bloque *disco;
static int bloques, modo;
static Entero B;
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

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    int inicio = 1;
    if (argc != inicio && argc != inicio + 5)
        error("uso: ./simulador [disco_MB bloque_KB modo min_KB max_KB]");
    const char *preguntas[] = {"Disco (MB)", "Bloque (KB)", "Mecanismo (1 contigua, 2 enlazada, 3 indexada)",
                               "Archivo minimo (KB)", "Archivo maximo (KB)"};
    int p[5] = {0};
    for (int i = 0; i < 5; i++) p[i] = argc == inicio ? pedir(preguntas[i]) : numero(argv[inicio + i], 1);
    modo = p[2];
    if (modo > INDEXADA || p[3] > p[4]) error("revise el mecanismo y el rango de archivos.");
    Entero discoKB = (Entero)p[0] * 1024, n = discoKB / p[1];
    if (n < 1 || n > INT_MAX) error("el disco debe contener entre 1 e INT_MAX bloques completos.");
    B = (Entero)p[1] * 1024;
    bloques = (int)n;
    disco = reservar((size_t)bloques, sizeof *disco);
    printf("Modo=%s bloques=%d bloque=%lld B resto=%lld KB\n",
           nombre[modo], bloques, B, discoKB % p[1]);
    free(disco);
    return 0;
}