#include <stdio.h>
#include <string.h>
#include "efectividad.h"
#include "entrada.h"
#include "utils.h"

// Los datos viven en DB
static char nombres_tipos[CANTIDAD_DE_TIPOS][20];
static double tabla[CANTIDAD_DE_TIPOS][CANTIDAD_DE_TIPOS];
static int catalogo_cargado = 0;

// Busca sin distinguir mayusculas en el catalogo recibido, que tambien puede ser
// el temporal durante una carga. -1 indica nombre ausente o fallo de conversion.
static int indice_tipo(const char nombres[][20], const char *nombre) {
    int indice = -1;
    char *minusculas = tolower_string(nombre);
    if (minusculas != NULL) {
        int i = 0;
        while (i < CANTIDAD_DE_TIPOS && indice == -1) {
            if (strcmp(minusculas, nombres[i]) == 0) indice = i;
            i++;
        }
        free(minusculas);
    }
    return indice;
}

// Parte de x1 y aplica solo las excepciones del archivo (0, 0.5 o 2).
// Valida nombres y relaciones sin duplicados en arreglos temporales; publica al
// terminar. Un archivo roto no se lleva por delante la tabla que funcionaba xd.
int cargar_efectividad_desde(const char *ruta) {
    int valido = ruta != NULL;
    FILE *archivo = valido ? fopen(ruta, "r") : NULL;
    valido = archivo != NULL;
    if (valido) {
        char nombres[CANTIDAD_DE_TIPOS][20] = {{0}};
        double valores[CANTIDAD_DE_TIPOS][CANTIDAD_DE_TIPOS];
        int definida[CANTIDAD_DE_TIPOS][CANTIDAD_DE_TIPOS] = {{0}};
        char linea[128];
        int cantidad;
        valido = leer_linea(archivo, linea, sizeof(linea)) == 1 &&
            convertir_entero(linea, CANTIDAD_DE_TIPOS, CANTIDAD_DE_TIPOS, &cantidad);
        for (int i = 0; i < CANTIDAD_DE_TIPOS && valido; i++) {
            char extra;
            valido = leer_linea(archivo, linea, sizeof(linea)) == 1;
            if (valido) {
                char nombre[20];
                valido = sscanf(linea, "%19s %c", nombre, &extra) == 1 &&
                    indice_tipo(nombres, nombre) == -1;
                if (valido) {
                    char *minusculas = tolower_string(nombre);
                    valido = minusculas != NULL && strcmp(minusculas, "-") != 0;
                    if (valido) strcpy(nombres[i], minusculas);
                    free(minusculas);
                }
            }
        }
        for (int i = 0; i < CANTIDAD_DE_TIPOS; i++) {
            for (int j = 0; j < CANTIDAD_DE_TIPOS; j++) valores[i][j] = 1.0;
        }
        int lectura = 0;
        while (valido && (lectura = leer_linea(archivo, linea, sizeof(linea))) == 1) {
            char atacante[20], defensor[20], factor[20], extra;
            valido = sscanf(linea, "%19s %19s %19s %c", atacante, defensor, factor, &extra) == 3;
            if (valido) {
                int fila = indice_tipo(nombres, atacante);
                int columna = indice_tipo(nombres, defensor);
                double multiplicador = -1;
                if (strcmp(factor, "0") == 0) multiplicador = 0;
                if (strcmp(factor, "0.5") == 0) multiplicador = 0.5;
                if (strcmp(factor, "2") == 0) multiplicador = 2;
                valido = fila >= 0 && columna >= 0 && multiplicador >= 0;
                if (valido) {
                    valido = !definida[fila][columna];
                    if (valido) {
                        definida[fila][columna] = 1;
                        valores[fila][columna] = multiplicador;
                    }
                }
            }
        }
        valido = valido && lectura == 0 && !ferror(archivo);
        fclose(archivo);
        if (valido) {
            memcpy(nombres_tipos, nombres, sizeof(nombres));
            memcpy(tabla, valores, sizeof(valores));
            catalogo_cargado = 1;
        }
    }
    if (!valido) fprintf(stderr, "No se pudo cargar el catalogo de efectividad.\n");
    return valido;
}

int cargar_efectividad() {
    return cargar_efectividad_desde("DB/EFECTIVIDAD.txt");
}

// Indice compartido para combate y diversidad del equipo; carga el catalogo
// en la primera consulta y devuelve -1 si el nombre o la carga no son validos.
int obtener_tipo(const char *nombre_del_tipo) {
    int indice = -1;
    if (nombre_del_tipo != NULL && (catalogo_cargado || cargar_efectividad())) {
        indice = indice_tipo(nombres_tipos, nombre_del_tipo);
    }
    return indice;
}

int es_tipo_pokemon_valido(const char *nombre_del_tipo) {
    return obtener_tipo(nombre_del_tipo) >= 0;
}

// El ataque debe pertenecer al atacante. Contra doble tipo multiplicamos ambas
// relaciones: x2 y x2 dan x4; cualquier inmunidad deja el producto en cero.
// Solo escribe la salida si todos los tipos necesarios son validos.
int calcular_efectividad(const pokemon *atacante, const char *tipo_ataque,
    const pokemon *defensor, double *multiplicador) {
    int valido = atacante != NULL && tipo_ataque != NULL &&
        defensor != NULL && multiplicador != NULL;
    double resultado = 1;
    if (valido) {
        int ataque = obtener_tipo(tipo_ataque);
        int defensa = obtener_tipo(defensor->tipo_primario);
        valido = ataque >= 0 && defensa >= 0 &&
            (ataque == obtener_tipo(atacante->tipo_primario) ||
             ataque == obtener_tipo(atacante->tipo_secundario));
        if (valido) {
            resultado = tabla[ataque][defensa];
            if (defensor->tipo_secundario[0] != '\0' &&
                strcmp(defensor->tipo_secundario, "-") != 0) {
                int secundario = obtener_tipo(defensor->tipo_secundario);
                valido = secundario >= 0;
                if (valido) resultado *= tabla[ataque][secundario];
            }
        }
    }
    if (valido) *multiplicador = resultado;
    return valido;
}
