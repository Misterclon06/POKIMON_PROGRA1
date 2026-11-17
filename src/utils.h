#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdint.h>

// archivo donde pongo funciones random la vrd xddd

#ifndef utils_h
#define utils_h

// hace una copia de la cadena y la convierte a minusculas
static inline char* tolower_string(const char *str) {
    char *new_str = NULL;

    if (str != NULL) {
        new_str = malloc(strlen(str) + 1);
        if (new_str != NULL) {
            strcpy(new_str, str);
            for (int i = 0; new_str[i]; i++) {
                new_str[i] = (char)tolower((unsigned char)new_str[i]);
            }
        }
    }

    return new_str;
}

// devuelve el tiempo actual en segundos como un double
static inline double ahora() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// alterador de semilla con xorshift de Marsaglia
static inline uint32_t alterador_semilla(uint32_t semilla) {
    semilla ^= semilla << 13;
    semilla ^= semilla >> 17;
    semilla ^= semilla << 5;
    return semilla;
}

// numero aleatorio con el tiempo actual y ciclos de CPU consumidos
static inline uint32_t gen_semilla(){

    static uint32_t contador = 0; // cambia en cada llamada, asi dos semillas pedidas en el mismo segundo no se repiten
    struct timespec ts;
    uint32_t semilla;

    do
    {
        contador += 0x9E3779B9u; // constante de la razon aurea, recorre los 32 bits de forma uniforme
        clock_gettime(CLOCK_MONOTONIC, &ts); // nanosegundos, varian aunque time(NULL) no haya cambiado

        semilla = (uint32_t)time(NULL) ^ (uint32_t)clock() ^ (uint32_t)ts.tv_nsec ^ contador;
        semilla = alterador_semilla(semilla); // mezcla los bits para que semillas cercanas den resultados distintos
    } while (semilla == 0);

    return semilla;
}


#endif // utils_h