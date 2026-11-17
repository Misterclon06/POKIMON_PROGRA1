#ifndef ENTRADA_H
#define ENTRADA_H

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 1: linea completa, 0: fin/error, -1: demasiado larga. Consume el sobrante.
static inline int leer_linea(FILE *archivo, char *destino, int capacidad) {
    int estado = 0;
    if (archivo != NULL && destino != NULL && capacidad >= 2 && fgets(destino, capacidad, archivo) != NULL) {
        estado = 1;
        if (strchr(destino, '\n') == NULL && !feof(archivo)) {
            int c = fgetc(archivo);
            if (c == '\r') c = fgetc(archivo);
            if (c != '\n' && c != EOF) {
                estado = -1;
                while (c != '\n' && c != EOF) c = fgetc(archivo);
            }
        }
        destino[strcspn(destino, "\r\n")] = '\0';
    }
    return estado;
}

// Una linea por dato: mezclar scanf y fgets termina invocando fantasmas xd.
static inline int leer_texto(const char *mensaje, char *destino, int capacidad) {
    int valido = 0;
    int terminado = mensaje == NULL || destino == NULL || capacidad < 2;
    while (!valido && !terminado) {
        printf("%s", mensaje);
        fflush(stdout);
        int estado = leer_linea(stdin, destino, capacidad);
        if (estado == 0) terminado = 1;
        else if (estado == -1) puts("Texto demasiado largo.");
        else {
            int i = 0;
            while (isspace((unsigned char)destino[i])) i++;
            valido = destino[i] != '\0';
            if (!valido) puts("Escribe al menos un caracter.");
        }
    }
    return valido;
}

// Convierte texto ya leido: rechaza desbordamientos, texto sobrante y valores fuera
// del rango. strtol permite distinguir un cero valido de un "cero porque falle" xd.
// Solo escribe resultado al aceptar; no lee del teclado ni consume archivos.
static inline int convertir_entero(const char *texto, int minimo, int maximo, int *resultado) {
    int valido = 0;
    if (texto != NULL && resultado != NULL && minimo <= maximo) {
        char *fin;
        long valor;
        errno = 0;
        valor = strtol(texto, &fin, 10);
        int tiene_numero = fin != texto;
        while (isspace((unsigned char)*fin)) {
            fin++;
        }
        if (tiene_numero && *fin == '\0' && errno != ERANGE &&
            valor >= minimo && valor <= maximo) {
            *resultado = (int)valor;
            valido = 1;
        }
    }
    return valido;
}

// Reintenta entradas invalidas, pero termina con 0 al agotarse el teclado/archivo.
// Descarta el resto de una linea larga para no convertirlo en la siguiente opcion.
static inline int leer_opcion(const char *mensaje, int minimo, int maximo, int *opcion) {
    char linea[100];
    int valido = 0;
    int fin_entrada = mensaje == NULL || opcion == NULL || minimo > maximo;
    while (!valido && !fin_entrada) {
        printf("%s", mensaje);
        fflush(stdout);
        if (fgets(linea, sizeof(linea), stdin) == NULL) {
            fprintf(stderr, "\nError: no se pudo leer la opcion.\n");
            fin_entrada = 1;
        } else if (strchr(linea, '\n') == NULL && !feof(stdin)) {
            int caracter;
            while ((caracter = getchar()) != '\n' && caracter != EOF) {}
            puts("Opcion demasiado larga.");
        } else {
            valido = convertir_entero(linea, minimo, maximo, opcion);
            if (!valido) {
                printf("Opcion invalida. Elige un numero entre %d y %d.\n", minimo, maximo);
            }
        }
    }
    return valido;
}

// Enter es una entrada valida aqui, no un numero faltante. EOF y 0 detienen
// el recorrido; no dejamos que el resto de una linea larga avance otra ronda.
static inline int esperar_enter(const char *mensaje) {
    char linea[80];
    int avanzar = 0, terminado = mensaje == NULL;
    while (!avanzar && !terminado) {
        printf("%s", mensaje);
        fflush(stdout);
        int estado = leer_linea(stdin, linea, sizeof(linea));
        if (estado == 0) terminado = 1;
        else if (estado == -1) puts("Entrada demasiado larga. Pulsa Enter o escribe 0 para volver.");
        else {
            int i = 0, cero;
            while (isspace((unsigned char)linea[i])) i++;
            avanzar = linea[i] == '\0';
            if (!avanzar) {
                terminado = convertir_entero(linea, 0, 0, &cero);
                if (!terminado) puts("Pulsa Enter para avanzar o escribe 0 para volver.");
            }
        }
    }
    return avanzar;
}

#endif
