#include <stdio.h>
#include "preparacion_torneo.h"

resultado_generacion_equipo completar_entrenadores(torneo *liga,
    const reglas_equipo_automatico *reglas) {
    resultado_generacion_equipo resultado = GENERACION_EQUIPO_ARGUMENTO_INVALIDO;
    int valido = liga != NULL;
    if (valido) valido = !liga->iniciado && liga->cantidad >= 0 &&
        liga->cantidad <= ENTRENADORES_TORNEO;
    if (valido) {
        int cantidad_original = liga->cantidad;
        int ultimo_entrenador = 0, ultimo_pokemon = 0;
        for (int i = 0; i < liga->cantidad; i++) {
            const entrenador *ent = &liga->entrenadores[i];
            if (ent->id > ultimo_entrenador) ultimo_entrenador = ent->id;
            if (ent->n_pokemon < 1 || ent->n_pokemon > MAX_EQUIPO) valido = 0;
            for (int j = 0; valido && j < ent->n_pokemon; j++) {
                if (ent->equipo[j].id > ultimo_pokemon) ultimo_pokemon = ent->equipo[j].id;
            }
        }
        if (valido) {
            actualizar_ids_restaurados(ultimo_pokemon, ultimo_entrenador);
            entrenador nuevos[ENTRENADORES_TORNEO];
            int faltan = ENTRENADORES_TORNEO - cantidad_original;
            resultado = GENERACION_EQUIPO_OK;
            for (int i = 0; resultado == GENERACION_EQUIPO_OK && i < faltan; i++) {
                nuevos[i] = crear_entrenador_con_nombre("Entrenador automatico");
                if (nuevos[i].id == 0) resultado = GENERACION_EQUIPO_ARGUMENTO_INVALIDO;
                else {
                    snprintf(nuevos[i].nombre, sizeof(nuevos[i].nombre),
                        "Entrenador automatico %d", nuevos[i].id);
                    resultado = generar_equipo_automatico(reglas, &nuevos[i], NULL);
                }
            }
            // Primero se arman todos. Nadie entra con medio equipo porque Oak
            // se quedo sin presupuesto a mitad del registro xd.
            for (int i = 0; resultado == GENERACION_EQUIPO_OK && i < faltan; i++) {
                if (!torneo_registrar(liga, &nuevos[i])) resultado = GENERACION_EQUIPO_ARGUMENTO_INVALIDO;
            }
            if (resultado != GENERACION_EQUIPO_OK) {
                memset(&liga->entrenadores[cantidad_original], 0,
                    (size_t)(liga->cantidad - cantidad_original) * sizeof(entrenador));
                liga->cantidad = cantidad_original;
            }
        }
    }
    return resultado;
}
