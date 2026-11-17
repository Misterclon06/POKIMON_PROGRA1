#ifndef EFECTIVIDAD_H
#define EFECTIVIDAD_H

#include "entidades.h"

#define CANTIDAD_DE_TIPOS 18

// Retornan 1 si la carga fue valida. Conservan el catalogo anterior ante errores.
// El archivo define nombres y relaciones distintas de x1; se carga una sola vez
// automaticamente al consultar, o explicitamente mediante estas funciones.
int cargar_efectividad();
int cargar_efectividad_desde(const char *ruta);

// Indice compartido del catalogo de tipos; -1 si el nombre no es valido.
int obtener_tipo(const char *nombre_del_tipo);

// Devuelve 1 si el nombre pertenece al catalogo de tipos o 0 si no es valido.
int es_tipo_pokemon_valido(const char *nombre_del_tipo);

// Calcula la efectividad del tipo de ataque elegido contra los tipos del defensor.
// El tipo elegido debe coincidir con uno de los tipos del atacante.
// Devuelve 1 si el calculo fue exitoso o 0 si los argumentos o tipos son invalidos.

int calcular_efectividad(
    const pokemon *atacante,
    const char *tipo_ataque_elegido,
    const pokemon *defensor,
    double *multiplicador);

#endif
