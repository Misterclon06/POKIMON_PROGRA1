#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "entrada.h"
#include "efectividad.h"
#ifndef _WIN32
#include <strings.h>
#endif
#include "pokedex.h"
#include "funcion_hasheo.h"
#include "utils.h"

// Si estás en Windows, define strcasecmp como _stricmp para compatibilidad, por que resulta que cambia si usas linux, odio aqui
#ifdef _WIN32
#define strcasecmp _stricmp
#endif

// ruta pokedex data
static const char *ruta_pokedex = "./DB/POKEDEX_DB.txt";
static pokedata pokedex[CANTIDAD]; // arreglo de structs para almacenar los datos de los 151 pokemones
static int id_a_slot[CANTIDAD]; // traduce id  -> slot en pokedex[] (para buscar por id de manera "eficiente")

// si, uso dos arreglos separados en vez de un arreglo de punteros, 
// pq es mas simple y no hay necesidad de malloc/free, ademas de que es mas cache friendly.
// lo nombro pq lo pense no pq fuera buena idea xdd.


// Solo publicamos un catalogo completo. Media Pokedex no es una Pokedex,
// es un album al que le arrancaron paginas xd.
static int pokedex_cargada = 0;

// Exige los ocho campos de una especie, sin sobrantes, numeros fuera de rango
// ni tipos inventados. Un sscanf exitoso por si solo no valida los enteros.
static int interpretar_especie(const char *linea, pokedata *especie) {
    char numeros[5][100];
    char sobrante;
    int valido = sscanf(linea, "%99s %49s %19s %19s %99s %99s %99s %99s %c",
        numeros[0], especie->nombre, especie->tipo_primario,
        especie->tipo_secundario, numeros[1], numeros[2], numeros[3],
        numeros[4], &sobrante) == 8;
    if (valido) {
        valido = convertir_entero(numeros[0], 1, CANTIDAD, &especie->id) &&
            convertir_entero(numeros[1], 1, INT_MAX, &especie->hp) &&
            convertir_entero(numeros[2], 1, INT_MAX, &especie->ataque) &&
            convertir_entero(numeros[3], 1, INT_MAX, &especie->defensa) &&
            convertir_entero(numeros[4], 1, INT_MAX, &especie->velocidad) &&
            es_tipo_pokemon_valido(especie->tipo_primario) &&
            (strcmp(especie->tipo_secundario, "-") == 0 ||
             es_tipo_pokemon_valido(especie->tipo_secundario));
    }
    return valido;
}

// Exige el catalogo completo, IDs unicos y slots sin colisiones con las semillas
// guardadas. Si una recarga falla, conserva tanto los datos como el indice anterior.
// Ojo al contrato heredado: aqui 0 significa exito, no 1 como en otros cargadores.
int cargar_pokedex_desde(const char *ruta) {
    int error = 1;
    // Primero validar en arreglos temporales; si falla, el catalogo anterior vive.
    pokedata temporal[CANTIDAD] = {0};
    int posiciones[CANTIDAD];
    for (int i = 0; i < CANTIDAD; i++) {
        posiciones[i] = -1;
    }
    // Las semillas solo se cargan antes de tener un catalogo publicado: cambiar
    // el hash debajo de los datos seria mover la casa y dejar la direccion xd.
    if (ruta != NULL && (pokedex_cargada || cargar_semilla() == 0)) {
        FILE *archivo = fopen(ruta, "r");
        if (archivo != NULL) {
            char linea[512];
            int cantidad = 0;
            int valido = 1;
            int lectura = 0;
            while (valido && (lectura = leer_linea(archivo, linea, sizeof(linea))) == 1) {
                pokedata especie = {0};
                valido = interpretar_especie(linea, &especie);
                if (valido) {
                    char *nombre = tolower_string(especie.nombre);
                    valido = nombre != NULL;
                    if (valido) {
                        int slot = slot_de(nombre);
                        valido = slot >= 0 && slot < CANTIDAD &&
                            posiciones[especie.id - 1] == -1 &&
                            temporal[slot].id == 0;
                        if (valido) {
                            temporal[slot] = especie;
                            posiciones[especie.id - 1] = slot;
                            cantidad++;
                        }
                    }
                    free(nombre);
                }
            }
            valido = valido && lectura == 0 && !ferror(archivo) && cantidad == CANTIDAD;
            fclose(archivo);
            if (valido) {
                memcpy(pokedex, temporal, sizeof(pokedex));
                memcpy(id_a_slot, posiciones, sizeof(id_a_slot));
                pokedex_cargada = 1;
                error = 0;
                printf("Pokedex cargada correctamente (%d registros)\n", cantidad);
            } else {
                fprintf(stderr, "Pokedex invalida: se requieren %d especies unicas compatibles con el hash.\n", CANTIDAD);
            }
        } else {
            fprintf(stderr, "No se pudo abrir la Pokedex: %s\n", ruta);
        }
    }
    return error;
}

int cargar_pokedex() {
    return cargar_pokedex_desde(ruta_pokedex);
}

void mostrar_pokedata(const pokedata *poke) {
    if (poke != NULL) {
        printf("ID: %d\n", poke->id);
        printf("Nombre: %s\n", poke->nombre);
        printf("Tipo Primario: %s\n", poke->tipo_primario);
        printf("Tipo Secundario: %s\n", poke->tipo_secundario);
        printf("HP: %d\n", poke->hp);
        printf("Ataque: %d\n", poke->ataque);
        printf("Defensa: %d\n", poke->defensa);
        printf("Velocidad: %d\n", poke->velocidad);
    }
}

void mostrar_pokedex() {
    for (int i = 0; i < CANTIDAD; i++) {
        pokedata *poke = buscar_id(i + 1);
        if (poke != NULL) {
            mostrar_pokedata(poke);
            printf("\n");
        }
    }
}

pokedata *buscar_nombre(const char *nombre) {
    pokedata *resultado = NULL;

    if (pokedex_cargada && nombre != NULL) {
        // El hash se construyo sobre nombres en minuscula
        // y bueno, no puede variar pq empieza a fallar xd
        char *nombre_lower = tolower_string(nombre);
        if (nombre_lower != NULL) {
            int idx = slot_de(nombre_lower);
            free(nombre_lower);
            // El hash es perfecto para las 151 especies conocidas, no para cualquier
            // texto. Confirmamos el nombre para no devolver un vecino por accidente xd.
            if (idx >= 0 && idx < CANTIDAD && strcasecmp(pokedex[idx].nombre, nombre) == 0) {
                resultado = &pokedex[idx];
            }
        }
    }

    return resultado;
}

pokedata *buscar_id(int id) {
    pokedata *resultado = NULL;

    if (pokedex_cargada && id >= 1 && id <= CANTIDAD) {
        resultado = &pokedex[id_a_slot[id - 1]];
    }

    return resultado;
}
