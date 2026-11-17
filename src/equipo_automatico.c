#include <stdio.h>
#include <string.h>

#include "equipo_automatico.h"
#include "efectividad.h"
#include "funcion_hasheo.h"
#include "utils.h"

#define LIMITE_NODOS_BUSQUEDA 2000000UL

typedef struct {
    const pokedata *especie;
    int nivel;
    int tipos[2];
} candidato_equipo;

typedef struct {
    const reglas_equipo_automatico *reglas;
    candidato_equipo candidatos[CANTIDAD];
    int equipo_actual[MAX_EQUIPO];
    int equipo_resultado[MAX_EQUIPO];
    int cantidad_candidatos;
    unsigned long nodos_visitados;
    uint32_t semilla_aleatoria;
    int veces_tipo[CANTIDAD_DE_TIPOS];
    int tipos_distintos;
} contexto_generacion_equipo;

// Un contador por tipo. Al volver de la recursion deshacemos lo que agregamos
static void modificar_tipos(contexto_generacion_equipo *contexto,
    const candidato_equipo *candidato, int cambio) {
    for (int i = 0; i < 2; i++) {
        int tipo = candidato->tipos[i];
        if (tipo >= 0) {
            int antes = contexto->veces_tipo[tipo];
            contexto->veces_tipo[tipo] += cambio;
            if (antes == 0 && contexto->veces_tipo[tipo] > 0) contexto->tipos_distintos++;
            else if (antes > 0 && contexto->veces_tipo[tipo] == 0) contexto->tipos_distintos--;
        }
    }
}

// Avanza una secuencia pseudoaleatoria
static unsigned int siguiente_aleatorio(contexto_generacion_equipo *contexto) {
    contexto->semilla_aleatoria = alterador_semilla(contexto->semilla_aleatoria);
    return contexto->semilla_aleatoria;
}

// Prepara especie, nivel e indices de tipos, sin crear ejemplares descartables.
// El backtracking solo necesita esos datos: los IV/EV esperan hasta tener equipo.
static int crear_candidato(
    contexto_generacion_equipo *contexto, int id, candidato_equipo *resultado) {
    const pokedata *especie = buscar_id(id);
    int valido = especie != NULL && especie->id == id && resultado != NULL;
    if (valido) {
        resultado->especie = especie;
        resultado->nivel = contexto->reglas->nivel_comun;
        if (resultado->nivel == 0) {
            resultado->nivel = (int)(siguiente_aleatorio(contexto) %
                (unsigned int)contexto->reglas->nivel_maximo_individual) + 1;
        }
        resultado->tipos[0] = obtener_tipo(especie->tipo_primario);
        resultado->tipos[1] = obtener_tipo(especie->tipo_secundario);
        valido = resultado->tipos[0] >= 0 &&
            (resultado->tipos[1] >= 0 || strcmp(especie->tipo_secundario, "-") == 0);
    }
    return valido;
}

// Mezcla las especies para que la busqueda no favorezca el orden numerico de la Pokedex.
static void mezclar_candidatos(contexto_generacion_equipo *contexto) {
    for (int i = contexto->cantidad_candidatos - 1; i > 0; i--) {
        int indice = (int)(siguiente_aleatorio(contexto) % (unsigned int)(i + 1));
        candidato_equipo temporal = contexto->candidatos[i];
        contexto->candidatos[i] = contexto->candidatos[indice];
        contexto->candidatos[indice] = temporal;
    }
}

// Descarta la rama si ya no quedan suficientes especies para completar el equipo.
static int quedan_suficientes_especies(
    const contexto_generacion_equipo *contexto,
    int siguiente_candidato,
    int espacios_restantes) {
    return contexto->cantidad_candidatos - siguiente_candidato >= espacios_restantes;
}

// Retrocede cuando una seleccion impide cumplir la diversidad de tipos requerida.
// Solo prueba candidatos posteriores al elegido: no repite especie ni visita
// todas las permutaciones del mismo equipo. Nos interesa el equipo, no sus asientos xd.
static int construir_equipo_recursivo(
    contexto_generacion_equipo *contexto, int posicion, int siguiente_candidato) {
    int encontrado = 0;
    int espacios = contexto->reglas->cantidad_pokemon - posicion;
    contexto->nodos_visitados++;
    // Cada espacio aporta como mucho dos tipos nuevos. Si ni ese caso ideal
    // alcanza el minimo, explorar esa rama seria gastar llamadas por deporte
    int viable = contexto->nodos_visitados <= LIMITE_NODOS_BUSQUEDA &&
        quedan_suficientes_especies(contexto, siguiente_candidato, espacios) &&
        contexto->tipos_distintos + 2 * espacios >= contexto->reglas->tipos_distintos_minimos;
    if (viable) {
        if (espacios == 0) {
            encontrado = contexto->tipos_distintos >= contexto->reglas->tipos_distintos_minimos;
            if (encontrado) {
                memcpy(contexto->equipo_resultado, contexto->equipo_actual,
                    (size_t)posicion * sizeof(contexto->equipo_resultado[0]));
            }
        } else {
            int i = siguiente_candidato;
            while (!encontrado && i <= contexto->cantidad_candidatos - espacios &&
                   contexto->nodos_visitados < LIMITE_NODOS_BUSQUEDA) {
                contexto->equipo_actual[posicion] = i;
                modificar_tipos(contexto, &contexto->candidatos[i], 1);
                encontrado = construir_equipo_recursivo(contexto, posicion + 1, i + 1);
                modificar_tipos(contexto, &contexto->candidatos[i], -1);
                i++;
            }
        }
    }
    return encontrado;
}

// Consulta cada especie una sola vez y guarda sus tipos antes de buscar.
// Las ramas solo suman/restan indices: nada de buscar nombres por deporte xd.
// Mezcla el orden de prueba y rechaza una Pokedex incompleta.
static int preparar_candidatos(contexto_generacion_equipo *contexto) {
    int valido = 1;
    int id = 1;
    while (valido && id <= CANTIDAD) {
        valido = crear_candidato(contexto, id,
            &contexto->candidatos[contexto->cantidad_candidatos]);
        if (valido) contexto->cantidad_candidatos++;
        id++;
    }
    if (valido) mezclar_candidatos(contexto);
    return valido;
}

// La recursion sigue eligiendo, probando y deshaciendo candidatos. Al encontrar
// solucion construimos SOLO sus ejemplares y los guardamos primero en una copia.
// Si falla un constructor, el equipo anterior no paga las consecuencias xd.
static int construir_ejemplares(const contexto_generacion_equipo *contexto,
    entrenador *destino) {
    entrenador preparado = *destino;
    preparado.n_pokemon = 0;
    int valido = 1;
    for (int i = 0; valido && i < contexto->reglas->cantidad_pokemon; i++) {
        const candidato_equipo *candidato = &contexto->candidatos[contexto->equipo_resultado[i]];
        pokemon poke = crear_pokemon(candidato->especie->nombre,
            candidato->especie->nombre, candidato->nivel);
        valido = poke.id > 0;
        if (valido) agregar_pokemon_a_entrenador(&preparado, poke);
    }
    if (valido) *destino = preparado;
    return valido;
}

// Sustituye el equipo solo al encontrar la primera combinacion valida; conserva
// identidad y resultados del entrenador. Agotar el limite de nodos no demuestra
// que no exista solucion: por eso devolvemos un resultado distinto.
resultado_generacion_equipo generar_equipo_automatico(
    const reglas_equipo_automatico *reglas,
    entrenador *destino,
    unsigned long *nodos_visitados) {
    contexto_generacion_equipo contexto = {0};
    resultado_generacion_equipo resultado = GENERACION_EQUIPO_ARGUMENTO_INVALIDO;
    int validas = reglas != NULL && destino != NULL;
    if (validas) {
        validas = reglas->cantidad_pokemon >= 1 &&
            reglas->cantidad_pokemon <= MAX_EQUIPO &&
            reglas->nivel_maximo_individual >= 1 &&
            reglas->nivel_maximo_individual <= 100 &&
            reglas->nivel_comun >= 0 &&
            reglas->nivel_comun <= reglas->nivel_maximo_individual &&
            reglas->tipos_distintos_minimos >= 0 &&
            reglas->tipos_distintos_minimos <= CANTIDAD_DE_TIPOS &&
            reglas->tipos_distintos_minimos <= 2 * reglas->cantidad_pokemon;
    }
    if (validas) {
        contexto.reglas = reglas;
        contexto.semilla_aleatoria = gen_semilla();
        resultado = GENERACION_EQUIPO_DATOS_NO_DISPONIBLES;
        if (preparar_candidatos(&contexto)) {
            if (construir_equipo_recursivo(&contexto, 0, 0)) {
                if (construir_ejemplares(&contexto, destino)) resultado = GENERACION_EQUIPO_OK;
            } else {
                resultado = contexto.nodos_visitados >= LIMITE_NODOS_BUSQUEDA
                    ? GENERACION_EQUIPO_LIMITE_BUSQUEDA : GENERACION_EQUIPO_SIN_SOLUCION;
            }
            if (nodos_visitados != NULL) *nodos_visitados = contexto.nodos_visitados;
        }
    }
    return resultado;
}
