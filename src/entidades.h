#ifndef ENTIDADES_H
#define ENTIDADES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "pokedex.h"

#define MAX_EQUIPO 6

typedef struct {
    int id; // unico por ejemplar durante la ejecucion; la especie se consulta por nombre
    char especie[50];
    char nombre[50];
    int nivel;
    int hp;
    int hp_max;
    int ataque;
    int defensa;
    int velocidad;
    char tipo_primario[20];
    char tipo_secundario[20];
    char estado[20];
} pokemon;

typedef struct {
    int id;
    char nombre[50];
    pokemon equipo[MAX_EQUIPO]; // maximo 6 pokemon por entrenador
    int n_pokemon; // cantidad de pokemon en el equipo
    int n_victorias;
    int n_empates;
    int n_derrotas;
    int puntuacion;
} entrenador;

void mostrar_entrenador(const entrenador *ent);
void agregar_pokemon_a_entrenador(entrenador *ent, pokemon poke);
void eliminar_pokemon_de_entrenador(entrenador *ent, int indice);
void mostrar_pokemon(const pokemon *poke);

// funcion que calcula las estadisticas de un pokemon segun su semilla, nivel y stats base
void gen_stats_pokemon_por_especie(pokemon *poke, pokedata *poke_base, uint32_t seed, int nivel);
void gen_stats_pokemon(pokemon *poke, uint32_t seed, int nivel);
// Comprueba rangos posibles de la formula original, sin sortear nuevas stats.
int estadisticas_pokemon_validas(const pokemon *poke, const pokedata *base);


pokemon preguntar_crear_pokemon();

// Retorna un ejemplar cero si especie, nombre o nivel son invalidos.
pokemon crear_pokemon(const char *especie, const char *nombre, int nivel);
// Mismo constructor con semilla explicita, para repetir una simulacion sin
// generar primero unas stats que despues terminan en la basura xd.
pokemon crear_pokemon_con_semilla(const char *especie, const char *nombre,
    int nivel, uint32_t semilla);
// Tras restaurar datos de archivo, los IDs nuevos deben continuar despues del mayor.
void actualizar_ids_restaurados(int ultimo_pokemon, int ultimo_entrenador);
entrenador crear_entrenador();
entrenador crear_entrenador_con_nombre(const char *nombre);



#endif // ENTIDADES_H
