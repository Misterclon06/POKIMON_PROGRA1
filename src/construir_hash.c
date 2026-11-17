/*
 *
 * Construye una funcion de hashing perfecto minimal (MPHF) de dos niveles
 * sobre un conjunto fijo de N nombres leidos de un archivo .txt.
 *
 *
 * Salida:
 *   - hash_config.bin : archivo binario con G y las semillas (una por grupo)
 *
 * Uso:
 *   ./construir nombres.txt
 *
 * Formato del .txt:
 *   Un nombre por linea, exactamente N lineas.
 *
 * Compilacion:
 *   gcc -O2 -Wall -Wextra -o construir construir.c
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* Configuracion                                                       */
/* ------------------------------------------------------------------ */

#define N          151          /* numero exacto de pokemones */
#define G          23           /* numero de grupos nivel 1 */
#define MAX_NOMBRE 64           /* longitud maxima de cada nombre */
#define MAX_INTENTOS 5000000L   /* intentos maximos por grupo */

/* ------------------------------------------------------------------ */
/* Datos globales                                                      */
/* ------------------------------------------------------------------ */

static char nombres[N][MAX_NOMBRE];
static unsigned long semilla[G];

/* ------------------------------------------------------------------ */
/* Utilidades                                                          */
/* ------------------------------------------------------------------ */

/* Genera un entero de 64 bits pseudoaleatorio combinando rand() */
static unsigned long rnd64() {
    return ((unsigned long)rand() << 33)
         ^ ((unsigned long)rand() << 17)
         ^  (unsigned long)rand()
         ^  ((unsigned long)rand() << 49);
}

/* ------------------------------------------------------------------ */
/* Carga del archivo                                                   */
/* ------------------------------------------------------------------ */

static int cargar_nombres(const char *ruta) {
    FILE *fp = fopen(ruta, "r");
    int valido = fp != NULL;
    int cantidad = 0;
    if (valido) {
        char linea[MAX_NOMBRE + 2];
        while (valido && fgets(linea, sizeof(linea), fp) != NULL) {
            valido = strchr(linea, '\n') != NULL || feof(fp);
            linea[strcspn(linea, "\r\n")] = '\0';
            char *nombre = linea;
            while (*nombre == ' ' || *nombre == '\t') nombre++;
            int longitud = (int)strlen(nombre);
            while (longitud > 0 && (nombre[longitud - 1] == ' ' || nombre[longitud - 1] == '\t')) nombre[--longitud] = '\0';
            if (longitud > 0) {
                valido = valido && cantidad < N && longitud < MAX_NOMBRE;
                if (valido) {
                    strcpy(nombres[cantidad], nombre);
                    cantidad++;
                }
            }
        }
        valido = valido && !ferror(fp) && cantidad == N;
        fclose(fp);
    }
    if (!valido) fprintf(stderr, "Se requieren %d nombres validos, uno por linea.\n", N);
    return valido;
}

/* ------------------------------------------------------------------ */
/* Funciones hash                                                      */
/* ------------------------------------------------------------------ */

/* Nivel 1: nombre -> grupo en [0, G-1] */
static int grupo(const char *s) {
    unsigned long h = 0;
    for (int i = 0; s[i]; i++) {
        h = h * 31UL + (unsigned char)s[i];
    }
    return (int)(h % G);
}

/* Nivel 2: (nombre, semilla) -> slot en [0, N-1] */
static int h2(const char *s, unsigned long seed) {
    unsigned long h = seed;
    for (int i = 0; s[i]; i++) {
        h = h * 131UL + (unsigned char)s[i];
    }
    /* Mezcla final para evitar patrones */
    h ^= (h >> 33);
    h *= 0xff51afd7ed558ccdUL;
    h ^= (h >> 33);
    return (int)(h % N);
}

/* ------------------------------------------------------------------ */
/* Construccion del MPHF                                               */
/* ------------------------------------------------------------------ */

// Busca una semilla por grupo que ubique todos sus nombres en slots libres.
// Los grupos grandes van primero porque son mas dificiles de acomodar; cada
// intento se reserva solo si no choca dentro del grupo ni con los anteriores.
// El limite evita buscar eternamente y la verificacion final exige slots unicos.
static int construir() {
    int valido = 1;
    /* 1. Agrupar los nombres */
    int miembros[G][N];
    int tam[G];
    for (int g = 0; g < G; g++) tam[g] = 0;

    for (int i = 0; i < N; i++) {
        int g = grupo(nombres[i]);
        miembros[g][tam[g]++] = i;
    }

    /* 2. Ordenar grupos por tamaño descendente */
    int orden[G];
    for (int i = 0; i < G; i++) orden[i] = i;
    for (int i = 0; i < G - 1; i++) {
        for (int j = i + 1; j < G; j++) {
            if (tam[orden[j]] > tam[orden[i]]) {
                int t = orden[i];
                orden[i] = orden[j];
                orden[j] = t;
            }
        }
    }

    /* Diagnostico: tamaño de grupos */
    printf("Distribucion de grupos (antes de ordenar):\n");
    for (int g = 0; g < G; g++) {
        printf("  grupo %2d: %d elementos\n", g, tam[g]);
    }
    printf("\n");

    /* 3. Buscar semilla para cada grupo, en orden descendente de tamaño */
    unsigned char ocupado[N];
    memset(ocupado, 0, sizeof(ocupado));

    for (int oi = 0; valido && oi < G; oi++) {
        int g = orden[oi];
        int k = tam[g];

        if (k == 0) {
            semilla[g] = 0;
            printf("Grupo %2d: vacio\n", g);
        } else {

        int encontrado = 0;
        long intento;

        for (intento = 0; !encontrado && intento < MAX_INTENTOS; intento++) {
            unsigned long s = rnd64() | 1UL;  /* impar, evita semilla 0 */

            int tmp[N];
            int ok = 1;

            for (int m = 0; ok && m < k; m++) {
                int idx = h2(nombres[miembros[g][m]], s);

                /* Colision con slot ya ocupado por otro grupo */
                if (ocupado[idx]) ok = 0;

                /* Colision interna dentro del mismo grupo */
                int dup = 0;
                for (int j = 0; !dup && j < m; j++) {
                    if (tmp[j] == idx) dup = 1;
                }
                if (dup) ok = 0;

                tmp[m] = idx;
            }

            if (ok) {
                semilla[g] = s;
                for (int m = 0; m < k; m++) {
                    int idx = h2(nombres[miembros[g][m]], s);
                    ocupado[idx] = 1;
                }
                encontrado = 1;
                printf("Grupo %2d (tam=%d): semilla=0x%016lx  (%ld intentos)\n",
                       g, k, s, intento + 1);
            }
        }

        if (!encontrado) {
            fprintf(stderr,
                    "FALLO: no se encontro semilla para el grupo %d (tam=%d) "
                    "tras %ld intentos\n", g, k, MAX_INTENTOS);
            valido = 0;
        }
        }
    }

    /* 4. Verificar biyeccion */
    unsigned char check[N];
    memset(check, 0, sizeof(check));

    for (int i = 0; valido && i < N; i++) {
        int g = grupo(nombres[i]);
        int idx = h2(nombres[i], semilla[g]);

        if (check[idx]) {
            fprintf(stderr,
                    "FALLO: colision en slot %d entre '%s' y otro nombre\n",
                    idx, nombres[i]);
            valido = 0;
        }
        check[idx] = 1;
    }

    if (valido) printf("\nVerificacion: %d/%d slots unicos. OK.\n", N, N);
    return valido;
}

/* ------------------------------------------------------------------ */
/* Guardar configuracion                                               */
/* ------------------------------------------------------------------ */

// Escribe cantidad de grupos y semillas en el formato que lee cargar_semilla.
// El binario usa tamaños nativos de int/unsigned long: se genera para la plataforma
// donde se va a consumir, no es un formato de intercambio universal.
static int guardar_config(const char *ruta) {
    FILE *fp = fopen(ruta, "wb");
    int valido = fp != NULL;
    if (valido) {
        int grupos = G;
        valido = fwrite(&grupos, sizeof(int), 1, fp) == 1 &&
            fwrite(semilla, sizeof(unsigned long), G, fp) == G;
        valido = fclose(fp) == 0 && valido;
    }
    if (valido) printf("Configuracion guardada en '%s' (%d semillas)\n", ruta, G);
    else fprintf(stderr, "No se pudo guardar la configuracion.\n");
    return valido;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    int valido = argc == 2;
    if (valido) {
        srand((unsigned)time(NULL) ^ 0xC0FFEEu);
        printf("=== Construccion de hash perfecto: N=%d, G=%d ===\n", N, G);
        valido = cargar_nombres(argv[1]);
        if (valido) valido = construir();
        if (valido) valido = guardar_config("hash_config.bin");
    } else fprintf(stderr, "Uso: %s <archivo_nombres_minusculas.txt>\n", argv[0]);
    return valido ? EXIT_SUCCESS : EXIT_FAILURE;
}
