#ifndef TORNEO_H
#define TORNEO_H
#include "entidades.h"
#include "../LPC/colas/colas.h"

#define ENTRENADORES_TORNEO 32
#define GRUPOS_TORNEO 8
#define INTEGRANTES_GRUPO 4
#define COMBATES_GRUPOS 48
#define COMBATES_TORNEO 64

typedef struct {
    int participantes[2]; // IDs, nunca posiciones de arreglo
    int resuelto;
    int ganador; // ID; cero significa empate en grupos
    int derrotados[2]; // ejemplares perdidos por cada participante
} encuentro;

typedef struct {
    int puntos, victorias, empates, derrotas;
    int rivales_derrotados, propios_derrotados;
} estadistica_grupo;

typedef struct {
    entrenador entrenadores[ENTRENADORES_TORNEO];
    int cantidad;
    int iniciado;
    encuentro encuentros[COMBATES_TORNEO];
    char fuentes[COMBATES_TORNEO - COMBATES_GRUPOS][2][4];
    estadistica_grupo estadisticas[ENTRENADORES_TORNEO];
    int posiciones[GRUPOS_TORNEO][INTEGRANTES_GRUPO];
    int grupo_clasificado[GRUPOS_TORNEO];
    cola pendientes; // numeros de encuentros ya habilitados y sin resultado
} torneo;

void torneo_crear(torneo *liga);
void torneo_destruir(torneo *liga);
int torneo_indice_entrenador(const torneo *liga, int id);
int torneo_registrar(torneo *liga, const entrenador *nuevo);
int torneo_iniciar(torneo *liga, const char *ruta_cruces);
int torneo_actualizar(torneo *liga);
int torneo_registrar_resultado(torneo *liga, int numero, int primero, int segundo,
    int ganador, int derrotados_primero, int derrotados_segundo);
int torneo_guardar_resultados(const torneo *liga, const char *ruta);
int torneo_cargar_resultados(torneo *liga, const char *ruta);
int torneo_guardar_clasificacion(const torneo *liga, const char *ruta);
int torneo_guardar_entrenadores(const torneo *liga, const char *ruta);
// Importa sobre un registro vacio, antes de iniciar; rechaza el archivo completo si falla.
int torneo_cargar_entrenadores(torneo *liga, const char *ruta);
// Guarda o restaura una sesion completa en un archivo versionado.
int torneo_guardar_partida(const torneo *liga, int jugador, const char *ruta);
int torneo_cargar_partida(torneo *liga, int *jugador, const char *ruta,
    const char *ruta_cruces);
// Devuelve 1 solo cuando se han resuelto tercer puesto y final.
int torneo_podio(const torneo *liga, int ids[4]);
#endif
