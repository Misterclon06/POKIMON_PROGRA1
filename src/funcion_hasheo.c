#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "funcion_hasheo.h"

unsigned long semilla[GRUPOS];

int cargar_semilla() {
    int resultado;
    FILE *f = fopen("./src/hash_config.bin", "rb");

    if (!f) {
        fprintf(stderr, "No se pudo abrir hash_config.bin\n");
        resultado = 1;
    } else {
        int g_leido;

        if (fread(&g_leido, sizeof(int), 1, f) != 1) {
            fprintf(stderr, "Error leyendo el numero de grupos\n");
            resultado = 2;
        } else if (g_leido != GRUPOS) {
            fprintf(stderr, "Numero de grupos incorrecto: %d (esperado %d)\n",
                    g_leido, GRUPOS);
            resultado = 3;
        } else if (fread(semilla, sizeof(unsigned long), GRUPOS, f) != GRUPOS) {
            fprintf(stderr, "Error leyendo las semillas\n");
            resultado = 4;
        } else {
            resultado = 0;
        }

        fclose(f);
    }

    return resultado;
}

// divide en grupos y luego aplica h2 con la semilla del grupo correspondiente
static int grupo(const char *s) {
    unsigned long h = 0;

    for (int i = 0; s[i]; i++){
        h = h * 31UL + (unsigned char)s[i]; // el hash de java segun la documentacion que lei ngl
    }

    return (int)(h % GRUPOS); // devuelve el grupo en [0, GRUPOS-1]
}

static int h2(const char *s, unsigned long seed) {
    unsigned long h = seed;
    for (int i = 0; s[i]; i++)
        h = h * 131UL + (unsigned char)s[i]; // 131 es el multiplicador de MurmurHash, el chiste es q sea un primo grande 
    h ^= (h >> 33);                         // y que no sea 31, pq ya lo usamos en el primer nivel.
    h *= 0xff51afd7ed558ccdUL;              //Esto es inspirado en MurmurHash3 y en el "finalizer" de splitmix64. 
    h ^= (h >> 33);                         //El objetivo es avalancha: un cambio de 1 bit en la entrada
    return (int)(h % CANTIDAD);             // debe cambiar ~50% de los bits de la salida. 
}

int slot_de(const char *nombre) {
    int resultado;

    if (nombre == NULL) {
        resultado = -1;
    } else {
        
        resultado = h2(nombre, semilla[grupo(nombre)]); // aqui se ve como se calcula el grupo, luego se busca la semilla a ese grupo, 
                                                        //  y luego se calcula el indice con h2.
    }

    return resultado;
}
