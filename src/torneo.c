#include <limits.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include "torneo.h"
#include "entrada.h"
#include "efectividad.h"

// Limites de las rondas del formato de 32 entrenadores del PDF.
enum {
    PRIMER_OCTAVO = COMBATES_GRUPOS + 1,
    PRIMER_CUARTO = PRIMER_OCTAVO + GRUPOS_TORNEO,
    PRIMER_SEMIFINAL = PRIMER_CUARTO + GRUPOS_TORNEO / 2,
    TERCER_PUESTO = PRIMER_SEMIFINAL + GRUPOS_TORNEO / 4
};

void torneo_crear(torneo *liga) {
    if (liga != NULL) {
        memset(liga, 0, sizeof(*liga));
        cola_crear(&liga->pendientes);
    }
}

void torneo_destruir(torneo *liga) {
    if (liga != NULL) cola_destruir(&liga->pendientes);
}

int torneo_indice_entrenador(const torneo *liga, int id) {
    int indice = -1;
    if (liga != NULL) {
        int i = 0;
        while (i < liga->cantidad && indice == -1) {
            if (liga->entrenadores[i].id == id) indice = i;
            i++;
        }
    }
    return indice;
}

// No basta con stats positivas: especie, tipos y rangos deben coincidir con la
// Pokedex y nuestra formula. Un Magikarp con ataque de meteorito no pasa aduana xd.
static int ejemplar_valido(const pokemon *p) {
    int valido = p->id > 0 && p->nivel >= 1 && p->nivel <= 100 &&
        p->hp_max > 0 && p->hp >= 0 && p->hp <= p->hp_max &&
        p->ataque > 0 && p->defensa > 0 && p->velocidad > 0 &&
        memchr(p->especie, '\0', sizeof(p->especie)) != NULL &&
        memchr(p->nombre, '\0', sizeof(p->nombre)) != NULL && p->nombre[0] != '\0' &&
        memchr(p->tipo_primario, '\0', sizeof(p->tipo_primario)) != NULL &&
        memchr(p->tipo_secundario, '\0', sizeof(p->tipo_secundario)) != NULL;
    if (valido) {
        const pokedata *especie = buscar_nombre(p->especie);
        valido = especie != NULL;
        if (valido) valido = estadisticas_pokemon_validas(p, especie) &&
            obtener_tipo(p->tipo_primario) == obtener_tipo(especie->tipo_primario) &&
            ((strcmp(p->tipo_secundario, "-") == 0 && strcmp(especie->tipo_secundario, "-") == 0) ||
             (obtener_tipo(p->tipo_secundario) >= 0 && obtener_tipo(p->tipo_secundario) == obtener_tipo(especie->tipo_secundario)));
    }
    return valido;
}

// requiere_equipo = 0 permite registrarse sin Pokemon; = 1 exige alguno al iniciar.
// Tambien impide que dos puestos del mismo equipo compartan ID de ejemplar.
static int entrenador_valido(const entrenador *ent, int requiere_equipo) {
    int valido = ent != NULL;
    if (valido) valido = ent->id > 0 && ent->n_pokemon >= requiere_equipo && ent->n_pokemon <= MAX_EQUIPO &&
        memchr(ent->nombre, '\0', sizeof(ent->nombre)) != NULL && ent->nombre[0] != '\0';
    for (int i = 0; valido && i < ent->n_pokemon; i++) {
        valido = ejemplar_valido(&ent->equipo[i]);
        for (int j = 0; valido && j < i; j++) valido = ent->equipo[i].id != ent->equipo[j].id;
    }
    return valido;
}

// Copia el entrenador antes de empezar, con ID propio y ejemplares que no esten
// registrados en otro equipo. Repetir especie es posible; clonar identidad, no xd.
int torneo_registrar(torneo *liga, const entrenador *nuevo) {
    int valido = liga != NULL && entrenador_valido(nuevo, 0);
    if (valido) valido = !liga->iniciado && liga->cantidad < ENTRENADORES_TORNEO &&
        torneo_indice_entrenador(liga, nuevo->id) == -1;
    for (int i = 0; valido && i < liga->cantidad; i++) {
        for (int j = 0; valido && j < liga->entrenadores[i].n_pokemon; j++) {
            for (int k = 0; valido && k < nuevo->n_pokemon; k++) {
                valido = liga->entrenadores[i].equipo[j].id != nuevo->equipo[k].id;
            }
        }
    }
    if (valido) {
        liga->entrenadores[liga->cantidad++] = *nuevo;
    }
    return valido;
}

// Una fuente dice de donde sale un participante: 1A/2A = puesto del grupo,
// G49 = ganador del 49, P61 = perdedor del 61. Solo admite la ronda previa
// correspondiente; los perdedores se usan exclusivamente para tercer puesto.
static int fuente_valida(const char *texto, int numero) {
    int valido = 0;
    if (numero < PRIMER_CUARTO) {
        valido = strlen(texto) == 2 && texto[0] >= '1' && texto[0] <= '2' &&
            texto[1] >= 'A' && texto[1] < 'A' + GRUPOS_TORNEO;
    } else if (strlen(texto) == 3 && texto[0] == (numero == TERCER_PUESTO ? 'P' : 'G')) {
        int minimo = PRIMER_SEMIFINAL, maximo = TERCER_PUESTO - 1;
        if (numero < PRIMER_SEMIFINAL) {
            minimo = PRIMER_OCTAVO;
            maximo = PRIMER_CUARTO - 1;
        } else if (numero < TERCER_PUESTO) {
            minimo = PRIMER_CUARTO;
            maximo = PRIMER_SEMIFINAL - 1;
        }
        int anterior;
        valido = convertir_entero(texto + 1, minimo, maximo, &anterior);
    }
    return valido;
}

// Comprueba la estructura del cuadro leido de DB: primero contra segundo de otro
// grupo en octavos y ninguna plaza repetida dentro de su ronda.
static int cruces_validos(const char fuentes[][2][4]) {
    int valido = 1;
    for (int i = 0; valido && i < COMBATES_TORNEO - COMBATES_GRUPOS; i++) {
        int numero = i + PRIMER_OCTAVO;
        int inicio = numero < PRIMER_CUARTO ? 0 :
            numero < PRIMER_SEMIFINAL ? PRIMER_CUARTO - PRIMER_OCTAVO :
            numero < TERCER_PUESTO ? PRIMER_SEMIFINAL - PRIMER_OCTAVO : i;
        if (numero < PRIMER_CUARTO) {
            valido = fuentes[i][0][0] == '1' && fuentes[i][1][0] == '2' &&
                fuentes[i][0][1] != fuentes[i][1][1];
        }
        // Una plaza se usa una sola vez por ronda. El ganador no se multiplica xd.
        for (int lado = 0; valido && lado < 2; lado++) {
            valido = fuente_valida(fuentes[i][lado], numero);
            for (int anterior = inicio; valido && anterior <= i; anterior++) {
                int lados_previos = anterior == i ? lado : 2;
                for (int otro = 0; valido && otro < lados_previos; otro++) {
                    valido = strcmp(fuentes[i][lado], fuentes[anterior][otro]) != 0;
                }
            }
        }
    }
    return valido;
}

// Valida equipos y cuadro antes de abrir el torneo. El orden de registro arma
// los grupos; cada pareja del grupo juega una vez y las eliminatorias esperan fuentes.
int torneo_iniciar(torneo *liga, const char *ruta_cruces) {
    int valido = liga != NULL && ruta_cruces != NULL;
    if (valido) valido = !liga->iniciado && liga->cantidad == ENTRENADORES_TORNEO;
    for (int i = 0; valido && i < liga->cantidad; i++) {
        valido = entrenador_valido(&liga->entrenadores[i], 1);
        for (int j = 0; valido && j < i; j++) {
            for (int p = 0; valido && p < liga->entrenadores[i].n_pokemon; p++) {
                for (int q = 0; valido && q < liga->entrenadores[j].n_pokemon; q++) {
                    valido = liga->entrenadores[i].equipo[p].id != liga->entrenadores[j].equipo[q].id;
                }
            }
        }
    }
    char fuentes[COMBATES_TORNEO - COMBATES_GRUPOS][2][4] = {{{0}}};
    if (valido) {
        FILE *archivo = fopen(ruta_cruces, "r");
        valido = archivo != NULL;
        if (valido) {
            char linea[100];
            int numero = PRIMER_OCTAVO;
            while (valido && numero <= COMBATES_TORNEO) {
                char nro[10], extra;
                int leido;
                valido = leer_linea(archivo, linea, sizeof(linea)) == 1;
                if (valido) valido = sscanf(linea, "%9s %3s %3s %c", nro,
                    fuentes[numero - PRIMER_OCTAVO][0], fuentes[numero - PRIMER_OCTAVO][1], &extra) == 3 &&
                    convertir_entero(nro, numero, numero, &leido);
                numero++;
            }
            valido = valido && leer_linea(archivo, linea, sizeof(linea)) == 0 && !ferror(archivo);
            fclose(archivo);
        }
    }
    if (valido) valido = cruces_validos(fuentes);
    if (valido) {
        memcpy(liga->fuentes, fuentes, sizeof(fuentes));
        int numero = 0;
        for (int grupo = 0; grupo < GRUPOS_TORNEO; grupo++) {
            for (int i = 0; i < INTEGRANTES_GRUPO; i++) {
                for (int j = i + 1; j < INTEGRANTES_GRUPO; j++) {
                    liga->encuentros[numero].participantes[0] = liga->entrenadores[grupo * 4 + i].id;
                    liga->encuentros[numero].participantes[1] = liga->entrenadores[grupo * 4 + j].id;
                    numero++;
                }
            }
        }
        liga->iniciado = 1;
        valido = torneo_actualizar(liga);
        if (!valido) {
            liga->iniciado = 0;
            memset(liga->encuentros, 0, sizeof(liga->encuentros));
            cola_destruir(&liga->pendientes);
        }
    }
    return valido;
}

// Devuelve 1 si a va antes que b, -1 si va despues y 0 si siguen iguales.
// Orden: puntos, victorias, rivales derrotados (PDF), menos bajas propias.
static int comparar_base(const estadistica_grupo *a, const estadistica_grupo *b) {
    int comparacion = 0;
    if (a->puntos != b->puntos) comparacion = a->puntos > b->puntos ? 1 : -1;
    else if (a->victorias != b->victorias) comparacion = a->victorias > b->victorias ? 1 : -1;
    else if (a->rivales_derrotados != b->rivales_derrotados) comparacion = a->rivales_derrotados > b->rivales_derrotados ? 1 : -1;
    else if (a->propios_derrotados != b->propios_derrotados) comparacion = a->propios_derrotados < b->propios_derrotados ? 1 : -1;
    return comparacion;
}

// Suma el encuentro desde la perspectiva de un participante: 3/1/0 puntos
// y bajas de ambos equipos. Se reutiliza en la tabla oficial y en la minitabla.
static void acumular_estadistica(estadistica_grupo *s, const encuentro *e, int lado) {
    int victoria = e->ganador == e->participantes[lado];
    int empate = e->ganador == 0;
    s->victorias += victoria;
    s->empates += empate;
    s->derrotas += !victoria && !empate;
    s->puntos += victoria ? 3 : empate;
    s->rivales_derrotados += e->derrotados[1 - lado];
    s->propios_derrotados += e->derrotados[lado];
}

// Solo los partidos entre quienes siguen empatados. No se alteran los puntos oficiales.
// Reordena el tramo [inicio, fin) con los mismos cuatro criterios. Devuelve 0 si
// queda alguna igualdad: el indice del arreglo no es un criterio deportivo xd.
static int ordenar_minitabla(torneo *liga, int grupo, int inicio, int fin) {
    estadistica_grupo mini[INTEGRANTES_GRUPO] = {{0}};
    int incluidos[INTEGRANTES_GRUPO] = {0};
    for (int i = inicio; i < fin; i++) incluidos[liga->posiciones[grupo][i] - grupo * 4] = 1;
    for (int i = grupo * 6; i < grupo * 6 + 6; i++) {
        const encuentro *e = &liga->encuentros[i];
        int a = torneo_indice_entrenador(liga, e->participantes[0]) - grupo * 4;
        int b = torneo_indice_entrenador(liga, e->participantes[1]) - grupo * 4;
        if (e->resuelto && incluidos[a] && incluidos[b]) {
            acumular_estadistica(&mini[a], e, 0);
            acumular_estadistica(&mini[b], e, 1);
        }
    }
    for (int i = inicio + 1; i < fin; i++) {
        int valor = liga->posiciones[grupo][i], j = i;
        while (j > inicio && comparar_base(&mini[valor - grupo * 4],
            &mini[liga->posiciones[grupo][j - 1] - grupo * 4]) > 0) {
            liga->posiciones[grupo][j] = liga->posiciones[grupo][j - 1];
            j--;
        }
        liga->posiciones[grupo][j] = valor;
    }
    int resuelta = 1;
    for (int i = inicio + 1; i < fin; i++) {
        if (comparar_base(&mini[liga->posiciones[grupo][i - 1] - grupo * 4],
            &mini[liga->posiciones[grupo][i] - grupo * 4]) == 0) resuelta = 0;
    }
    return resuelta;
}

// Traduce una fuente ya validada a un ID real; 0 significa que aun falta resolver
// su grupo o encuentro previo. Mantiene cerrados los cruces sin participantes ciertos.
static int resolver_fuente(const torneo *liga, const char *fuente) {
    int id = 0;
    if (fuente[0] == '1' || fuente[0] == '2') {
        int grupo = fuente[1] - 'A';
        if (liga->grupo_clasificado[grupo]) id = liga->entrenadores[liga->posiciones[grupo][fuente[0] - '1']].id;
    } else {
        int numero;
        if (convertir_entero(fuente + 1, 49, 63, &numero)) {
            const encuentro *previo = &liga->encuentros[numero - 1];
            if (previo->resuelto && previo->ganador != 0) {
                id = previo->ganador;
                if (fuente[0] == 'P') id = previo->participantes[0] == previo->ganador ? previo->participantes[1] : previo->participantes[0];
            }
        }
    }
    return id;
}

// Reconstruye estadisticas, clasificados y cola desde los resultados guardados.
// Recalcular desde cero evita sumar puntos otra vez al importar o refrescar.
// V/E/D incluye todo el torneo; puntos y orden de grupos solo incluyen sus 48 partidos.
int torneo_actualizar(torneo *liga) {
    int valido = liga != NULL && liga->iniciado;
    if (valido) {
        memset(liga->estadisticas, 0, sizeof(liga->estadisticas));
        for (int i = 0; i < liga->cantidad; i++) {
            liga->entrenadores[i].n_victorias = liga->entrenadores[i].n_empates = 0;
            liga->entrenadores[i].n_derrotas = liga->entrenadores[i].puntuacion = 0;
        }
        for (int i = 0; i < COMBATES_TORNEO; i++) {
            encuentro *e = &liga->encuentros[i];
            if (e->resuelto) {
                for (int lado = 0; lado < 2; lado++) {
                    int indice = torneo_indice_entrenador(liga, e->participantes[lado]);
                    entrenador *ent = &liga->entrenadores[indice];
                    int victoria = e->ganador == ent->id, empate = e->ganador == 0;
                    ent->n_victorias += victoria;
                    ent->n_empates += empate;
                    ent->n_derrotas += !victoria && !empate;
                    if (i < COMBATES_GRUPOS) {
                        estadistica_grupo *s = &liga->estadisticas[indice];
                        acumular_estadistica(s, e, lado);
                        ent->puntuacion = s->puntos;
                    }
                }
            }
        }
        for (int grupo = 0; grupo < GRUPOS_TORNEO; grupo++) {
            int completo = 1;
            for (int i = 0; i < 6; i++) completo = completo && liga->encuentros[grupo * 6 + i].resuelto;
            for (int i = 0; i < 4; i++) liga->posiciones[grupo][i] = grupo * 4 + i;
            // Cuatro elementos: ordenamiento por insercion. No hace falta invocar a la NASA.
            for (int i = 1; i < 4; i++) {
                int valor = liga->posiciones[grupo][i], j = i;
                while (j > 0 && comparar_base(&liga->estadisticas[valor],
                    &liga->estadisticas[liga->posiciones[grupo][j - 1]]) > 0) {
                    liga->posiciones[grupo][j] = liga->posiciones[grupo][j - 1];
                    j--;
                }
                liga->posiciones[grupo][j] = valor;
            }
            liga->grupo_clasificado[grupo] = completo;
            int inicio = 0;
            while (inicio < INTEGRANTES_GRUPO) {
                int fin = inicio + 1;
                while (fin < INTEGRANTES_GRUPO && comparar_base(
                    &liga->estadisticas[liga->posiciones[grupo][inicio]],
                    &liga->estadisticas[liga->posiciones[grupo][fin]]) == 0) fin++;
                if (fin - inicio > 1 && !ordenar_minitabla(liga, grupo, inicio, fin)) liga->grupo_clasificado[grupo] = 0;
                inicio = fin;
            }
        }
        for (int i = COMBATES_GRUPOS; i < COMBATES_TORNEO; i++) {
            liga->encuentros[i].participantes[0] = resolver_fuente(liga, liga->fuentes[i - COMBATES_GRUPOS][0]);
            liga->encuentros[i].participantes[1] = resolver_fuente(liga, liga->fuentes[i - COMBATES_GRUPOS][1]);
        }
        cola_destruir(&liga->pendientes);
        cola_crear(&liga->pendientes);
        valido = liga->pendientes.inicio != NULL;
        for (int i = 0; valido && i < COMBATES_TORNEO; i++) {
            encuentro *e = &liga->encuentros[i];
            if (!e->resuelto && e->participantes[0] != 0 && e->participantes[1] != 0) {
                int antes = cola_size(&liga->pendientes);
                cola_encolar(&liga->pendientes, i + 1);
                valido = cola_size(&liga->pendientes) == antes + 1;
            }
        }
    }
    return valido;
}

// Solo acepta participantes habilitados y bajas compatibles con el resultado:
// ganador con vivos, perdedor sin ellos; en empate de grupos ambos conservan vivos.
// Si falla la actualizacion, restaura el encuentro y reconstruye los datos derivados.
int torneo_registrar_resultado(torneo *liga, int numero, int primero, int segundo,
    int ganador, int derrotados_primero, int derrotados_segundo) {
    int valido = liga != NULL && numero >= 1 && numero <= COMBATES_TORNEO;
    if (valido) valido = liga->iniciado && !liga->encuentros[numero - 1].resuelto;
    if (valido) {
        encuentro *e = &liga->encuentros[numero - 1];
        valido = primero > 0 && segundo > 0 && primero != segundo &&
            primero == e->participantes[0] && segundo == e->participantes[1] &&
            (ganador == primero || ganador == segundo || (ganador == 0 && numero <= COMBATES_GRUPOS));
        if (valido) {
            int a = torneo_indice_entrenador(liga, primero), b = torneo_indice_entrenador(liga, segundo);
            int cantidad_a = liga->entrenadores[a].n_pokemon, cantidad_b = liga->entrenadores[b].n_pokemon;
            valido = derrotados_primero >= 0 && derrotados_primero <= cantidad_a &&
                derrotados_segundo >= 0 && derrotados_segundo <= cantidad_b;
            if (valido && ganador != 0) valido = ganador == primero
                ? derrotados_segundo == cantidad_b && derrotados_primero < cantidad_a
                : derrotados_primero == cantidad_a && derrotados_segundo < cantidad_b;
            else if (valido) valido = derrotados_primero < cantidad_a && derrotados_segundo < cantidad_b;
        }
        if (valido) {
            encuentro anterior = *e;
            e->resuelto = 1;
            e->ganador = ganador;
            e->derrotados[0] = derrotados_primero;
            e->derrotados[1] = derrotados_segundo;
            valido = torneo_actualizar(liga);
            if (!valido) {
                *e = anterior;
                torneo_actualizar(liga);
            }
        }
    }
    return valido;
}

int torneo_podio(const torneo *liga, int ids[4]) {
    int valido = liga != NULL && ids != NULL;
    if (valido) valido = liga->iniciado && liga->encuentros[62].resuelto && liga->encuentros[63].resuelto;
    if (valido) {
        ids[0] = liga->encuentros[63].ganador;
        ids[1] = liga->encuentros[63].participantes[0] == ids[0] ? liga->encuentros[63].participantes[1] : liga->encuentros[63].participantes[0];
        ids[2] = liga->encuentros[62].ganador;
        ids[3] = liga->encuentros[62].participantes[0] == ids[2] ? liga->encuentros[62].participantes[1] : liga->encuentros[62].participantes[0];
    }
    return valido;
}

static int escribir_resultado(FILE *archivo, int numero, const encuentro *e) {
    int correcto = fprintf(archivo, "%d %d %d %s %d %d %d\n", numero,
        e->participantes[0], e->participantes[1],
        e->ganador == 0 ? "EMPATE" : "VICTORIA", e->ganador,
        e->derrotados[0], e->derrotados[1]) > 0;
    return correcto;
}

static int escribir_resultados_archivo(const torneo *liga, FILE *archivo) {
    int correcto = liga != NULL && archivo != NULL && liga->iniciado;
    if (correcto) {
        for (int i = 0; correcto && i < COMBATES_TORNEO; i++) {
            const encuentro *e = &liga->encuentros[i];
            if (e->resuelto) correcto = escribir_resultado(archivo, i + 1, e);
        }
        correcto = correcto && !ferror(archivo);
    }
    return correcto;
}

static int cantidad_resultados(const torneo *liga) {
    int cantidad = 0;
    if (liga != NULL && liga->iniciado) {
        for (int i = 0; i < COMBATES_TORNEO; i++) cantidad += liga->encuentros[i].resuelto;
    }
    return cantidad;
}

int torneo_guardar_resultados(const torneo *liga, const char *ruta) {
    int valido = liga != NULL && ruta != NULL && liga->iniciado;
    FILE *archivo = valido ? fopen(ruta, "w") : NULL;
    valido = archivo != NULL;
    if (valido) {
        valido = escribir_resultados_archivo(liga, archivo);
        valido = fclose(archivo) == 0 && valido;
    }
    return valido;
}

static int aplicar_resultados_archivo(torneo *liga, FILE *archivo,
    int cantidad, int validar_fin) {
    int valido = liga != NULL && archivo != NULL && liga->iniciado &&
        cantidad >= -1 && cantidad <= COMBATES_TORNEO;
    if (valido) {
        encuentro importados[COMBATES_TORNEO] = {0};
        encuentro respaldo[COMBATES_TORNEO];
        memcpy(respaldo, liga->encuentros, sizeof(respaldo));
        char linea[200];
        int limite = cantidad < 0 ? COMBATES_TORNEO : cantidad;
        int leidos = 0, lectura = 1;
        while (valido && leidos < limite && lectura == 1) {
            lectura = leer_linea(archivo, linea, sizeof(linea));
            if (lectura == 1) {
                char tokens[7][32], extra;
                int numero, a, b, ganador, caidos_a, caidos_b;
                valido = sscanf(linea, "%31s %31s %31s %31s %31s %31s %31s %c",
                    tokens[0], tokens[1], tokens[2], tokens[3], tokens[4],
                    tokens[5], tokens[6], &extra) == 7 &&
                    convertir_entero(tokens[0], 1, COMBATES_TORNEO, &numero) &&
                    convertir_entero(tokens[1], 1, INT_MAX, &a) &&
                    convertir_entero(tokens[2], 1, INT_MAX, &b) &&
                    convertir_entero(tokens[4], 0, INT_MAX, &ganador) &&
                    convertir_entero(tokens[5], 0, MAX_EQUIPO, &caidos_a) &&
                    convertir_entero(tokens[6], 0, MAX_EQUIPO, &caidos_b);
                if (valido) valido = !importados[numero - 1].resuelto &&
                    ((ganador == 0 && strcmp(tokens[3], "EMPATE") == 0) ||
                     (ganador > 0 && strcmp(tokens[3], "VICTORIA") == 0));
                if (valido) importados[numero - 1] =
                    (encuentro){{a, b}, 1, ganador, {caidos_a, caidos_b}};
                leidos++;
            } else if (cantidad >= 0) valido = 0;
            else if (lectura == -1) valido = 0;
        }
        if (cantidad < 0 && leidos == limite && lectura == 1)
            lectura = leer_linea(archivo, linea, sizeof(linea));
        if (cantidad < 0) valido = valido && lectura == 0;
        valido = valido && !ferror(archivo);
        if (validar_fin && valido) {
            int siguiente = fgetc(archivo);
            valido = siguiente == EOF && !ferror(archivo);
        }
        for (int i = 0; valido && i < COMBATES_TORNEO; i++) {
            encuentro *e = &importados[i];
            if (e->resuelto) {
                const encuentro *actual = &liga->encuentros[i];
                if (actual->resuelto) valido = actual->participantes[0] == e->participantes[0] &&
                    actual->participantes[1] == e->participantes[1] &&
                    actual->ganador == e->ganador &&
                    actual->derrotados[0] == e->derrotados[0] &&
                    actual->derrotados[1] == e->derrotados[1];
                else valido = torneo_registrar_resultado(liga, i + 1,
                    e->participantes[0], e->participantes[1], e->ganador,
                    e->derrotados[0], e->derrotados[1]);
            }
        }
        if (!valido) {
            memcpy(liga->encuentros, respaldo, sizeof(respaldo));
            torneo_actualizar(liga);
        }
    }
    return valido;
}

// La carga acepta lineas desordenadas, pero aplica las dependencias en orden
// de encuentro. El respaldo evita una liga a medio importar si algo falla.
int torneo_cargar_resultados(torneo *liga, const char *ruta) {
    int valido = liga != NULL && ruta != NULL && liga->iniciado;
    FILE *archivo = valido ? fopen(ruta, "r") : NULL;
    valido = archivo != NULL;
    if (valido) {
        valido = aplicar_resultados_archivo(liga, archivo, -1, 0);
        fclose(archivo);
    }
    return valido;
}

int torneo_guardar_clasificacion(const torneo *liga, const char *ruta) {
    int valido = liga != NULL && ruta != NULL && liga->iniciado;
    FILE *archivo = valido ? fopen(ruta, "w") : NULL;
    valido = archivo != NULL;
    if (valido) {
        fputs("Grupo Posicion ID Nombre Victorias Empates Derrotas Puntos Pokemon_derrotados Pokemon_propios_derrotados\n", archivo);
        for (int g = 0; g < GRUPOS_TORNEO; g++) {
            for (int p = 0; p < INTEGRANTES_GRUPO; p++) {
                int i = liga->posiciones[g][p];
                const entrenador *ent = &liga->entrenadores[i];
                const estadistica_grupo *s = &liga->estadisticas[i];
                fprintf(archivo, "%c %d %d %s %d %d %d %d %d %d%s\n", 'A' + g, p + 1,
                    ent->id, ent->nombre, s->victorias, s->empates, s->derrotas, s->puntos, s->rivales_derrotados,
                    s->propios_derrotados,
                    liga->grupo_clasificado[g] ? "" : " (provisional)");
            }
        }
        valido = !ferror(archivo);
        valido = fclose(archivo) == 0 && valido;
    }
    return valido;
}

static int escribir_entrenadores_archivo(const torneo *liga, FILE *archivo) {
    int valido = liga != NULL && archivo != NULL &&
        liga->cantidad >= 0 && liga->cantidad <= ENTRENADORES_TORNEO;
    if (valido) {
        fprintf(archivo, "%d\n", liga->cantidad);
        for (int i = 0; valido && i < liga->cantidad; i++) {
            const entrenador *ent = &liga->entrenadores[i];
            valido = entrenador_valido(ent, 0) &&
                fprintf(archivo, "%d %d\n%s\n", ent->id,
                ent->n_pokemon, ent->nombre) > 0;
            for (int j = 0; valido && j < ent->n_pokemon; j++) {
                const pokemon *p = &ent->equipo[j];
                valido = fprintf(archivo, "%d %d %d %d %d %d\n%s\n%s\n",
                    p->id, p->nivel, p->hp_max, p->ataque, p->defensa,
                    p->velocidad, p->especie, p->nombre) > 0;
            }
        }
        valido = valido && !ferror(archivo);
    }
    return valido;
}

int torneo_guardar_entrenadores(const torneo *liga, const char *ruta) {
    int valido = liga != NULL && ruta != NULL;
    FILE *archivo = valido ? fopen(ruta, "w") : NULL;
    valido = archivo != NULL;
    if (valido) {
        valido = escribir_entrenadores_archivo(liga, archivo);
        valido = fclose(archivo) == 0 && valido;
    }
    return valido;
}

// En el formato de entrenadores ninguna linea de un dato puede estar vacia,
// truncada o ausente. Adaptamos los estados de leer_linea a valido/invalido.
static int leer_linea_archivo(FILE *archivo, char *texto, int capacidad) {
    int valido = leer_linea(archivo, texto, capacidad) == 1 && texto[0] != '\0';
    return valido;
}

// Lee un bloque sin consumir la siguiente etiqueta; los IDs se aplican al
// contador global solo despues de aceptar la partida completa.
static int leer_entrenadores_archivo(torneo *destino, FILE *archivo,
    int *ultimo_pokemon, int *ultimo_entrenador) {
    int valido = destino != NULL && archivo != NULL && ultimo_pokemon != NULL &&
        ultimo_entrenador != NULL && !destino->iniciado && destino->cantidad == 0;
    if (valido) {
        torneo temporal;
        torneo_crear(&temporal);
        char linea[256];
        int cantidad = 0, ultimo_poke = 0, ultimo_ent = 0;
        valido = temporal.pendientes.inicio != NULL &&
            leer_linea_archivo(archivo, linea, sizeof(linea)) &&
            convertir_entero(linea, 0, ENTRENADORES_TORNEO, &cantidad);
        for (int i = 0; valido && i < cantidad; i++) {
            entrenador ent = {0};
            char tokens[6][32], extra;
            valido = leer_linea_archivo(archivo, linea, sizeof(linea)) &&
                sscanf(linea, "%31s %31s %c", tokens[0], tokens[1], &extra) == 2 &&
                convertir_entero(tokens[0], 1, INT_MAX, &ent.id) &&
                convertir_entero(tokens[1], 0, MAX_EQUIPO, &ent.n_pokemon) &&
                leer_linea_archivo(archivo, ent.nombre, sizeof(ent.nombre));
            if (valido && ent.id > ultimo_ent) ultimo_ent = ent.id;
            for (int j = 0; valido && j < ent.n_pokemon; j++) {
                pokemon *p = &ent.equipo[j];
                valido = leer_linea_archivo(archivo, linea, sizeof(linea)) &&
                    sscanf(linea, "%31s %31s %31s %31s %31s %31s %c",
                        tokens[0], tokens[1], tokens[2], tokens[3], tokens[4],
                        tokens[5], &extra) == 6 &&
                    convertir_entero(tokens[0], 1, INT_MAX, &p->id) &&
                    convertir_entero(tokens[1], 1, 100, &p->nivel) &&
                    convertir_entero(tokens[2], 1, INT_MAX, &p->hp_max) &&
                    convertir_entero(tokens[3], 1, INT_MAX, &p->ataque) &&
                    convertir_entero(tokens[4], 1, INT_MAX, &p->defensa) &&
                    convertir_entero(tokens[5], 1, INT_MAX, &p->velocidad) &&
                    leer_linea_archivo(archivo, p->especie, sizeof(p->especie)) &&
                    leer_linea_archivo(archivo, p->nombre, sizeof(p->nombre));
                if (valido) {
                    const pokedata *base = buscar_nombre(p->especie);
                    valido = base != NULL;
                    if (valido) {
                        strcpy(p->tipo_primario, base->tipo_primario);
                        strcpy(p->tipo_secundario, base->tipo_secundario);
                        strcpy(p->estado, "normal");
                        p->hp = p->hp_max;
                        if (p->id > ultimo_poke) ultimo_poke = p->id;
                    }
                }
            }
            if (valido) valido = torneo_registrar(&temporal, &ent);
        }
        if (valido) {
            memcpy(destino->entrenadores, temporal.entrenadores,
                sizeof(destino->entrenadores));
            destino->cantidad = temporal.cantidad;
            *ultimo_pokemon = ultimo_poke;
            *ultimo_entrenador = ultimo_ent;
        }
        torneo_destruir(&temporal);
    }
    return valido;
}

int torneo_cargar_entrenadores(torneo *liga, const char *ruta) {
    int valido = liga != NULL && ruta != NULL;
    if (valido) valido = !liga->iniciado && liga->cantidad == 0;
    FILE *archivo = valido ? fopen(ruta, "r") : NULL;
    valido = archivo != NULL;
    if (valido) {
        torneo temporal;
        torneo_crear(&temporal);
        int ultimo_pokemon = 0, ultimo_entrenador = 0;
        valido = temporal.pendientes.inicio != NULL &&
            leer_entrenadores_archivo(&temporal, archivo,
                &ultimo_pokemon, &ultimo_entrenador);
        int siguiente = valido ? fgetc(archivo) : EOF;
        valido = valido && siguiente == EOF && !ferror(archivo);
        fclose(archivo);
        if (valido) {
            memcpy(liga->entrenadores, temporal.entrenadores,
                sizeof(liga->entrenadores));
            liga->cantidad = temporal.cantidad;
            actualizar_ids_restaurados(ultimo_pokemon, ultimo_entrenador);
        }
        torneo_destruir(&temporal);
    }
    return valido;
}


static int leer_etiqueta(FILE *archivo, const char *esperada) {
    char linea[80];
    int valido = leer_linea(archivo, linea, sizeof(linea)) == 1 &&
        strcmp(linea, esperada) == 0;
    return valido;
}

static int guardar_partida_archivo(const torneo *liga, int jugador, FILE *archivo) {
    int valido = liga != NULL && archivo != NULL && jugador >= 0 &&
        (jugador == 0 || torneo_indice_entrenador(liga, jugador) >= 0);
    if (valido) {
        int cantidad = cantidad_resultados(liga);
        valido = fprintf(archivo, "POKIMON_SAVE 1\nJUGADOR %d\nINICIADO %d\nENTRENADORES\n",
            jugador, liga->iniciado) > 0 &&
            escribir_entrenadores_archivo(liga, archivo) &&
            fprintf(archivo, "RESULTADOS\n%d\n", cantidad) > 0;
        if (valido && liga->iniciado) valido = escribir_resultados_archivo(liga, archivo);
        if (valido) valido = fputs("FIN_PARTIDA\n", archivo) >= 0 && !ferror(archivo);
    }
    return valido;
}

static char *crear_ruta_sufijo(const char *ruta, const char *sufijo) {
    char *resultado = NULL;
    if (ruta != NULL && sufijo != NULL) {
        size_t largo_ruta = strlen(ruta), largo_sufijo = strlen(sufijo);
        if (largo_ruta <= SIZE_MAX - largo_sufijo - 1) {
            resultado = malloc(largo_ruta + largo_sufijo + 1);
            if (resultado != NULL) {
                memcpy(resultado, ruta, largo_ruta);
                memcpy(resultado + largo_ruta, sufijo, largo_sufijo + 1);
            }
        }
    }
    return resultado;
}

// Cambia el archivo solo despues de escribir y cerrar el temporal. Si hay una
// partida anterior, primero conserva una copia recuperable con extension .bak.
static int reemplazar_partida(const char *ruta, const char *temporal) {
    int correcto = ruta != NULL && temporal != NULL;
    char *respaldo = correcto ? crear_ruta_sufijo(ruta, ".bak") : NULL;
    if (correcto) correcto = respaldo != NULL;
    int respaldo_creado = 0;
    if (correcto) {
        struct stat datos_respaldo;
        errno = 0;
        correcto = stat(respaldo, &datos_respaldo) != 0 && errno == ENOENT;
    }
    FILE *anterior = correcto ? fopen(ruta, "r") : NULL;
    if (anterior != NULL) {
        fgetc(anterior);
        correcto = !ferror(anterior);
        correcto = fclose(anterior) == 0 && correcto;
    }
    if (correcto) {
        errno = 0;
        respaldo_creado = rename(ruta, respaldo) == 0;
        if (!respaldo_creado && errno != ENOENT) correcto = 0;
    }
    if (correcto) correcto = rename(temporal, ruta) == 0;
    if (!correcto && respaldo_creado) rename(respaldo, ruta);
    if (correcto && respaldo_creado) remove(respaldo);
    if (respaldo != NULL) free(respaldo);
    return correcto;
}

int torneo_guardar_partida(const torneo *liga, int jugador, const char *ruta) {
    int valido = liga != NULL && ruta != NULL && jugador >= 0 &&
        (jugador == 0 || torneo_indice_entrenador(liga, jugador) >= 0);
    char *temporal = valido ? crear_ruta_sufijo(ruta, ".tmp") : NULL;
    if (valido) valido = temporal != NULL;
    FILE *archivo = valido ? fopen(temporal, "w") : NULL;
    valido = archivo != NULL;
    if (valido) {
        valido = guardar_partida_archivo(liga, jugador, archivo);
        valido = fclose(archivo) == 0 && valido;
    }
    if (valido) valido = reemplazar_partida(ruta, temporal);
    if (!valido && temporal != NULL) remove(temporal);
    if (temporal != NULL) free(temporal);
    return valido;
}

static int leer_cabecera_partida(FILE *archivo, int *jugador, int *iniciado) {
    char linea[80], etiqueta[32], valor[32], extra;
    int version, valido = archivo != NULL && jugador != NULL && iniciado != NULL;
    if (valido) valido = leer_linea(archivo, linea, sizeof(linea)) == 1 &&
        sscanf(linea, "%31s %31s %c", etiqueta, valor, &extra) == 2 &&
        strcmp(etiqueta, "POKIMON_SAVE") == 0 &&
        convertir_entero(valor, 1, 1, &version);
    if (valido) valido = leer_linea(archivo, linea, sizeof(linea)) == 1 &&
        sscanf(linea, "%31s %31s %c", etiqueta, valor, &extra) == 2 &&
        strcmp(etiqueta, "JUGADOR") == 0 &&
        convertir_entero(valor, 0, INT_MAX, jugador);
    if (valido) valido = leer_linea(archivo, linea, sizeof(linea)) == 1 &&
        sscanf(linea, "%31s %31s %c", etiqueta, valor, &extra) == 2 &&
        strcmp(etiqueta, "INICIADO") == 0 &&
        convertir_entero(valor, 0, 1, iniciado);
    return valido;
}

int torneo_cargar_partida(torneo *liga, int *jugador, const char *ruta,
    const char *ruta_cruces) {
    int valido = liga != NULL && jugador != NULL && ruta != NULL && ruta_cruces != NULL;
    FILE *archivo = valido ? fopen(ruta, "r") : NULL;
    valido = archivo != NULL;
    if (valido) {
        torneo temporal;
        torneo_crear(&temporal);
        int jugador_leido = -1, iniciado = 0;
        int ultimo_pokemon = 0, ultimo_entrenador = 0;
        int cantidad_resultados_leidos = 0;
        valido = temporal.pendientes.inicio != NULL &&
            leer_cabecera_partida(archivo, &jugador_leido, &iniciado) &&
            leer_etiqueta(archivo, "ENTRENADORES") &&
            leer_entrenadores_archivo(&temporal, archivo,
                &ultimo_pokemon, &ultimo_entrenador) &&
            leer_etiqueta(archivo, "RESULTADOS");
        char linea[80];
        if (valido) valido = leer_linea(archivo, linea, sizeof(linea)) == 1 &&
            convertir_entero(linea, 0, COMBATES_TORNEO, &cantidad_resultados_leidos);
        if (valido && iniciado) valido = torneo_iniciar(&temporal, ruta_cruces);
        if (valido && !iniciado) valido = cantidad_resultados_leidos == 0;
        if (valido && jugador_leido > 0)
            valido = torneo_indice_entrenador(&temporal, jugador_leido) >= 0;
        if (valido && cantidad_resultados_leidos > 0)
            valido = aplicar_resultados_archivo(&temporal, archivo,
                cantidad_resultados_leidos, 0);
        if (valido) valido = leer_etiqueta(archivo, "FIN_PARTIDA");
        if (valido) {
            int siguiente = fgetc(archivo);
            valido = siguiente == EOF && !ferror(archivo);
        }
        valido = fclose(archivo) == 0 && valido;
        if (valido) {
            torneo_destruir(liga);
            *liga = temporal;
            *jugador = jugador_leido;
            actualizar_ids_restaurados(ultimo_pokemon, ultimo_entrenador);
        } else torneo_destruir(&temporal);
    }
    return valido;
}
