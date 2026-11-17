#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "combate.h"
#include "efectividad.h"
#include "entrada.h"
#include "utils.h"

#define MAX_DECISIONES (8 + 1 + 3 + MAX_EQUIPO - 1)
#define PROFUNDIDAD_BOT 2

typedef enum { ATACAR, BLOQUEAR, AUMENTAR, CAMBIAR } accion;
typedef struct { int estadistica; int usos; } aumento;
typedef struct {
    accion accion;
    int tipo; // 0 primario, 1 secundario
    int potencia;
    int destino;
    int estadistica;
} decision;

// La ficha original conserva sus stats. HP, aumentos y reservas viven aqui
// durante la pelea, asi el bot puede probar futuros sin editar al Pokemon real xd.
typedef struct {
    int hp[MAX_EQUIPO];
    aumento aumentos[MAX_EQUIPO];
    int usados[MAX_EQUIPO][3];
    int activo;
    int puede_bloquear;
} lado_combate;
typedef struct {
    const entrenador *ent[2];
    lado_combate lados[2];
    int emergencia;
} posicion_combate;


// Traduce el indice compartido por los aumentos: 0 ataque, 1 defensa, 2 velocidad
static const char *nombre_estadistica(int indice) {
    const char *nombre = "Velocidad";
    if (indice == 0) nombre = "Ataque";
    else if (indice == 1) nombre = "Defensa";
    return nombre;
}

static const char *nombre_pokemon(const pokemon *poke) {
    const char *nombre = "Pokemon sin nombre";
    if (poke->nombre[0] != '\0') nombre = poke->nombre;
    else if (poke->especie[0] != '\0') nombre = poke->especie;
    return nombre;
}

int potencia_desbloqueada(int nivel, int potencia) {
    return nivel >= 1 && nivel <= 100 &&
        (potencia == 25 || ((potencia == 50 || potencia == 75 || potencia == 100) && nivel >= potencia));
}

// repartimos un 10% segun las velocidades entre iguales toca 5%
// el porcentaje se trunca a entero y usa las velocidades con aumentos aplicados
int probabilidad_esquiva(int atacante, int defensor) {
    int probabilidad = 0;
    if (atacante >= 0 && defensor > 0) {
        probabilidad = (int)(10.0 * defensor / ((double)atacante + defensor));
    }
    return probabilidad;
}

// Base de Rojo/Azul: descartar fracciones al dividir nivel, defensa y 50;
// sumar 2 antes de la efectividad. El sorteo se aplica despues, al ejecutar.
// Los productos usan double para no desbordar int; floor conserva los pasos
// enteros. Minimo 1 salvo inmunidad: ser resistente no te hace inmortal xd.
static int calcular_formula(const pokemon *atacante, const pokemon *defensor,
    double efectividad, int potencia, int *dano) {
    int valido = atacante != NULL && defensor != NULL && dano != NULL &&
        (potencia == 25 || potencia == 50 || potencia == 75 || potencia == 100);
    if (valido) {
        valido = atacante->nivel >= 1 && atacante->nivel <= 100 &&
            atacante->ataque >= 0 && defensor->defensa > 0 &&
            isfinite(efectividad) && efectividad >= 0;
        if (valido) {
            int factor_nivel = 2 * atacante->nivel / 5 + 2;
            double base = floor(((double)factor_nivel * potencia * atacante->ataque) /
                defensor->defensa);
            base = floor(base / 50.0) + 2;
            double valor = base * efectividad;
            valido = isfinite(valor);
            if (valido) {
                if (efectividad == 0) *dano = 0;
                else if (valor >= INT_MAX) *dano = INT_MAX;
                else *dano = valor < 1 ? 1 : (int)valor;
            }
        }
    }
    return valido;
}

int calcular_dano_con_potencia(const pokemon *atacante, const pokemon *defensor,
    double efectividad, int potencia, int *dano) {
    int valido = atacante != NULL && potencia_desbloqueada(atacante->nivel, potencia);
    if (valido) valido = calcular_formula(atacante, defensor, efectividad, potencia, dano);
    return valido;
}

int variar_dano_combate(int dano, uint32_t *semilla) {
    int resultado = dano;
    if (dano > 1 && semilla != NULL) {
        uint32_t factor = (*semilla = alterador_semilla(*semilla)) % 39U + 217U;
        resultado = (int)((unsigned long long)(unsigned int)dano * factor / 255U);
        if (resultado < 1) resultado = 1;
    }
    return resultado;
}

const char *elegir_tipo_ataque_automatico(const pokemon *atacante, const pokemon *defensor) {
    const char *tipo = NULL;
    double primario, secundario;
    if (atacante != NULL && defensor != NULL &&
        calcular_efectividad(atacante, atacante->tipo_primario, defensor, &primario)) {
        tipo = atacante->tipo_primario;
        if (atacante->tipo_secundario[0] != '\0' && strcmp(atacante->tipo_secundario, "-") != 0) {
            if (!calcular_efectividad(atacante, atacante->tipo_secundario, defensor, &secundario)) tipo = NULL;
            else if (secundario > primario) tipo = atacante->tipo_secundario;
        }
    }
    return tipo;
}

static int validar_equipo(const entrenador *ent) {
    int valido = ent != NULL;
    if (valido) valido = ent->n_pokemon >= 1 && ent->n_pokemon <= MAX_EQUIPO;
    for (int i = 0; valido && i < ent->n_pokemon; i++) {
        const pokemon *p = &ent->equipo[i];
        valido = memchr(p->tipo_primario, '\0', sizeof(p->tipo_primario)) != NULL &&
            memchr(p->tipo_secundario, '\0', sizeof(p->tipo_secundario)) != NULL &&
            memchr(p->nombre, '\0', sizeof(p->nombre)) != NULL &&
            memchr(p->especie, '\0', sizeof(p->especie)) != NULL &&
            p->nivel >= 1 && p->nivel <= 100 && p->hp_max > 0 &&
            p->hp >= 0 && p->hp <= p->hp_max && p->ataque >= 0 &&
            p->defensa > 0 && p->velocidad >= 0;
        if (valido) valido = es_tipo_pokemon_valido(p->tipo_primario) &&
            (p->tipo_secundario[0] == '\0' || strcmp(p->tipo_secundario, "-") == 0 ||
             es_tipo_pokemon_valido(p->tipo_secundario));
    }
    return valido;
}

static int cantidad_vivos(const posicion_combate *pos, int lado) {
    int vivos = 0;
    for (int i = 0; i < pos->ent[lado]->n_pokemon; i++) vivos += pos->lados[lado].hp[i] > 0;
    return vivos;
}

// mira todos los ejemplares vivos, tambien los del banco, en ambas direcciones
// si no hay ninguno que pueda hacer daño, el combate termina en empate
static int hay_dano_posible(const posicion_combate *pos) {
    int hay = 0;
    for (int lado = 0; lado < 2 && !hay; lado++) {
        for (int i = 0; i < pos->ent[lado]->n_pokemon && !hay; i++) {
            for (int j = 0; j < pos->ent[1 - lado]->n_pokemon && !hay; j++) {
                if (pos->lados[lado].hp[i] > 0 && pos->lados[1 - lado].hp[j] > 0) {
                    const pokemon *a = &pos->ent[lado]->equipo[i];
                    const pokemon *d = &pos->ent[1 - lado]->equipo[j];
                    const char *tipo = elegir_tipo_ataque_automatico(a, d);
                    double efectividad;
                    hay = tipo != NULL && calcular_efectividad(a, tipo, d, &efectividad) && efectividad > 0;
                }
            }
        }
    }
    return hay;
}

// Arma una copia del activo con HP temporal y aumento sobre la stat original
static pokemon pokemon_actual(const posicion_combate *pos, int lado) {
    int activo = pos->lados[lado].activo;
    pokemon p = pos->ent[lado]->equipo[activo];
    aumento a = pos->lados[lado].aumentos[activo];
    p.hp = pos->lados[lado].hp[activo];
    int *estadistica = &p.ataque;
    if (a.estadistica == 1) estadistica = &p.defensa;
    else if (a.estadistica == 2) estadistica = &p.velocidad;
    double valor = *estadistica * (1.0 + 0.25 * a.usos);
    *estadistica = valor >= INT_MAX ? INT_MAX : (int)valor;
    return p;
}

// Solo las potencias 50/75/100 tienen reserva. El 0 de las demas no significa
// que potencia 25 este agotada: ataque_disponible la trata como ilimitada.
static int usos_maximos(int potencia) {
    int usos = 0;
    if (potencia == 50) usos = 4;
    else if (potencia == 75) usos = 2;
    else if (potencia == 100) usos = 1;
    return usos;
}

static int ataque_disponible(const posicion_combate *pos, int lado, int potencia) {
    int activo = pos->lados[lado].activo;
    int disponible = potencia_desbloqueada(pos->ent[lado]->equipo[activo].nivel, potencia);
    if (disponible && potencia > 25) {
        // 50/75/100 -> columnas 0/1/2. Los dos tipos gastan la misma reserva del ejemplar
        disponible = pos->lados[lado].usados[activo][potencia / 25 - 2] < usos_maximos(potencia);
    }
    return disponible;
}

// Consulta el daño de un ataque sin ejecutarlo ni gastar reservas.
static int dano_decision(const posicion_combate *pos, int lado, const decision *d) {
    pokemon atacante = pokemon_actual(pos, lado);
    pokemon defensor = pokemon_actual(pos, 1 - lado);
    const char *tipo = d->tipo == 0 ? atacante.tipo_primario : atacante.tipo_secundario;
    double efectividad;
    int dano = 0;
    if (d->tipo == 2) {
        calcular_dano_con_potencia(&atacante, &defensor, 1.0, 25, &dano);
    } else if (calcular_efectividad(&atacante, tipo, &defensor, &efectividad)) {
        calcular_dano_con_potencia(&atacante, &defensor, efectividad, d->potencia, &dano);
    }
    return dano;
}

// Enumera las acciones legales segun nivel, reservas, aumentos y compañeros vivos.
// El jugador y el bot reciben las mismas opciones; aqui todavia no se elige ninguna.
static int generar_decisiones(const posicion_combate *pos, int lado, decision opciones[]) {
    int cantidad = 0;
    int activo = pos->lados[lado].activo;
    const pokemon *p = &pos->ent[lado]->equipo[activo];
    int tipos = p->tipo_secundario[0] == '\0' || strcmp(p->tipo_secundario, "-") == 0 ? 1 : 2;
    if (pos->emergencia) {
        opciones[cantidad++] = (decision){ATACAR, 2, 25, -1, 0};
        tipos = 0;
    }
    for (int potencia = 25; potencia <= 100; potencia += 25) {
        for (int tipo = 0; tipo < tipos; tipo++) {
            if (ataque_disponible(pos, lado, potencia)) {
                opciones[cantidad++] = (decision){ATACAR, tipo, potencia, -1, 0};
            }
        }
    }
    if (pos->lados[lado].puede_bloquear) opciones[cantidad++] = (decision){BLOQUEAR, 0, 0, -1, 0};
    aumento a = pos->lados[lado].aumentos[activo];
    if (a.usos < 2) {
        int valores[] = {p->ataque, p->defensa, p->velocidad};
        int mayor = valores[0];
        for (int i = 1; i < 3; i++) if (valores[i] > mayor) mayor = valores[i];
        for (int i = 0; i < 3; i++) {
            if ((a.usos > 0 && i == a.estadistica) || (a.usos == 0 && valores[i] == mayor)) {
                opciones[cantidad++] = (decision){AUMENTAR, 0, 0, -1, i};
            }
        }
    }
    for (int i = 0; i < pos->ent[lado]->n_pokemon; i++) {
        if (i != activo && pos->lados[lado].hp[i] > 0) opciones[cantidad++] = (decision){CAMBIAR, 0, 0, i, 0};
    }
    return cantidad;
}

// Elige el mayor daño inmediato entre ataques legales, sin anticipar otra ronda.
// Si varios hacen lo mismo conserva el primero, no gasta mas potencia por presumir xd.
static decision ataque_directo(const posicion_combate *pos, int lado) {
    decision opciones[MAX_DECISIONES];
    int cantidad = generar_decisiones(pos, lado, opciones);
    decision mejor = opciones[0];
    int dano_mayor = -1;
    for (int i = 0; i < cantidad; i++) {
        if (opciones[i].accion == ATACAR) {
            int dano = dano_decision(pos, lado, &opciones[i]);
            if (dano > dano_mayor) {
                dano_mayor = dano;
                mejor = opciones[i];
            }
        }
    }
    return mejor;
}

static uint32_t aleatorio(uint32_t *semilla) {
    *semilla = alterador_semilla(*semilla);
    return *semilla;
}

static void imprimir_barra_hp(int hp, int hp_max) {
    const int ancho = 20;
    int llenos = hp_max > 0 ? (int)((double)hp / hp_max * ancho) : 0;
    if (llenos < 0) llenos = 0;
    if (llenos > ancho) llenos = ancho;
    putchar('[');
    for (int i = 0; i < ancho; i++) putchar(i < llenos ? '#' : '-');
    printf("] %d/%d\n", hp, hp_max);
}

static void imprimir_estado_equipo(const posicion_combate *pos, int lado) {
    printf("%s\n", pos->ent[lado]->nombre);
    for (int i = 0; i < pos->ent[lado]->n_pokemon; i++) {
        const pokemon *poke = &pos->ent[lado]->equipo[i];
        const char *marca = pos->lados[lado].hp[i] == 0 ? "[DEBILITADO]" :
            pos->lados[lado].activo == i ? "[ACTIVO]" : "";
        printf("  %-18s %-12s ", nombre_pokemon(poke), marca);
        imprimir_barra_hp(pos->lados[lado].hp[i], poke->hp_max);
    }
}

static void imprimir_estado_combate(const posicion_combate *pos, int ronda) {
    puts("\n============================================================");
    printf("ESTADO DEL COMBATE | RONDA %d\n", ronda);
    imprimir_estado_equipo(pos, 0);
    imprimir_estado_equipo(pos, 1);
    puts("------------------------------------------------------------");
}

// Cambiar borra el aumento, pero no imprime nuevas reservas de ataques xd.
static void cambiar(posicion_combate *pos, int lado, int destino) {
    lado_combate *l = &pos->lados[lado];
    l->aumentos[l->activo] = (aumento){0};
    l->activo = destino;
    l->aumentos[destino] = (aumento){0};
}

// Respaldo cuando el bot no encuentra un KO: mayor daño inmediato; si es cero,
// busca el primer compañero vivo que pueda afectar al rival y propone cambiar.
static decision decision_por_defecto(const posicion_combate *pos, int lado) {
    decision elegida = ataque_directo(pos, lado);
    if (dano_decision(pos, lado, &elegida) == 0) {
        int encontrado = 0;
        for (int i = 0; i < pos->ent[lado]->n_pokemon && !encontrado; i++) {
            if (i != pos->lados[lado].activo && pos->lados[lado].hp[i] > 0) {
                posicion_combate alternativa = *pos;
                cambiar(&alternativa, lado, i);
                decision ataque = ataque_directo(&alternativa, lado);
                if (dano_decision(&alternativa, lado, &ataque) > 0) {
                    elegida = (decision){CAMBIAR, 0, 0, i, 0};
                    encontrado = 1;
                }
            }
        }
    }
    return elegida;
}

// Aplica ataque o aumento. Los cambios y bloqueos ya los organizo resolver_ronda.
// Un ataque ejecutado gasta su uso aunque se bloquee, esquive o encuentre inmunidad;
// si el atacante cae antes de actuar, esta funcion ni se llama para el. La variacion
// se sortea aqui en combate real; el backtracking conserva el daño base determinista.
static void ejecutar(posicion_combate *pos, int lado, const decision *d,
    int bloqueado, int esquiva, uint32_t *semilla, resumen_combate *resumen,
    int mostrar, int busqueda) {
    lado_combate *l = &pos->lados[lado];
    lado_combate *rival = &pos->lados[1 - lado];
    if (d->accion == AUMENTAR) {
        pokemon antes = pokemon_actual(pos, lado);
        l->aumentos[l->activo].estadistica = d->estadistica;
        l->aumentos[l->activo].usos++;
        if (mostrar) {
            pokemon despues = pokemon_actual(pos, lado);
            int valores_antes[] = {antes.ataque, antes.defensa, antes.velocidad};
            int valores_despues[] = {despues.ataque, despues.defensa, despues.velocidad};
            printf("%s aumenta %s de %d a %d (x%.2f).\n", nombre_pokemon(&antes),
                nombre_estadistica(d->estadistica), valores_antes[d->estadistica],
                valores_despues[d->estadistica], 1.0 + 0.25 * l->aumentos[l->activo].usos);
        }
    } else if (d->accion == ATACAR) {
        pokemon atacante = pokemon_actual(pos, lado);
        pokemon defensor = pokemon_actual(pos, 1 - lado);
        if (d->potencia > 25) {
            int indice = d->potencia / 25 - 2;
            l->usados[l->activo][indice]++;
            resumen->usos_potencia[lado][indice]++;
        }
        resumen->ataques[lado]++;
        int evita = 0;
        if (!bloqueado && esquiva) {
            evita = (int)(aleatorio(semilla) % 100) < probabilidad_esquiva(atacante.velocidad, defensor.velocidad);
        }
        int dano = 0;
        int hp_antes = rival->hp[rival->activo];
        if (bloqueado) resumen->bloqueos[1 - lado]++;
        else if (evita) resumen->esquivas[1 - lado]++;
        else {
            dano = dano_decision(pos, lado, d);
            if (!busqueda) dano = variar_dano_combate(dano, semilla);
        }
        int dano_recibido = dano > hp_antes ? hp_antes : dano;
        rival->hp[rival->activo] = hp_antes - dano_recibido;
        const char *estado = bloqueado ? " (bloqueado)" : evita ? " (esquivado)" :
            dano == 0 ? " (sin efecto)" : rival->hp[rival->activo] == 0 ? " (KO)" : "";
        if (mostrar) printf("%s ataca con %s, potencia %d. %s recibe %d de dano%s.\n",
            nombre_pokemon(&atacante), d->tipo == 2 ? "neutral de emergencia" :
                d->tipo == 0 ? atacante.tipo_primario : atacante.tipo_secundario,
            d->potencia, nombre_pokemon(&defensor), dano_recibido, estado);
        if (mostrar) {
            if (bloqueado) printf("%s bloquea el ataque y evita el dano.\n", nombre_pokemon(&defensor));
            else if (evita) printf("%s esquiva el ataque.\n", nombre_pokemon(&defensor));
            else if (dano == 0) printf("El ataque no tiene efecto sobre %s.\n", nombre_pokemon(&defensor));
            if (rival->hp[rival->activo] == 0) printf("%s se ha debilitado.\n", nombre_pokemon(&defensor));
        }
    }
}

// Motor unico para juego y busqueda. En la busqueda no sorteamos el futuro:
// asumimos golpes acertados y que el rival empieza si empatan en velocidad.
static void resolver_ronda(posicion_combate *pos, const decision decisiones[2],
    int esquiva, uint32_t *semilla, resumen_combate *resumen, int mostrar, int busqueda, int lado_bot) {
    for (int lado = 0; lado < 2; lado++) {
        if (decisiones[lado].accion == CAMBIAR) {
            if (mostrar) {
                printf("%s retira a %s y envia a %s.\n", pos->ent[lado]->nombre,
                    nombre_pokemon(&pos->ent[lado]->equipo[pos->lados[lado].activo]),
                    nombre_pokemon(&pos->ent[lado]->equipo[decisiones[lado].destino]));
                if (pos->lados[lado].aumentos[pos->lados[lado].activo].usos > 0)
                    puts("Al cambiar, se pierden los aumentos de estadisticas del Pokemon que sale.");
            }
            cambiar(pos, lado, decisiones[lado].destino);
        } else if (mostrar && decisiones[lado].accion == BLOQUEAR)
            printf("%s se prepara para defenderse.\n",
                nombre_pokemon(&pos->ent[lado]->equipo[pos->lados[lado].activo]));
        pos->lados[lado].puede_bloquear = decisiones[lado].accion != BLOQUEAR;
    }
    pokemon p = pokemon_actual(pos, 0), q = pokemon_actual(pos, 1);
    int primero = p.velocidad > q.velocidad ? 0 : 1;
    if (p.velocidad == q.velocidad) primero = busqueda ? 1 - lado_bot : (int)(aleatorio(semilla) % 2);
    ejecutar(pos, primero, &decisiones[primero], decisiones[1 - primero].accion == BLOQUEAR,
        esquiva, semilla, resumen, mostrar, busqueda);
    if (pos->lados[1 - primero].hp[pos->lados[1 - primero].activo] > 0) {
        ejecutar(pos, 1 - primero, &decisiones[1 - primero], decisiones[primero].accion == BLOQUEAR,
            esquiva, semilla, resumen, mostrar, busqueda);
    } else if (mostrar && (decisiones[1 - primero].accion == ATACAR ||
                          decisiones[1 - primero].accion == AUMENTAR)) {
        printf("%s no llega a ejecutar su movimiento porque se ha debilitado.\n",
            nombre_pokemon(&pos->ent[1 - primero]->equipo[pos->lados[1 - primero].activo]));
    }
}

// Backtracking: probar una accion, resolver una ronda y explorar la siguiente.
// Exito = activo propio vivo y activo rival derrotado, no ganar todo el equipo.
// Cada rama copia el estado temporal; al descartarla, sus gastos desaparecen.
// El rival responde con su mayor daño inmediato y no se simulan esquivas.
static int buscar_secuencia(const posicion_combate *pos, int lado, int restantes,
    decision *primera, unsigned long *nodos) {
    int encontrado = 0;
    decision opciones[MAX_DECISIONES];
    int cantidad = generar_decisiones(pos, lado, opciones);
    int i = 0;
    while (!encontrado && i < cantidad) {
        posicion_combate siguiente = *pos; // solo HP, reservas y aumentos; las fichas se comparten
        decision ronda[2];
        ronda[lado] = opciones[i];
        ronda[1 - lado] = ataque_directo(pos, 1 - lado);
        resumen_combate descartado = {0};
        uint32_t semilla = 1;
        (*nodos)++;
        resolver_ronda(&siguiente, ronda, 0, &semilla, &descartado, 0, 1, lado);
        int vivo = siguiente.lados[lado].hp[siguiente.lados[lado].activo] > 0;
        int rival_vivo = siguiente.lados[1 - lado].hp[siguiente.lados[1 - lado].activo] > 0;
        if (vivo && !rival_vivo) encontrado = 1;
        else if (vivo && restantes > 1) {
            decision continuacion;
            encontrado = buscar_secuencia(&siguiente, lado, restantes - 1, &continuacion, nodos);
        }
        if (encontrado) *primera = opciones[i];
        i++;
    }
    return encontrado;
}

// Busca primero un KO en una ronda y luego en dos. Conserva la primera secuencia
// que cumple el objetivo
static decision elegir_bot(const posicion_combate *pos, int lado, unsigned long *nodos) {
    decision elegida = decision_por_defecto(pos, lado);
    int encontrada = 0;
    for (int profundidad = 1; !encontrada && profundidad <= PROFUNDIDAD_BOT; profundidad++) {
        encontrada = buscar_secuencia(pos, lado, profundidad, &elegida, nodos);
    }

    return elegida;
}

static int seleccionar_pokemon(posicion_combate *pos, int lado, int automatico, int mostrar) {
    int elegido = -1;
    int leyendo = 1;
    if (automatico) {
        int i = 0;
        while (elegido == -1 && i < pos->ent[lado]->n_pokemon) {
            if (pos->lados[lado].hp[i] > 0) elegido = i;
            i++;
        }
    } else {
        while (elegido == -1 && leyendo) {
            puts("\nEquipo disponible:");
            for (int i = 0; i < pos->ent[lado]->n_pokemon; i++) {
                const char *estado = pos->lados[lado].hp[i] == 0 ? " [DEBILITADO]" : "";
                printf("  %d. %-18s HP %d/%d%s\n", i + 1,
                    nombre_pokemon(&pos->ent[lado]->equipo[i]), pos->lados[lado].hp[i],
                    pos->ent[lado]->equipo[i].hp_max, estado);
            }
            int opcion;
            leyendo = leer_opcion("Elige tu Pokemon: ", 1, pos->ent[lado]->n_pokemon, &opcion);
            if (leyendo && pos->lados[lado].hp[opcion - 1] > 0) elegido = opcion - 1;
        }
    }
    if (elegido >= 0) {
        cambiar(pos, lado, elegido);
        if (mostrar) printf("%s envia a %s.\n", pos->ent[lado]->nombre, nombre_pokemon(&pos->ent[lado]->equipo[elegido]));
    }
    return elegido >= 0;
}

static const char *nombre_accion(accion a) {
    const char *nombre = "Cambiar";
    switch (a) {
        case ATACAR: nombre = "Atacar"; break;
        case BLOQUEAR: nombre = "Bloquear"; break;
        case AUMENTAR: nombre = "Aumentar estadistica"; break;
        case CAMBIAR: nombre = "Cambiar"; break;
    }
    return nombre;
}

static const char *nombre_tipo_ataque(const pokemon *p, int tipo) {
    const char *nombre = "neutral de emergencia";
    if (tipo == 0) nombre = p->tipo_primario;
    else if (tipo == 1) nombre = p->tipo_secundario;
    return nombre;
}

// Filtra las decisiones YA legales del motor; el menu no inventa reglas paralelas.
// Devuelve 1 al elegir, 0 al volver y -1 ante EOF. Volver no ejecuta nada xd.
static int elegir_detalle(const posicion_combate *pos, int lado,
    const decision opciones[], int cantidad, accion accion_elegida, int tipo,
    decision *elegida) {
    int indices[MAX_DECISIONES];
    int disponibles = 0;
    for (int i = 0; i < cantidad; i++) {
        decision d = opciones[i];
        if (d.accion == accion_elegida && (accion_elegida != ATACAR || d.tipo == tipo)) {
            indices[disponibles++] = i;
            printf("%d. ", disponibles);
            if (d.accion == ATACAR) {
                printf("Potencia %d", d.potencia);
                if (d.potencia == 25) printf(" (ilimitado)");
                else printf(" (%d usos restantes)", usos_maximos(d.potencia) -
                    pos->lados[lado].usados[pos->lados[lado].activo][d.potencia / 25 - 2]);
            } else if (d.accion == CAMBIAR) {
                const pokemon *p = &pos->ent[lado]->equipo[d.destino];
                printf("%s | Nivel %d | HP %d/%d", nombre_pokemon(p), p->nivel,
                    pos->lados[lado].hp[d.destino], p->hp_max);
            } else printf("%s (+25%% de la estadistica original)", nombre_estadistica(d.estadistica));
            puts("");
        }
    }
    puts("0. Volver");
    const char *prompt = accion_elegida == ATACAR ? "Elige potencia: " :
        accion_elegida == CAMBIAR ? "Elige reemplazo: " : "Elige estadistica: ";
    int opcion = 0;
    int resultado = leer_opcion(prompt, 0, disponibles, &opcion) ? 0 : -1;
    if (resultado == 0 && opcion > 0) {
        *elegida = opciones[indices[opcion - 1]];
        resultado = 1;
    }
    return resultado;
}

// Conserva el tipo elegido mientras vuelves de potencias; volver otra vez lleva
// a las acciones. Las reservas se descuentan al ejecutar, nunca al mirar el menu.
static int elegir_ataque(const posicion_combate *pos, int lado,
    const decision opciones[], int cantidad, decision *elegida) {
    pokemon p = pokemon_actual(pos, lado);
    int tipos[3], cantidad_tipos = 0;
    for (int tipo = 0; tipo < 3; tipo++) {
        int existe = 0;
        for (int i = 0; i < cantidad; i++) {
            if (opciones[i].accion == ATACAR && opciones[i].tipo == tipo) existe = 1;
        }
        if (existe) tipos[cantidad_tipos++] = tipo;
    }
    int resultado = 0, volver = 0;
    while (resultado == 0 && !volver) {
        puts("\nATACAR | Tipo de ataque");
        for (int i = 0; i < cantidad_tipos; i++)
            printf("%d. %s\n", i + 1, nombre_tipo_ataque(&p, tipos[i]));
        puts("0. Volver");
        int opcion = 0;
        if (!leer_opcion("Elige tipo: ", 0, cantidad_tipos, &opcion)) resultado = -1;
        else if (opcion == 0) volver = 1;
        else {
            printf("\nPOTENCIAS | %s | Nivel %d\n", nombre_tipo_ataque(&p, tipos[opcion - 1]), p.nivel);
            puts("Solo aparecen potencias desbloqueadas con usos disponibles.");
            resultado = elegir_detalle(pos, lado, opciones, cantidad, ATACAR,
                tipos[opcion - 1], elegida);
        }
    }
    return resultado;
}

static int elegir_jugador(const posicion_combate *pos, int lado, decision *elegida) {
    decision opciones[MAX_DECISIONES];
    int cantidad = generar_decisiones(pos, lado, opciones);
    int disponibles[4] = {0};
    for (int i = 0; i < cantidad; i++) disponibles[opciones[i].accion]++;
    pokemon p = pokemon_actual(pos, lado);
    int resultado = 0;
    while (resultado == 0) {
        printf("\nENTRENADOR: %s\nACTIVO: %s | Nivel %d\n",
            pos->ent[lado]->nombre, nombre_pokemon(&p), p.nivel);
        printf("HP %d/%d | Ataque %d | Defensa %d | Velocidad %d\n",
            p.hp, p.hp_max, p.ataque, p.defensa, p.velocidad);
        puts("ACCIONES DEL TURNO");
        for (int i = 0; i < 4; i++)
            printf("%d. %s%s\n", i + 1, nombre_accion((accion)i), disponibles[i] ? "" : " (no disponible)");
        int opcion = 0;
        if (!leer_opcion("Elige tu movimiento: ", 1, 4, &opcion)) resultado = -1;
        else if (!disponibles[opcion - 1]) {
            const char *motivo = opcion == 2 ? "No puedes bloquear dos turnos seguidos." :
                opcion == 3 ? "Este Pokemon ya utilizo sus dos aumentos." :
                "No hay otro Pokemon vivo para cambiar.";
            puts(motivo);
        } else if (opcion == 1) resultado = elegir_ataque(pos, lado, opciones, cantidad, elegida);
        else if (opcion == 2) {
            *elegida = (decision){BLOQUEAR, 0, 0, -1, 0};
            resultado = 1;
        } else {
            puts(opcion == 3 ? "\nAUMENTAR ESTADISTICA" : "\nCAMBIAR POKEMON | Companeros disponibles");
            resultado = elegir_detalle(pos, lado, opciones, cantidad, (accion)(opcion - 1), -1, elegida);
        }
    }
    return resultado == 1;
}

opciones_combate opciones_combate_iniciales(modo_combate modo) {
    opciones_combate opciones = {modo, 0, 1, 1, 1000, 0, 0, 0, {0, 0}, 0};
    return opciones;
}

// Coordina una pelea nueva: HP completo, contadores de uso en cero y decisiones por ronda.
// Publica HP/estado al terminar; un error de entrada deja los equipos curados.
// Llegar al limite devuelve PENDIENTE: el reloj no reparte puntos de grupos xd.
resultado_combate combatir_configurado(entrenador *primero, entrenador *segundo,
    const opciones_combate *opciones, resumen_combate *salida) {
    resultado_combate resultado = COMBATE_ERROR;
    resumen_combate resumen = {0};
    int valido = opciones != NULL && primero != segundo && validar_equipo(primero) && validar_equipo(segundo);
    if (valido) valido = opciones->modo >= COMBATE_AUTOMATICO && opciones->modo <= COMBATE_CONTRA_BOT &&
        opciones->max_rondas > 0;
    if (valido) {
        posicion_combate pos = {.ent = {primero, segundo}};
        uint32_t semilla = opciones->semilla != 0 ? opciones->semilla : gen_semilla();
        int automatico = opciones->seleccion_automatica || opciones->modo == COMBATE_AUTOMATICO;
        int automaticos[2] = {automatico,
            automatico || opciones->modo == COMBATE_CONTRA_BOT};
        for (int lado = 0; lado < 2; lado++) {
            pos.lados[lado].puede_bloquear = 1;
            for (int i = 0; i < pos.ent[lado]->n_pokemon; i++) pos.lados[lado].hp[i] = pos.ent[lado]->equipo[i].hp_max;
        }
        valido = seleccionar_pokemon(&pos, 0, automaticos[0], opciones->mostrar);
        if (valido) valido = seleccionar_pokemon(&pos, 1, automaticos[1], opciones->mostrar);
        int dano_posible = hay_dano_posible(&pos);
        if (!dano_posible && !opciones->permitir_empate && opciones->ataque_emergencia) {
            pos.emergencia = 1;
            dano_posible = 1;
            if (opciones->mostrar) puts("Bloqueo total: se habilita ataque neutral de emergencia de potencia 25.");
        }
        int vivos_anteriores = cantidad_vivos(&pos, 0) + cantidad_vivos(&pos, 1);
        int continuar = 1;
        while (valido && continuar && dano_posible && cantidad_vivos(&pos, 0) > 0 && cantidad_vivos(&pos, 1) > 0 && resumen.rondas < opciones->max_rondas) {
            if (opciones->mostrar) imprimir_estado_combate(&pos, resumen.rondas + 1);
            decision decisiones[2];
            for (int lado = 0; valido && lado < 2; lado++) {
                if (opciones->modo == COMBATE_AUTOMATICO) decisiones[lado] = decision_por_defecto(&pos, lado);
                else if (automaticos[lado] && opciones->ataque_directo[lado]) decisiones[lado] = decision_por_defecto(&pos, lado);
                else if (automaticos[lado]) decisiones[lado] = elegir_bot(&pos, lado, &resumen.nodos_bot);
                else valido = elegir_jugador(&pos, lado, &decisiones[lado]);
            }
            if (valido) {
                if (opciones->mostrar) puts("\n-------------------- RELATO DE LA RONDA --------------------");
                resumen.rondas++;
                resolver_ronda(&pos, decisiones, opciones->usar_esquiva, &semilla, &resumen, opciones->mostrar, 0, 0);
                for (int lado = 0; valido && lado < 2; lado++) {
                    if (pos.lados[lado].hp[pos.lados[lado].activo] == 0 && cantidad_vivos(&pos, lado) > 0) {
                        valido = seleccionar_pokemon(&pos, lado, automaticos[lado], opciones->mostrar);
                    }
                }
                int vivos_actuales = cantidad_vivos(&pos, 0) + cantidad_vivos(&pos, 1);
                // La inmunidad depende de los tipos de TODOS los vivos. Cambiar de activo
                // o gastar potencia no altera esa posibilidad: solo hace falta revisar tras un KO.
                if (vivos_actuales != vivos_anteriores) {
                    dano_posible = hay_dano_posible(&pos);
                    if (!dano_posible && !opciones->permitir_empate && opciones->ataque_emergencia &&
                        cantidad_vivos(&pos, 0) > 0 && cantidad_vivos(&pos, 1) > 0) pos.emergencia = 1;
                    dano_posible = dano_posible || pos.emergencia;
                    vivos_anteriores = vivos_actuales;
                }
                if (opciones->mostrar) puts("---------------------- FIN DEL RELATO ----------------------\n");
                if (valido && opciones->mostrar && opciones->pausar_rondas &&
                    automaticos[0] && automaticos[1] && dano_posible &&
                    cantidad_vivos(&pos, 0) > 0 && cantidad_vivos(&pos, 1) > 0 &&
                    resumen.rondas < opciones->max_rondas)
                    continuar = esperar_enter("Enter: siguiente ronda | 0: volver al menu: ");
            }
        }
        if (valido) {
            resultado = COMBATE_PENDIENTE;
            resumen.sin_dano_posible = !dano_posible && cantidad_vivos(&pos, 0) > 0 && cantidad_vivos(&pos, 1) > 0;
            if (!dano_posible && opciones->permitir_empate) resultado = COMBATE_EMPATE;
            if (cantidad_vivos(&pos, 0) == 0) resultado = COMBATE_GANADOR_ENTRENADOR_2;
            else if (cantidad_vivos(&pos, 1) == 0) resultado = COMBATE_GANADOR_ENTRENADOR_1;
        }
        entrenador *equipos[] = {primero, segundo};
        for (int lado = 0; lado < 2; lado++) {
            resumen.derrotados[lado] = pos.ent[lado]->n_pokemon - cantidad_vivos(&pos, lado);
            for (int i = 0; i < equipos[lado]->n_pokemon; i++) {
                equipos[lado]->equipo[i].hp = valido ? pos.lados[lado].hp[i] : equipos[lado]->equipo[i].hp_max;
                strcpy(equipos[lado]->equipo[i].estado,
                    equipos[lado]->equipo[i].hp == 0 ? "debilitado" : "normal");
            }
        }
        if (opciones->mostrar) {
            if (resultado == COMBATE_GANADOR_ENTRENADOR_1 || resultado == COMBATE_GANADOR_ENTRENADOR_2) {
                printf("\n============================================================\nGANADOR: %s\nCombate terminado en %d rondas.\n",
                    equipos[resultado - 1]->nombre, resumen.rondas);
            } else if (resultado == COMBATE_EMPATE) {
                puts("\n============================================================\nEMPATE: ningun equipo puede causar dano.");
            } else if (resultado == COMBATE_PENDIENTE && resumen.sin_dano_posible) {
                puts("\n============================================================\nCombate pendiente: los equipos siguen vivos, pero no pueden causar dano.");
            } else if (resultado == COMBATE_PENDIENTE && !continuar) {
                puts("\nCombate detenido: sigue pendiente. Al retomarlo, el encuentro comienza de nuevo.");
            } else if (resultado == COMBATE_PENDIENTE) {
                printf("\n============================================================\nCombate pendiente: se alcanzo el limite de %d rondas.\n",
                    resumen.rondas);
            } else puts("Combate cancelado por una entrada incompleta o un dato invalido.");
        }
    }
    if (salida != NULL) *salida = resumen;
    return resultado;
}
