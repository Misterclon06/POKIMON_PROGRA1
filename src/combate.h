#ifndef COMBATE_H
#define COMBATE_H
#include "entidades.h"

typedef enum {
    COMBATE_ERROR = 0,
    COMBATE_GANADOR_ENTRENADOR_1 = 1,
    COMBATE_GANADOR_ENTRENADOR_2 = 2,
    // Un limite tecnico detiene la simulacion, esto no declara un empate oficial.
    COMBATE_PENDIENTE = 3,
    COMBATE_EMPATE = 4
} resultado_combate;

typedef enum {
    COMBATE_AUTOMATICO = 1, // ambos usan ataque directo, sin backtracking
    COMBATE_INTERACTIVO = 2, // seleccion_automatica convierte ambos lados en bots 
    COMBATE_CONTRA_BOT = 3 // segundo lado automatico; primero segun seleccion_automatica
} modo_combate;

typedef struct {
    modo_combate modo;
    int seleccion_automatica; // evita teclado en simulaciones
    int mostrar;
    int usar_esquiva;
    int max_rondas;
    uint32_t semilla; // cero pide una semilla nueva; otro valor permite repetir
    int permitir_empate; // grupos: ningun equipo vivo puede causar daño
    int ataque_emergencia; // neutral 25 solo si ambos equipos estan bloqueados
    // En lados automaticos, permite medir frente al ataque directo.
    // No cambia los movimientos del jugador ni las reglas de la pelea.
    int ataque_directo[2];
    int pausar_rondas; // solo al mostrar combates entre bots; 0 en simulaciones de prueba
} opciones_combate;

typedef struct {
    int rondas;
    int ataques[2];
    int esquivas[2];
    int bloqueos[2];
    int usos_potencia[2][3]; // 50, 75, 100; 25 no necesita reserva
    unsigned long nodos_bot;
    int derrotados[2]; // cantidad de Pokemon perdidos por cada entrenador
    int sin_dano_posible;
} resumen_combate;

opciones_combate opciones_combate_iniciales(modo_combate modo);
// Empieza con HP completo y reservas nuevas; deja HP/estado de salida en los equipos
// resumen puede ser NULL. PENDIENTE no autoriza registrar empate ni repartir puntos
resultado_combate combatir_configurado(entrenador *primero, entrenador *segundo,
    const opciones_combate *opciones, resumen_combate *resumen);
// Elige el tipo de ataque automatico basado en los tipos de los pokemons
const char *elegir_tipo_ataque_automatico(const pokemon *atacante, const pokemon *defensor);
int potencia_desbloqueada(int nivel, int potencia);
int calcular_dano_con_potencia(const pokemon *atacante, const pokemon *defensor,
    double efectividad, int potencia, int *dano);
// Aplica el factor aleatorio clasico 217..255/255 al daño ejecutado.
// Conserva daños de 0 y 1, y permite repetir el sorteo con la misma semilla.
int variar_dano_combate(int dano, uint32_t *semilla);
// Porcentaje entero de 10 * velocidad_defensor / suma de velocidades
int probabilidad_esquiva(int velocidad_atacante, int velocidad_defensor);
#endif
