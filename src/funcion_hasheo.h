#ifndef FUNCION_HASHEO_H
#define FUNCION_HASHEO_H

#include <stdio.h>
#include <stdlib.h>

#define CANTIDAD 151
#define GRUPOS 23
extern unsigned long semilla[GRUPOS];

/**
 * Carga las semillas del hash perfecto de dos niveles desde
 * ./src/hash_config.bin al arreglo semilla[]. 
 *
 * @return 0 si tuvo exito
 */
int cargar_semilla();

/**
 * Calcula el slot de hash perfecto para el nombre de un pokemon.
 *
 * @param nombre nombre a hashear, se espera que ya venga en minusculas.
 * @return slot en el rango [0, CANTIDAD - 1], o -1 si nombre es NULL.
 */
int slot_de(const char *nombre);

#endif // FUNCION_HASHEO_H
