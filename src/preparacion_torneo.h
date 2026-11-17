#ifndef PREPARACION_TORNEO_H
#define PREPARACION_TORNEO_H

#include "torneo.h"
#include "equipo_automatico.h"

// Solo agrega entrenadores nuevos. Si falla, conserva el registro anterior.
// Con 32 registrados no necesita reglas ni genera equipos adicionales.
resultado_generacion_equipo completar_entrenadores(torneo *liga,
    const reglas_equipo_automatico *reglas);

#endif
