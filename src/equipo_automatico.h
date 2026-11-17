#ifndef EQUIPO_AUTOMATICO_H
#define EQUIPO_AUTOMATICO_H

#include "entidades.h"

typedef struct {
    int cantidad_pokemon;
    int nivel_maximo_individual;
    int nivel_comun;
    int tipos_distintos_minimos;
} reglas_equipo_automatico;

typedef enum {
    GENERACION_EQUIPO_OK,
    GENERACION_EQUIPO_ARGUMENTO_INVALIDO,
    GENERACION_EQUIPO_DATOS_NO_DISPONIBLES,
    GENERACION_EQUIPO_SIN_SOLUCION,
    GENERACION_EQUIPO_LIMITE_BUSQUEDA
} resultado_generacion_equipo;

// nivel_comun == 0 asigna un nivel aleatorio individual a cada Pokemon.
// Si es mayor que cero, todos usan ese nivel; se puede reutilizar la misma
// regla al generar varios equipos para mantenerlos al mismo nivel.
// Genera un equipo usando la Pokedex ya cargada y las reglas recibidas.
// Las especies no se repiten y se eligen al azar dentro de las reglas.
// El equipo se actualiza solo si se encuentra una solucion.
resultado_generacion_equipo generar_equipo_automatico(
    const reglas_equipo_automatico *reglas,
    entrenador *destino,
    unsigned long *nodos_visitados);

#endif
