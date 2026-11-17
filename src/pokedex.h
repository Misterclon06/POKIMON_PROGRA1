#ifndef pokedex_h
#define pokedex_h

// estructura para almacenar los datos de un pokemon en la pokedex
typedef struct {
    int id;
    char nombre[50];
    char tipo_primario[20];
    char tipo_secundario[20];
    int hp;
    int ataque;
    int defensa;
    int velocidad;
} pokedata;

/**
 * Carga la pokedex desde el archivo de datos (DB/POKEDEX_DB.txt) a memoria y
 * construye la tabla de traduccion id -> slot usando el hash perfecto
 * precalculado. se llama al entrar en el menu de pokedex.
 *
 * @return 0 si tuvo exito
 */
int cargar_pokedex();

// Valida un archivo completo antes de reemplazar el catalogo actual.
// Retorna 0 si tuvo exito; conserva el catalogo anterior si falla.
int cargar_pokedex_desde(const char *ruta);

/**
 * hace lo que dice el nombre xd, imprime los datos de un pokemon en formato legible.
 */
void mostrar_pokedata(const pokedata *poke);

/**
 * Imprime todos los registros cargados en la pokedex
 */
void mostrar_pokedex();

/**
 * Busca un pokemon por nombre sin distinguir mayusculas o minusculas usando
 * el hash perfecto.
 *
 */
pokedata *buscar_nombre(const char *nombre);

/**
 * Busca un pokemon por su id numerico usando la tabla id -> slot.
 *.
 */
pokedata *buscar_id(int id);


#endif // pokedex_h
