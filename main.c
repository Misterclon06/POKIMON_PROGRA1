#include <limits.h>
#include <stdio.h>
#include "src/torneo.h"
#include "src/combate.h"
#include "src/equipo_automatico.h"
#include "src/entrada.h"
#include "src/preparacion_torneo.h"

static int final_fase(int numero);
static const char *nombre_fase(int numero);

static void menu_pokedex() {
    int opcion = -1, leyendo = 1;
    while (leyendo && opcion != 0) {
        puts("\n1. Mostrar Pokedex\n2. Buscar por numero\n3. Buscar por nombre\n0. Volver");
        leyendo = leer_opcion("Opcion: ", 0, 3, &opcion);
        const pokedata *encontrado = NULL;
        if (leyendo && opcion == 1) mostrar_pokedex();
        else if (leyendo && opcion == 2) {
            int id;
            leyendo = leer_opcion("Numero: ", 1, INT_MAX, &id);
            if (leyendo) encontrado = buscar_id(id);
        } else if (leyendo && opcion == 3) {
            char nombre[50];
            leyendo = leer_texto("Nombre: ", nombre, sizeof(nombre));
            if (leyendo) encontrado = buscar_nombre(nombre);
        }
        if (leyendo && (opcion == 2 || opcion == 3)) {
            if (encontrado != NULL) mostrar_pokedata(encontrado);
            else puts("No aparece en la Pokedex.");
        }
    }
}

static void listar_entrenadores(const torneo *liga, int completos) {
    if (liga->cantidad == 0) puts("No hay entrenadores registrados. Crea uno o carga un archivo.");
    for (int i = 0; i < liga->cantidad; i++) {
        const entrenador *ent = &liga->entrenadores[i];
        printf("\n%d. %s (%d Pokemon)\n", ent->id, ent->nombre, ent->n_pokemon);
        if (completos) mostrar_entrenador(ent);
    }
}

static int coincide_nombre(const char *nombre, const char *busqueda) {
    size_t inicio = 0, fin = strlen(busqueda);
    while (inicio < fin && isspace((unsigned char)busqueda[inicio])) inicio++;
    while (fin > inicio && isspace((unsigned char)busqueda[fin - 1])) fin--;
    size_t largo = fin - inicio;
    int encontrado = 0;
    for (size_t i = 0; nombre[i] != '\0' && !encontrado && largo > 0; i++) {
        size_t j = 0;
        while (j < largo && nombre[i + j] != '\0' &&
            tolower((unsigned char)nombre[i + j]) == tolower((unsigned char)busqueda[inicio + j])) j++;
        encontrado = j == largo;
    }
    return encontrado;
}

// Con 32 entrenadores basta recorrer el registro. Si varios nombres coinciden,
// primero mostramos los IDs; consultar una ficha no debe ser una adivinanza xd.
static int buscar_entrenador(const torneo *liga, int modo) {
    int indice = -1;
    if (liga->cantidad == 0) puts("No hay entrenadores registrados.");
    else if (modo == 1) {
        int id;
        if (leer_opcion("ID del entrenador (0: volver): ", 0, INT_MAX, &id) && id > 0) {
            indice = torneo_indice_entrenador(liga, id);
            if (indice < 0) puts("Entrenador inexistente. Puedes consultar la lista completa.");
        }
    } else {
        char nombre[50];
        if (leer_texto("Nombre o parte del nombre: ", nombre, sizeof(nombre))) {
            int encontrados = 0;
            for (int i = 0; i < liga->cantidad; i++) {
                if (coincide_nombre(liga->entrenadores[i].nombre, nombre)) {
                    printf("  ID %-5d %s\n", liga->entrenadores[i].id, liga->entrenadores[i].nombre);
                    encontrados++;
                    indice = i;
                }
            }
            if (encontrados == 0) puts("No hay entrenadores que coincidan con ese nombre.");
            else if (encontrados > 1) {
                int id;
                indice = -1;
                if (leer_opcion("ID de la ficha que quieres ver (0: volver): ", 0, INT_MAX, &id) && id > 0) {
                    indice = torneo_indice_entrenador(liga, id);
                    if (indice < 0 || !coincide_nombre(liga->entrenadores[indice].nombre, nombre)) {
                        puts("El ID no pertenece a los entrenadores encontrados.");
                        indice = -1;
                    }
                }
            }
        }
    }
    return indice;
}

static void consultar_entrenador(const torneo *liga, int modo) {
    int indice = buscar_entrenador(liga, modo);
    if (indice >= 0) mostrar_entrenador(&liga->entrenadores[indice]);
}

static int pedir_reglas(reglas_equipo_automatico *reglas) {
    int valido = leer_opcion("Cantidad (1..6): ", 1, MAX_EQUIPO, &reglas->cantidad_pokemon);
    if (valido) valido = leer_opcion("Nivel maximo individual (1..100): ", 1, 100,
        &reglas->nivel_maximo_individual);
    if (valido) valido = leer_opcion("Nivel comun (0: niveles aleatorios): ", 0,
        reglas->nivel_maximo_individual, &reglas->nivel_comun);
    if (valido) valido = leer_opcion("Minimo de tipos distintos (0: sin minimo): ", 0,
        2 * reglas->cantidad_pokemon, &reglas->tipos_distintos_minimos);
    return valido;
}

static const char *mensaje_generacion(resultado_generacion_equipo resultado) {
    const char *mensaje = "datos o reglas invalidos";
    switch (resultado) {
        case GENERACION_EQUIPO_OK: mensaje = "equipo encontrado"; break;
        case GENERACION_EQUIPO_SIN_SOLUCION: mensaje = "sin solucion para esas restricciones"; break;
        case GENERACION_EQUIPO_LIMITE_BUSQUEDA: mensaje = "limite de busqueda; prueba restricciones menos exigentes"; break;
        case GENERACION_EQUIPO_DATOS_NO_DISPONIBLES: mensaje = "especies o identificadores de ejemplares no disponibles"; break;
        case GENERACION_EQUIPO_ARGUMENTO_INVALIDO: break;
    }
    return mensaje;
}

static int preparar_equipo(entrenador *ent, int modo) {
    int preparado = 0;
    if (modo == 1) {
        reglas_equipo_automatico reglas = {0};
        int confirmar = 1;
        if (ent->n_pokemon > 0) {
            int leido = leer_opcion("Se reemplazara tu equipo. 1: confirmar, 0: cancelar: ", 0, 1, &confirmar);
            if (!leido) confirmar = 0;
        }
        if (confirmar && pedir_reglas(&reglas)) {
            resultado_generacion_equipo r = generar_equipo_automatico(&reglas, ent, NULL);
            printf("Generacion: %s.\n", mensaje_generacion(r));
            preparado = r == GENERACION_EQUIPO_OK;
            if (!preparado) puts("El equipo anterior se conserva. Puedes intentarlo otra vez desde Equipos.");
        }
    } else if (modo == 2) {
        if (ent->n_pokemon == MAX_EQUIPO) puts("Equipo completo. No es un autobus xd.");
        else {
            pokemon poke = preguntar_crear_pokemon();
            preparado = poke.id > 0;
            if (preparado) agregar_pokemon_a_entrenador(ent, poke);
        }
    }
    if (preparado) {
        mostrar_entrenador(ent);
    }
    return preparado;
}

static void registrar_entrenador(torneo *liga) {
    if (liga->iniciado) puts("El torneo ya empezo: el registro esta cerrado.");
    else if (liga->cantidad == ENTRENADORES_TORNEO) puts("Registro completo: ya hay 32 entrenadores.");
    else {
        entrenador ent = crear_entrenador();
        int modo;
        int valido = ent.id > 0 && leer_opcion(
            "1. Crear equipo automatico\n2. Crear un Pokemon manual\n3. Preparar equipo despues\nOpcion: ",
            1, 3, &modo);
        if (valido && modo != 3) preparar_equipo(&ent, modo);
        if (valido && !feof(stdin)) {
            if (torneo_registrar(liga, &ent)) {
                puts("Entrenador registrado.");
                if (ent.n_pokemon == 0) puts("Equipo pendiente: completalo en Entrenadores y equipos antes de iniciar.");
            } else puts("No se pudo registrar el entrenador.");
        }
    }
}

static void crear_equipo(torneo *liga, int modo) {
    if (liga->iniciado) puts("El torneo ya empezo: los equipos estan cerrados.");
    else if (liga->cantidad == 0) listar_entrenadores(liga, 0);
    else {
        int id;
        listar_entrenadores(liga, 0);
        if (leer_opcion("ID del entrenador (0: volver): ", 0, INT_MAX, &id) && id != 0) {
            int indice = torneo_indice_entrenador(liga, id);
            if (indice == -1) puts("Entrenador inexistente. Consulta los IDs de la lista.");
            else preparar_equipo(&liga->entrenadores[indice], modo);
        }
    }
}

static void mostrar_clasificacion(const torneo *liga) {
    if (!liga->iniciado) puts("Todavia no se ha iniciado el torneo.");
    else for (int grupo = 0; grupo < GRUPOS_TORNEO; grupo++) {
        printf("\nGrupo %c%s\nPos  ID           V   E   D  Pts   KO Bajas\n", 'A' + grupo,
            liga->grupo_clasificado[grupo] ? "" : " (provisional o pendiente de desempate)");
        for (int p = 0; p < INTEGRANTES_GRUPO; p++) {
            int indice = liga->posiciones[grupo][p];
            const entrenador *ent = &liga->entrenadores[indice];
            const estadistica_grupo *s = &liga->estadisticas[indice];
            printf("%-4d %-10d %3d %3d %3d %4d %4d %5d\n", p + 1, ent->id,
                s->victorias, s->empates, s->derrotas, s->puntos, s->rivales_derrotados, s->propios_derrotados);
            // El nombre completo va aparte: recortar bytes puede partir una letra con tilde.
            printf("     %s\n", ent->nombre);
        }
    }
}

static void mostrar_encuentros(const torneo *liga, int solo_resueltos) {
    if (!liga->iniciado) puts("Primero inicia el torneo.");
    else {
        int mostrados = 0;
        for (int i = 0; i < COMBATES_TORNEO; i++) {
            const encuentro *e = &liga->encuentros[i];
            if (!solo_resueltos || e->resuelto) {
                mostrados++;
                printf("%d: %d vs %d", i + 1, e->participantes[0], e->participantes[1]);
                if (e->resuelto) printf(" -> %s %d; bajas %d/%d", e->ganador == 0 ? "empate" : "ganador",
                    e->ganador, e->derrotados[0], e->derrotados[1]);
                else if (e->participantes[0] == 0 || e->participantes[1] == 0)
                    printf(" [espera clasificados/resultados previos]");
                else printf(" [habilitado]");
                puts("");
                for (int lado = 0; lado < 2; lado++) {
                    int indice = torneo_indice_entrenador(liga, e->participantes[lado]);
                    printf("    %s: %s\n", lado == 0 ? "Primero" : "Segundo",
                        indice >= 0 ? liga->entrenadores[indice].nombre : "Por definir");
                }
                puts("------------------------------------------------------------");
            }
        }
        if (mostrados == 0) puts("Todavia no hay combates resueltos.");
    }
}

static int primer_encuentro_pendiente(const torneo *liga) {
    int indice = 0;
    while (indice < COMBATES_TORNEO && liga->encuentros[indice].resuelto) indice++;
    return indice < COMBATES_TORNEO ? indice + 1 : 0;
}

static void ficha_encuentro(const torneo *liga, int numero) {
    const encuentro *e = &liga->encuentros[numero - 1];
    printf("\n---------------- ENCUENTRO %d | %s ----------------\n", numero, nombre_fase(numero));
    for (int lado = 0; lado < 2; lado++) {
        int indice = torneo_indice_entrenador(liga, e->participantes[lado]);
        printf(" %s: %s (ID %d)\n", lado == 0 ? "Primero" : "Segundo",
            indice >= 0 ? liga->entrenadores[indice].nombre : "Por definir", e->participantes[lado]);
    }
    printf(" Estado: %s\n", e->resuelto ? "resultado registrado" :
        e->participantes[0] > 0 && e->participantes[1] > 0 ? "habilitado para jugar" : "espera clasificados");
    if (e->resuelto) printf(" Ganador ID: %d | Bajas: %d / %d\n", e->ganador, e->derrotados[0], e->derrotados[1]);
    puts("------------------------------------------------------------");
}

static void listar_encuentros_seleccion(const torneo *liga, int fase, int entrenador_id) {
    for (int i = 0; i < COMBATES_TORNEO; i++) {
        const encuentro *e = &liga->encuentros[i];
        int coincide = fase == 0 || final_fase(i + 1) == fase;
        coincide = coincide && (entrenador_id == 0 || e->participantes[0] == entrenador_id || e->participantes[1] == entrenador_id);
        if (coincide) {
            int a = torneo_indice_entrenador(liga, e->participantes[0]);
            int b = torneo_indice_entrenador(liga, e->participantes[1]);
            printf(" %2d | %s\n", i + 1,
                e->resuelto ? "resuelto" : a >= 0 && b >= 0 ? "habilitado" : "espera clasificados");
            printf("      Primero: %s\n      Segundo: %s\n",
                a >= 0 ? liga->entrenadores[a].nombre : "Por definir",
                b >= 0 ? liga->entrenadores[b].nombre : "Por definir");
        }
    }
}

static int seleccionar_encuentro(torneo *liga) {
    int modo, numero = 0;
    if (leer_opcion("\n1. Siguiente encuentro habilitado\n2. Elegir de la fase actual\n3. Buscar por numero de encuentro\n4. Buscar encuentros de un entrenador\n0. Volver\nOpcion: ", 0, 4, &modo)) {
        int fase = 0, entrenador_id = 0, pedir_numero = 0;
        if (modo == 1) {
            numero = cola_frente(&liga->pendientes);
            if (numero <= 0) puts("No hay encuentros habilitados. Consulta la clasificacion pendiente.");
        } else if (modo == 2) {
            int pendiente = primer_encuentro_pendiente(liga);
            if (pendiente == 0) puts("Todos los encuentros tienen resultado.");
            else {
                fase = final_fase(pendiente);
                printf("\nEncuentros de %s:\n", nombre_fase(pendiente));
                listar_encuentros_seleccion(liga, fase, 0);
                pedir_numero = 1;
            }
        } else if (modo == 3) pedir_numero = 1;
        else if (modo == 4) {
            int tipo;
            if (leer_opcion("Buscar entrenador: 1. ID, 2. Nombre, 0. Volver: ", 0, 2, &tipo) && tipo > 0) {
                int indice = buscar_entrenador(liga, tipo);
                if (indice >= 0) {
                    entrenador_id = liga->entrenadores[indice].id;
                    listar_encuentros_seleccion(liga, 0, entrenador_id);
                    pedir_numero = 1;
                }
            }
        }
        if (pedir_numero && leer_opcion("Numero de encuentro (0: volver): ", 0, COMBATES_TORNEO, &numero) && numero > 0) {
            const encuentro *e = &liga->encuentros[numero - 1];
            if ((fase > 0 && final_fase(numero) != fase) ||
                (entrenador_id > 0 && e->participantes[0] != entrenador_id && e->participantes[1] != entrenador_id)) {
                puts("El encuentro no pertenece a los registros mostrados.");
                numero = 0;
            }
        }
    }
    return numero;
}

static void registrar_resultado_manual(torneo *liga) {
    if (!liga->iniciado) puts("Primero inicia el torneo.");
    else {
        int numero = seleccionar_encuentro(liga);
        if (numero > 0) {
            int ganador, bajas[2];
            encuentro e = liga->encuentros[numero - 1];
            ficha_encuentro(liga, numero);
            int valido = !e.resuelto && e.participantes[0] > 0 && e.participantes[1] > 0;
            if (!valido) puts("El encuentro ya tiene resultado o espera clasificados de rondas anteriores.");
            if (valido) valido = leer_opcion("ID ganador (0: empate de grupos): ", 0, INT_MAX, &ganador);
            if (valido) valido = leer_opcion("Pokemon perdidos por el primero: ", 0, MAX_EQUIPO, &bajas[0]);
            if (valido) valido = leer_opcion("Pokemon perdidos por el segundo: ", 0, MAX_EQUIPO, &bajas[1]);
            if (valido) valido = torneo_registrar_resultado(liga, numero, e.participantes[0], e.participantes[1], ganador, bajas[0], bajas[1]);
            puts(valido ? "Resultado registrado." : "Resultado rechazado. Revisa participantes, ronda y bajas.");
        }
    }
}

// Toma el primer encuentro habilitado de la cola y registra solo un resultado
// oficial. Si queda pendiente se conserva para resolverlo despues.
// El motor pone al humano primero: reordenamos participantes y luego traducimos
// ganador/bajas al orden del fixture.
static int jugar_proximo(torneo *liga, int jugador, int mostrar) {
    int registrado = 0;
    int numero = cola_frente(&liga->pendientes);
    if (liga->iniciado && numero > 0) {
        encuentro e = liga->encuentros[numero - 1];
        int indices[] = {torneo_indice_entrenador(liga, e.participantes[0]), torneo_indice_entrenador(liga, e.participantes[1])};
        int orden = jugador == e.participantes[1] ? 1 : 0;
        int humano = jugador != 0 && (jugador == e.participantes[0] || jugador == e.participantes[1]);
        opciones_combate o = opciones_combate_iniciales(humano ? COMBATE_CONTRA_BOT : COMBATE_INTERACTIVO);
        o.seleccion_automatica = !humano;
        o.mostrar = mostrar || humano;
        o.pausar_rondas = mostrar && !humano;
        o.permitir_empate = numero <= COMBATES_GRUPOS;
        o.ataque_emergencia = numero > COMBATES_GRUPOS;
        resumen_combate r;
        printf("\nEncuentro %d: %d vs %d\n", numero, e.participantes[0], e.participantes[1]);
        printf(" %s frente a %s\n------------------------------------------------------------\n",
            liga->entrenadores[indices[0]].nombre, liga->entrenadores[indices[1]].nombre);
        resultado_combate resultado = combatir_configurado(&liga->entrenadores[indices[orden]],
            &liga->entrenadores[indices[1 - orden]], &o, &r);
        if (resultado == COMBATE_GANADOR_ENTRENADOR_1 || resultado == COMBATE_GANADOR_ENTRENADOR_2 || resultado == COMBATE_EMPATE) {
            int ganador = resultado == COMBATE_EMPATE ? 0 : e.participantes[resultado == COMBATE_GANADOR_ENTRENADOR_1 ? orden : 1 - orden];
            registrado = torneo_registrar_resultado(liga, numero, e.participantes[0], e.participantes[1],
                ganador, r.derrotados[orden], r.derrotados[1 - orden]);
            if (registrado) {
                const char *nombre = ganador == 0 ? "Empate" :
                    liga->entrenadores[torneo_indice_entrenador(liga, ganador)].nombre;
                printf("\n RESULTADO encuentro %d: %s | Bajas: %d / %d\n\n",
                    numero, nombre, r.derrotados[orden], r.derrotados[1 - orden]);
            }
        } else puts("Encuentro pendiente: falta resolverlo; no se asignan puntos ni un ganador inventado.");
    } else puts("No hay encuentros habilitados. Consulta clasificacion y resultados pendientes.");
    return registrado;
}

static void mostrar_podio(const torneo *liga) {
    int ids[4];
    if (torneo_podio(liga, ids)) {
        puts("\n===================== PODIO DEL TORNEO =====================");
        const char *puestos[] = {"Campeon", "Subcampeon", "Tercer lugar", "Cuarto lugar"};
        for (int i = 0; i < 4; i++) printf("%s: %s (ID %d)\n", puestos[i],
            liga->entrenadores[torneo_indice_entrenador(liga, ids[i])].nombre, ids[i]);
    } else puts("Aun no hay podio completo. El trofeo no se entrega por adelantado xd.");
}


static void elegir_jugador(const torneo *liga, int *jugador) {
    int valido = 0, id = 0, leyendo = 1;
    puts("\nElige el modo:\nID 0: organizar entre bots.\nID de tu personaje: jugar sus encuentros.");
    listar_entrenadores(liga, 0);
    while (!valido && leyendo) {
        leyendo = leer_opcion("ID de tu entrenador (0: todos son bots): ", 0, INT_MAX, &id);
        if (leyendo) {
            valido = id == 0 || torneo_indice_entrenador(liga, id) >= 0;
            if (!valido) puts("Entrenador inexistente. Elige un ID de la lista.");
        }
    }
    if (valido) *jugador = id;
}

static void cargar_entrenadores(torneo *liga, int *jugador) {
    char ruta[200];
    if (liga->iniciado || liga->cantidad != 0) puts("La carga requiere un registro vacio y un torneo sin iniciar.");
    else if (leer_texto("Archivo de entrenadores: ", ruta, sizeof(ruta))) {
        if (torneo_cargar_entrenadores(liga, ruta)) {
            puts("Entrenadores y equipos restaurados.");
            elegir_jugador(liga, jugador);
        } else puts("Archivo rechazado: revisa ruta y formato. Se conserva el registro.");
    }
}

static void iniciar_torneo(torneo *liga) {
    int valido = !liga->iniciado;
    if (liga->iniciado) puts("El torneo ya esta iniciado.");
    for (int i = 0; i < liga->cantidad && !liga->iniciado; i++) {
        if (liga->entrenadores[i].n_pokemon == 0) {
            printf("Falta equipo: %s (ID %d). Completalo en Entrenadores y equipos.\n",
                liga->entrenadores[i].nombre, liga->entrenadores[i].id);
            valido = 0;
        }
    }
    if (valido && liga->cantidad < ENTRENADORES_TORNEO) {
        printf("Se crearan %d entrenadores con especies aleatorias y las mismas reglas.\n",
            ENTRENADORES_TORNEO - liga->cantidad);
        reglas_equipo_automatico reglas = {0};
        valido = pedir_reglas(&reglas);
        if (valido) {
            resultado_generacion_equipo r = completar_entrenadores(liga, &reglas);
            valido = r == GENERACION_EQUIPO_OK;
            if (valido) puts("Registro completado: 32 entrenadores con equipo.");
            else printf("No se agregaron entrenadores: %s.\n", mensaje_generacion(r));
        }
    }
    if (valido) {
        if (torneo_iniciar(liga, "DB/CRUCES.txt"))
            puts("Torneo iniciado: ocho grupos de cuatro, segun el orden de registro.");
        else puts("No se pudo iniciar: revisa equipos y DB/CRUCES.txt. Los entrenadores se conservan.");
    }
}

static void menu_entrenadores(torneo *liga) {
    int opcion = -1, leyendo = 1;
    while (leyendo && opcion != 0 && !feof(stdin)) {
        puts("\n=== ENTRENADORES Y EQUIPOS ===\n1. Registrar entrenador\n2. Agregar un Pokemon manual\n3. Generar o reemplazar equipo automatico\n4. Consultar entrenadores\n5. Consultar equipos\n6. Buscar entrenador por ID\n7. Buscar entrenador por nombre\n0. Volver");
        leyendo = leer_opcion("Opcion: ", 0, 7, &opcion);
        if (leyendo) switch (opcion) {
            case 1: registrar_entrenador(liga); break;
            case 2: crear_equipo(liga, 2); break;
            case 3: crear_equipo(liga, 1); break;
            case 4: listar_entrenadores(liga, 0); break;
            case 5: listar_entrenadores(liga, 1); break;
            case 6: consultar_entrenador(liga, 1); break;
            case 7: consultar_entrenador(liga, 2); break;
            case 0: break;
        }
    }
}

static int guardar_sesion(const torneo *liga, int jugador) {
    int guardado = torneo_guardar_partida(liga, jugador, "save.txt");
    if (!guardado) puts("No se pudo guardar save.txt. Revisa permisos, espacio o un respaldo save.txt.bak pendiente.");
    return guardado;
}

static int final_fase(int numero) {
    const int finales[] = {48, 56, 60, 62, 63, 64};
    int i = 0;
    while (i < 5 && numero > finales[i]) i++;
    return finales[i];
}

static const char *nombre_fase(int numero) {
    const char *nombre = "Final";
    if (numero <= 48) nombre = "Fase de grupos";
    else if (numero <= 56) nombre = "Octavos de final";
    else if (numero <= 60) nombre = "Cuartos de final";
    else if (numero <= 62) nombre = "Semifinales";
    else if (numero == 63) nombre = "Tercer lugar";
    return nombre;
}

// Se detiene en el limite de la fase: la final no se cuela mientras estas
// revisando los grupos. Una igualdad pendiente tampoco se resuelve por decreto xd.
static int avanzar_fase(torneo *liga, int jugador, int detalle, int *detenido) {
    int primero = 0, jugados = 0;
    while (primero < COMBATES_TORNEO && liga->encuentros[primero].resuelto) primero++;
    if (primero == COMBATES_TORNEO) mostrar_podio(liga);
    else {
        int limite = final_fase(primero + 1);
        printf("\n============================================================\n %s\n============================================================\n",
            nombre_fase(primero + 1));
        int seguir = !cola_esVacia(&liga->pendientes);
        while (seguir && !feof(stdin)) {
            int numero = cola_frente(&liga->pendientes);
            seguir = numero > 0 && numero <= limite;
            if (seguir) {
                seguir = jugar_proximo(liga, jugador, detalle);
                if (seguir) jugados++;
                else *detenido = 1;
                int siguiente = cola_frente(&liga->pendientes);
                if (seguir && siguiente > 0 && siguiente <= limite) {
                    seguir = esperar_enter("Enter: siguiente encuentro | 0: volver al menu: ");
                    if (!seguir) *detenido = 1;
                }
            }
            seguir = seguir && !cola_esVacia(&liga->pendientes);
        }
        printf("\n Fin del recorrido de fase: %d encuentros resueltos.\n", jugados);
        if (limite == COMBATES_GRUPOS) mostrar_clasificacion(liga);
        if (jugados == 0) puts("Fase pendiente: consulta la clasificacion y los resultados que faltan.");
        if (limite == COMBATES_TORNEO) mostrar_podio(liga);
    }
    return jugados;
}

static void menu_torneo(torneo *liga, int *jugador) {
    int opcion = -1, leyendo = 1;
    while (leyendo && opcion != 0 && !feof(stdin)) {
        printf("\n=== TORNEO | %s ===\n", *jugador == 0 ? "ORGANIZADOR: TODOS SON BOTS" : "JUGADOR CON PERSONAJE");
        puts("1. Iniciar torneo\n2. Jugar o ver siguiente encuentro (todos los turnos)\n3. Avanzar siguiente fase\n4. Registrar resultado por teclado\n5. Elegir jugar con personaje u organizar bots\n6. Simular torneo completo entre bots\n0. Volver");
        leyendo = leer_opcion("Opcion: ", 0, 6, &opcion);
        if (leyendo && opcion == 1) iniciar_torneo(liga);
        else if (leyendo && opcion == 5) {
            if (liga->cantidad == 0) puts("Crea participantes o inicia el torneo como organizador antes de elegir un personaje.");
            else elegir_jugador(liga, jugador);
        } else if (leyendo && opcion != 0) {
            if (!liga->iniciado) puts("Primero inicia el torneo.");
            else if (opcion == 2) jugar_proximo(liga, *jugador, 1);
            else if (opcion == 4) registrar_resultado_manual(liga);
            else if (opcion == 6 && *jugador != 0)
                puts("Para simular sin jugar, elige modo organizador (opcion 5, ID 0).");
            else {
                int detalle;
                puts("Los bots se enfrentan solos. En modo jugador, tu personaje sigue bajo tu control.");
                int valido = leer_opcion("Detalle: 1. Resultados por encuentro, 2. Todos los turnos, 0. Cancelar: ", 0, 2, &detalle);
                if (valido && detalle != 0) {
                    int avanzar = 1, detenido = 0;
                    while (avanzar && !feof(stdin)) {
                        avanzar = avanzar_fase(liga, *jugador, detalle == 2, &detenido);
                        avanzar = avanzar && !detenido && opcion == 6 && !cola_esVacia(&liga->pendientes);
                        if (avanzar) avanzar = esperar_enter("Enter: siguiente fase | 0: volver al menu: ");
                    }
                }
            }
        }
        if (leyendo && opcion != 0 && liga->cantidad > 0) guardar_sesion(liga, *jugador);
    }
}

static void menu_consultas(const torneo *liga) {
    int opcion = -1, leyendo = 1;
    while (leyendo && opcion != 0 && !feof(stdin)) {
        puts("\n=== CONSULTAS ===\n1. Clasificacion\n2. Enfrentamientos\n3. Historial de combates\n4. Podio y campeon\n0. Volver");
        leyendo = leer_opcion("Opcion: ", 0, 4, &opcion);
        if (leyendo) switch (opcion) {
            case 1: mostrar_clasificacion(liga); break;
            case 2: mostrar_encuentros(liga, 0); break;
            case 3: mostrar_encuentros(liga, 1); break;
            case 4: mostrar_podio(liga); break;
            case 0: break;
        }
    }
}

static void operar_archivo(torneo *liga, int opcion) {
    char ruta[200];
    int correcto = 0;
    if (opcion != 4 && !liga->iniciado) puts("Primero inicia el torneo.");
    else if (opcion == 4 && liga->cantidad == 0) puts("No hay entrenadores para guardar.");
    else if (leer_texto(opcion == 1 ? "Archivo de resultados: " : "Guardar en archivo: ", ruta, sizeof(ruta))) {
        switch (opcion) {
            case 1: correcto = torneo_cargar_resultados(liga, ruta); break;
            case 2: correcto = torneo_guardar_resultados(liga, ruta); break;
            case 3: correcto = torneo_guardar_clasificacion(liga, ruta); break;
            case 4: correcto = torneo_guardar_entrenadores(liga, ruta); break;
        }
        puts(correcto ? "Operacion completada." :
            "No se pudo completar: revisa ruta, permisos y formato. La liga en memoria se conserva.");
    }
}

static void menu_archivos(torneo *liga, int *jugador) {
    int opcion = -1, leyendo = 1;
    while (leyendo && opcion != 0 && !feof(stdin)) {
        puts("\n=== ARCHIVOS ===\n1. Cargar entrenadores y equipos\n2. Cargar resultados\n3. Guardar entrenadores y equipos\n4. Guardar resultados\n5. Guardar clasificacion\n6. Guardar partida en save.txt\n7. Continuar desde save.txt\n0. Volver");
        leyendo = leer_opcion("Opcion: ", 0, 7, &opcion);
        if (leyendo) switch (opcion) {
            case 1: cargar_entrenadores(liga, jugador); break;
            case 2: operar_archivo(liga, 1); break;
            case 3: operar_archivo(liga, 4); break;
            case 4: operar_archivo(liga, 2); break;
            case 5: operar_archivo(liga, 3); break;
            case 6:
                if (guardar_sesion(liga, *jugador)) puts("Partida guardada en save.txt.");
                break;
            case 7:
                puts(torneo_cargar_partida(liga, jugador, "save.txt", "DB/CRUCES.txt") ?
                    "Partida restaurada desde save.txt." : "No se pudo cargar save.txt. La partida actual se conserva.");
                break;
            case 0: break;
        }
    }
}

static int bienvenida(torneo *liga, int *jugador) {
    int opcion = -1, leyendo = 1, entrar = 0;
    puts("\nProfesor Oak: Bienvenido al mundo de los Pokemon!");
    puts("Para algunos son mascotas. Otros los usan para combatir.");
    while (leyendo && opcion != 0 && !entrar && !feof(stdin)) {
        puts("\n1. Crear tu personaje y jugar\n2. Ya tengo datos guardados: continuar save.txt\n3. Importar entrenadores de otro archivo\n4. Organizar torneo automatico entre bots\n0. Salir");
        leyendo = leer_opcion("Opcion: ", 0, 4, &opcion);
        if (leyendo && opcion == 1) {
            registrar_entrenador(liga);
            entrar = liga->cantidad > 0;
            if (entrar) *jugador = liga->entrenadores[0].id;
        } else if (leyendo && opcion == 2) {
            entrar = torneo_cargar_partida(liga, jugador, "save.txt", "DB/CRUCES.txt");
            puts(entrar ? "Partida restaurada automaticamente desde save.txt." :
                "No se pudo cargar save.txt: no existe o sus datos son invalidos. Puedes crear una partida o importar entrenadores.");
        } else if (leyendo && opcion == 3) {
            cargar_entrenadores(liga, jugador);
            entrar = liga->cantidad > 0;
        } else if (leyendo && opcion == 4) {
            *jugador = 0;
            entrar = 1;
            puts("Modo organizador: inicia el torneo para crear los 32 participantes y avanzar por fases.");
        }
    }
    return leyendo && opcion != 0 && entrar && !feof(stdin);
}

int main() {
    torneo liga;
    torneo_crear(&liga);
    int opcion = -1, jugador = 0;
    int correcto = cargar_pokedex() == 0 && liga.pendientes.inicio != NULL;
    int leyendo = correcto && bienvenida(&liga, &jugador);
    while (leyendo && opcion != 0 && !feof(stdin)) {
        printf("\n=== GRAN TORNEO POKEMON ===\nEstado: %s | Entrenadores: %d/%d\nModo: %s | Jugador: %d\n",
            liga.iniciado ? "iniciado" : "preparacion", liga.cantidad, ENTRENADORES_TORNEO,
            jugador == 0 ? "Organizador (bots)" : "Jugar con personaje", jugador);
        puts("1. Pokedex\n2. Entrenadores y equipos\n3. Torneo\n4. Consultas\n5. Archivos\n6. Guardar partida en save.txt\n0. Salir");
        leyendo = leer_opcion("Opcion: ", 0, 6, &opcion);
        if (leyendo) switch (opcion) {
            case 1: menu_pokedex(); break;
            case 2: menu_entrenadores(&liga); break;
            case 3: menu_torneo(&liga, &jugador); break;
            case 4: menu_consultas(&liga); break;
            case 5: menu_archivos(&liga, &jugador); break;
            case 6:
                if (guardar_sesion(&liga, jugador)) puts("Partida guardada en save.txt.");
                break;
            case 0: break;
        }
    }
    if (liga.cantidad > 0) guardar_sesion(&liga, jugador);
    puts("Hasta el proximo torneo.");
    torneo_destruir(&liga);
    return correcto ? 0 : 1;
}
