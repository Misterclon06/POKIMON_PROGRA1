#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "utils.h"
#include "entidades.h"
#include "pokedex.h"
#include "entrada.h"

// para calcular estadisticas de pokemon nos vamos a basar en la formula original de la  primera generacion
// las variables que haran "variable" (xd) los pokemones seran el NV (determinado por el usuario), los ivs
// (determinado entre 1-15) y los stats de esfuerzo que van desde 0 hasta 65535 que es lo mismo que el nro
// maximo que representan 16bits (caculado de forma pseudo aleatoria con xorshift y valor time(NULL) y clock())

static int ultimo_id_pokemon = 0; // identidad del ejemplar, no su numero de especie xd
static int n_entrenadores = 0; // cantidad de entrenadores creados

// Al cargar ejemplares guardados, adelanta ambos contadores sin hacerlos retroceder.
// Asi crear uno nuevo no reutiliza el ID de alguien que ya existe.
void actualizar_ids_restaurados(int ultimo_pokemon, int ultimo_entrenador) {
    if (ultimo_pokemon > ultimo_id_pokemon) ultimo_id_pokemon = ultimo_pokemon;
    if (ultimo_entrenador > n_entrenadores) n_entrenadores = ultimo_entrenador;
}

entrenador crear_entrenador_con_nombre(const char *nombre) {
    entrenador ent = {0};
    int valido = nombre != NULL && n_entrenadores < INT_MAX;
    if (valido) {
        size_t longitud = strlen(nombre);
        valido = longitud > 0 && longitud < sizeof(ent.nombre);
        int i = 0;
        while (valido && isspace((unsigned char)nombre[i])) i++;
        valido = valido && nombre[i] != '\0';
    }
    if (valido) {
        ent.id = ++n_entrenadores;
        strcpy(ent.nombre, nombre);
    }
    return ent;
}

entrenador crear_entrenador() {
    entrenador ent = {0};
    char nombre[50];
    int genero;
    if (n_entrenadores == INT_MAX) puts("No quedan identificadores de entrenador disponibles.");
    int valido = n_entrenadores < INT_MAX &&
        leer_opcion("Dime. ¿Chico (1), chica (2), buene cede quien (0)? ", 0, 2, &genero);
    if (valido) valido = leer_texto("¿Como te llamas? ", nombre, sizeof(nombre));
    if (valido) {
        ent = crear_entrenador_con_nombre(nombre);
        printf("¡%s! Tu propia leyenda Pokemon esta a punto de comenzar.\n", ent.nombre);
        puts("Te presentaria a mi nieto, pero... no esta por aqui. EN FIN!!!");
    }
    return ent;
}

void agregar_pokemon_a_entrenador(entrenador *ent, pokemon poke) {
    if (ent == NULL || ent->n_pokemon < 0 || ent->n_pokemon > MAX_EQUIPO) {
        printf("Entrenador invalido, no se puede agregar el Pokémon.\n");
    } else if (ent->n_pokemon < MAX_EQUIPO) {
        ent->equipo[ent->n_pokemon] = poke;
        ent->n_pokemon++;
    } else {
        printf("No se puede agregar más Pokémon al equipo de %s.\n", ent->nombre);
    }
}

void eliminar_pokemon_de_entrenador(entrenador *ent, int indice) {
    if (ent == NULL || ent->n_pokemon < 0 || ent->n_pokemon > MAX_EQUIPO) {
        printf("Entrenador invalido, no se puede eliminar el Pokémon.\n");
    } else if (indice >= 0 && indice < ent->n_pokemon) {
        for (int i = indice; i < ent->n_pokemon - 1; i++) {
            ent->equipo[i] = ent->equipo[i + 1];
        }
        ent->n_pokemon--;
    } else {
        printf("Indice invalido para eliminar pokemon del equipo de %s.\n", ent->nombre);
    }
}

void mostrar_entrenador(const entrenador *ent) {
    if (ent != NULL) {
        printf("\n============================================================\n");
        printf(" ENTRENADOR: %s (ID %d)\n", ent->nombre, ent->id);
        printf(" Equipo: %d/%d | Victorias: %d | Empates: %d | Derrotas: %d\n",
            ent->n_pokemon, MAX_EQUIPO, ent->n_victorias, ent->n_empates, ent->n_derrotas);
        printf(" Puntuacion de grupos: %d\n", ent->puntuacion);
        puts("------------------------------------------------------------");
        if (ent->n_pokemon == 0) puts(" Equipo pendiente. Todavia no tiene Pokemon.");
        else {
            printf(" %-3s %5s %7s %5s %5s %5s\n",
                "Nro", "Nivel", "HP max", "Atk", "Def", "Vel");
            for (int i = 0; i < ent->n_pokemon; i++) {
                const pokemon *p = &ent->equipo[i];
                printf(" %-3d %5d %7d %5d %5d %5d\n",
                    i + 1, p->nivel, p->hp_max, p->ataque, p->defensa, p->velocidad);
                printf("     Apodo: %s\n     Especie: %s\n", p->nombre, p->especie);
            }
        }
        puts("============================================================\n");
    }
}

pokemon crear_pokemon(const char *especie, const char *nombre, int nivel){
    return crear_pokemon_con_semilla(especie, nombre, nivel, gen_semilla());
}

// La semilla explicita repite las stats, incluso si vale cero; el ID sigue siendo
// nuevo. Esto deja comparar simulaciones sin volver a tirar los dados a escondidas xd.
// Si falla especie, nombre o nivel devuelve un ejemplar vacio (id = 0).
pokemon crear_pokemon_con_semilla(const char *especie, const char *nombre,
    int nivel, uint32_t semilla) {
    pokemon poke = {0};
    pokedata *poke_base = buscar_nombre(especie);

    if (poke_base != NULL && nombre != NULL && nombre[0] != '\0' &&
        strlen(nombre) < sizeof(poke.nombre) && nivel >= 1 && nivel <= 100 &&
        ultimo_id_pokemon < INT_MAX) {
        strcpy(poke.especie, poke_base->nombre);
        strcpy(poke.nombre, nombre);
        poke.id = ++ultimo_id_pokemon;
        strcpy(poke.tipo_primario, poke_base->tipo_primario);
        strcpy(poke.tipo_secundario, poke_base->tipo_secundario);
        strcpy(poke.estado, "normal");
        gen_stats_pokemon_por_especie(&poke, poke_base, semilla, nivel);
    }

    return poke;
}

pokemon preguntar_crear_pokemon() {
    char especie[50], nombre[50];
    int nivel = 0, opcion = 2;
    int valido = 1, encontrada = 0;
    pokemon poke = {0};
    while (valido && !encontrada) {
        valido = leer_texto("¿Que especie quieres crear? ", especie, sizeof(especie));
        if (valido) {
            encontrada = buscar_nombre(especie) != NULL;
            if (!encontrada) puts("No esta en la Pokedex. Intenta con un Pokemon, no con tu vecino xd.");
        }
    }
    if (valido) valido = leer_texto("¿Que apodo le damos? ", nombre, sizeof(nombre));
    if (valido) valido = leer_opcion("Nivel (1..100): ", 1, 100, &nivel);
    while (valido && opcion == 2) {
        poke = crear_pokemon(especie, nombre, nivel);
        valido = poke.id > 0;
        if (valido) {
            mostrar_pokemon(&poke);
            valido = leer_opcion("¿Aceptas las stats? 1: si, 2: otra tirada: ", 1, 2, &opcion);
        }
    }
    if (!valido) poke = (pokemon){0};
    return poke;
}

// La misma formula, pero sin desbordar enteros si el archivo trae stats enormes.
static int calcular_stat(int base, int iv, uint32_t ev, int nivel, int bono) {
    double valor = ((base + (double)iv + ((int)sqrt(ev) / 4)) * nivel) / 100;
    valor = floor(valor) + bono;
    return valor >= INT_MAX ? INT_MAX : (int)valor;
}

// Sin guardar IV/EV no podemos reconstruir la tirada original. Comprobamos que
// cada stat caiga entre los extremos posibles de nuestra formula para ese nivel;
// es un control de rangos, no una prueba de cual fue su semilla.
int estadisticas_pokemon_validas(const pokemon *poke, const pokedata *base) {
    int valido = poke != NULL && base != NULL;
    if (valido) valido = poke->nivel >= 1 && poke->nivel <= 100;
    if (valido) {
        int valores[] = {poke->ataque, poke->defensa, poke->velocidad, poke->hp_max};
        int bases[] = {base->ataque, base->defensa, base->velocidad, base->hp};
        for (int i = 0; valido && i < 4; i++) {
            int bono = i == 3 ? poke->nivel + 10 : 5;
            int minimo = calcular_stat(bases[i], 0, 0, poke->nivel, bono);
            int maximo = calcular_stat(bases[i], 15, 65535, poke->nivel, bono);
            valido = valores[i] >= minimo && valores[i] <= maximo;
        }
    }
    return valido;
}

void gen_stats_pokemon_por_especie(pokemon *poke, pokedata *poke_base, uint32_t seed, int nivel){
    // esta funcion si la voy a comentar paso a paso pq sino parece chino xddd

    uint8_t iv; // stat individual (0-15)

    uint32_t ev; // stat de esfuerzo (0-65535)

    uint8_t iv_hp; // Se calcula con los bits menos significativos de cada stat para calcular hp
                   // inspirado en como se calcula en primera gen

    if (poke != NULL && poke_base != NULL && nivel >= 1 && nivel <= 100) {

        // guarda el seed completo en 32 bits, solo se recorta lo que se extrae de ella
        uint32_t semilla = (uint32_t)seed;

        poke->nivel = nivel;

        // se altera la semilla cada vez que se utiliza para tener una pseudo aleatoriedad
        semilla = alterador_semilla(semilla);

        ev = semilla & 0xFFFF;  // limitar a 16 bits (0-65535) para los stats de esfuerzo

        semilla = alterador_semilla(semilla);

        iv = semilla & 0xF; // limitar a 4 bits para los ivs (0-15)

        poke->ataque = calcular_stat(poke_base->ataque, iv, ev, nivel, 5);
        iv_hp = (iv & 1) << 3; // guardamos el bit menos significativo en la "ultima" posicion de los 4 bits de hp

        semilla = alterador_semilla(semilla);
        ev = semilla & 0xFFFF;
        semilla = alterador_semilla(semilla);
        iv = semilla & 0xF;

        poke->defensa = calcular_stat(poke_base->defensa, iv, ev, nivel, 5);
        iv_hp |= (iv & 1) << 2;

        semilla = alterador_semilla(semilla);
        ev = semilla & 0xFFFF;
        semilla = alterador_semilla(semilla);
        iv = semilla & 0xF;

        poke->velocidad = calcular_stat(poke_base->velocidad, iv, ev, nivel, 5);
        iv_hp |= (iv & 1) << 1;

        semilla = alterador_semilla(semilla);
        ev = semilla & 0xFFFF;
        semilla = alterador_semilla(semilla);
        iv = semilla & 0xF;
        iv_hp |= (iv & 1); // guardar un iv adicional para completar los 4 bits del iv de hp (es que no hay estadistica especial xd)

        poke->hp_max = calcular_stat(poke_base->hp, iv_hp, ev, nivel, nivel + 10);
    
        poke->hp = poke->hp_max; // al crear el pokemon, su hp actual es igual a su hp maximo
    }   

}

// El ID identifica al ejemplar, asi que recuperamos las bases por especie.
// Recalcular tambien restablece HP al maximo mediante la funcion compartida.
void gen_stats_pokemon(pokemon *poke, uint32_t seed, int nivel){
    if (poke != NULL) {
        pokedata *poke_base = buscar_nombre(poke->especie); // ID de ejemplar != ID de especie
        gen_stats_pokemon_por_especie(poke, poke_base, seed, nivel);
    }
}

void mostrar_pokemon(const pokemon *poke) {
    if (poke != NULL) {
        puts("\n------------------------------------------------------------");
        printf(" %s | Especie: %s | ID: %d | Nivel: %d\n",
            poke->nombre, poke->especie, poke->id, poke->nivel);
        printf(" Tipos: %s / %s | Estado: %s\n",
            poke->tipo_primario, poke->tipo_secundario, poke->estado);
        printf(" HP: %d/%d | Ataque: %d | Defensa: %d | Velocidad: %d\n",
            poke->hp, poke->hp_max, poke->ataque, poke->defensa, poke->velocidad);
        puts("------------------------------------------------------------\n");
    }
}
