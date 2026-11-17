# Modulo Entidades

## 1. Descripcion general

El modulo este modulo define las entidades (valga la redundancia) y su "CRUD" por asi decirlo: el **ejemplar
Pokemon** (`pokemon`) y el **entrenador** (`entrenador`).
Se encarga de crearlos de forma interactiva y rapida posible.

La idea del modulo es la separacion que pide el enunciado entre
**especie** y **ejemplar**:

- La **especie** (`pokedata`) vive en la Pokedex, se carga desde
  `DB/POKEDEX_DB.txt` y **nunca se modifica**.
- El **ejemplar** (`pokemon`) es una version propia del entrenador, con su nivel,
  su nombre y estadisticas unicas calculadas al crearlo.

Dos pichu (para variar xd) de nivel 50 casi nunca tendran las mismas estadisticas,
porque cada uno recibe sus propios IVs y EVs aleatorios.

## 2. Arquitectura y estructura de archivos

```
src/
  entidades.h      Structs pokemon y entrenador
  entidades.c      Creacion interactiva y generacion de stats
  utils.h          gen_semilla() y alterador_semilla() (generacion procedural XDXDXD)
  pokedex.h        pokedata, buscar_nombre(), buscar_id() (de donde salen las especies)
```

Dependencias:

```
entidades.c ──> pokedex.h   (busca la especie al crear un ejemplar)
            ──> utils.h     (semillas pseudoaleatorias)
```

## 3. Estructuras

### `pokemon` (ejemplar)

| Campo | Tipo | Descripcion |
|---|---|---|
| `id` | `int` | Identificador unico del ejemplar; independiente del numero de especie |
| `especie` | `char[50]` | Nombre de la especie |
| `nombre` | `char[50]` | Apodo del ejemplar |
| `nivel` | `int` | 1 a 100 |
| `hp`, `hp_max` | `int` | Vida actual y maxima; el combate baja `hp` sin tocar `hp_max` |
| `ataque`, `defensa`, `velocidad` | `int` | Estadisticas calculadas del ejemplar |
| `tipo_primario`, `tipo_secundario` | `char[20]` | Copiados de la especie (`-` si no tiene segundo tipo) |
| `estado` | `char[20]` | Estado actual, inicia en `"normal"` |

### `entrenador`

| Campo | Tipo | Descripcion |
|---|---|---|
| `id` | `int` | Identificador, se asigna de forma correlativa (1, 2, 3...) |
| `nombre` | `char[50]` | Nombre del entrenador |
| `equipo` | `pokemon[MAX_EQUIPO]` | Hasta 6 ejemplares |
| `n_pokemon` | `int` | Cantidad de ejemplares en el equipo |
| `n_victorias`, `n_empates`, `n_derrotas` | `int` | Historial del torneo |
| `puntuacion` | `int` | Puntos de la fase de grupos |

`MAX_EQUIPO` vale `6` y esta definido en `entidades.h`.

## 4. Funciones

### `src/entidades.h`

```c
entrenador crear_entrenador();
```
hice un inicio interactivo como en el juego original, solo que 
el genero no sirve para nada xd. inicializa contadores,
asigna el id y devuelve al entrenador sin equipo. El menu ofrece crear un
equipo automatico, un Pokemon manual o completarlo despues.
**Retorna:** el entrenador creado, o una estructura en cero si falla la entrada.

```c
entrenador crear_entrenador_con_nombre(const char *nombre);
```

Constructor sin preguntas, compartido por la creacion interactiva y los rivales.
Valida nombre no vacio, no compuesto solo por espacios y menor de 50 caracteres.
Asigna un ID del mismo contador; no acepta duplicados por usar un contador paralelo.
Devuelve una estructura en cero ante nombre invalido o agotamiento de IDs.

```c
pokemon crear_pokemon(const char *especie, const char *nombre, int nivel);
```
Busca la especie, asigna un ID nuevo al ejemplar, copia tipos, guarda el apodo y genera las stats.
Esta version recibe los datos por parametros y no pregunta nada por consola;
asi tambien la puede llamar el generador sin montar una entrevista por Pokemon xd.
**Retorna:** el ejemplar creado, o uno inicializado en cero ante especie, nombre
o nivel invalidos. Valida apodo no vacio y menor de 50 caracteres, y nivel
entre 1 y 100. La Pokedex debe estar cargada. Los IDs se mantienen unicos
durante la ejecucion; al restaurar datos de archivo se actualizan los
contadores con `actualizar_ids_restaurados()`.

```c
pokemon crear_pokemon_con_semilla(const char *especie, const char *nombre,
    int nivel, uint32_t semilla);
```

Es el mismo constructor con semilla explicita. `crear_pokemon()` lo llama con
`gen_semilla()`. Quien necesite repetir atributos puede pasar una semilla
explicita a esta API. Asi las stats se calculan **una sola vez**: ya no nacen unas para
ser reemplazadas inmediatamente por otras.

La semilla se usa literalmente, incluido cero; no significa "sortea otra".
Dos llamadas con la misma especie, nivel y semilla producen las mismas stats,
pero IDs distintos: reproducir atributos no clona identidades. Validaciones,
tipos y asignacion de IDs se comparten con el constructor habitual.

```c
pokemon preguntar_crear_pokemon();
```
Pide especie, apodo y nivel, copia los tipos de la especie y genera
estadisticas. El usuario puede rechazarlas y pedir unas nuevas cuantas veces
quiera. Esta parte interactiva llama al constructor por parametros.
**Retorna:** el ejemplar creado, o una estructura en cero si se interrumpe la
entrada o falla el constructor.

```c
void agregar_pokemon_a_entrenador(entrenador *ent, pokemon poke);
```
Agrega un ejemplar al final del equipo de un entrenador. 
Rechaza un entrenador ausente, una cantidad fuera de `0..MAX_EQUIPO` o un
equipo completo antes de escribir en el arreglo.

```c
void eliminar_pokemon_de_entrenador(entrenador *ent, int indice);
```
Quita el ejemplar en `indice` y corre los siguientes una posicion.
Valida la cantidad del equipo y el indice antes de mover ejemplares; si alguno
es invalido, conserva el equipo.

```c
void mostrar_entrenador(const entrenador *ent);
void mostrar_pokemon(const pokemon *poke);
```
Imprimen la ficha del entrenador (con su equipo) o del ejemplar.

```c
void gen_stats_pokemon_por_especie(pokemon *poke, pokedata *poke_base, uint32_t seed, int nivel);
```
Calcula `hp_max`, `ataque`, `defensa` y `velocidad` del ejemplar y deja
`hp = hp_max` a partir de los
stats base de `poke_base`, una semilla y el nivel. 

```c
void gen_stats_pokemon(pokemon *poke, uint32_t seed, int nivel);
```
Igual que la anterior, pero busca la especie sola con `buscar_nombre(poke->especie)`.
Sirve cuando ya se tiene el ejemplar y solo se quieren recalcular sus stats.
El ID ahora identifica al ejemplar. Al reconstruir equipos desde archivo se
conservan las stats guardadas, sin volver a sortearlas con esta funcion.

```c
int estadisticas_pokemon_validas(const pokemon *poke, const pokedata *base);
void actualizar_ids_restaurados(int ultimo_pokemon, int ultimo_entrenador);
```

La primera comprueba que las stats guardadas esten entre el minimo y maximo
posibles de la formula a ese nivel. No demuestra una semilla concreta, porque
no se guardan IV/EV. La segunda adelanta los contadores de identidad sin hacerlos
retroceder; al alcanzar `INT_MAX`, crear un nuevo ejemplar o entrenador falla
con ID cero, sin desbordar.

## 5. Generacion de estadisticas

Se usa la formula de la **primera generacion** (Rojo/Azul). Por cada stat se
sacan dos numeros de la semilla:

- **IV** (valor individual): 4 bits, `0` a `15`.
- **EV** (valor de esfuerzo): 16 bits, `0` a `65535`.

```
stat = ((base + IV + floor(sqrt(EV)) / 4) * nivel) / 100 + 5
HP   = ((base + IV_hp + floor(sqrt(EV)) / 4) * nivel) / 100 + nivel + 10
```

El **IV de HP** no se sortea aparte: igual que en el juego original, se arma con
el bit menos significativo de los IVs de ataque, defensa y velocidad, mas un
cuarto IV extra (en primera gen seria el de "Especial", que aca no existe):

```
IV_hp = (IV_atk & 1) << 3 | (IV_def & 1) << 2 | (IV_vel & 1) << 1 | (IV_extra & 1)
```

### De donde sale la pseudoaleatoriedad

```
gen_semilla()
   time(NULL) ^ clock() ^ nanosegundos ^ contador  ──> xorshift ──> semilla
        |
        v
alterador_semilla(semilla)  (xorshift 13/17/5)  se llama antes de cada EV y cada IV
```

- `time(NULL)` y `clock()` era suficiente, pero se repetian dentro del
  mismo segundo, al rechazar unas stats salian exactamente las mismas.
- Los **nanosegundos** (`clock_gettime`) cambian en cada llamada.
- El **contador** avanza `0x9E3779B9` (constante de la razon aurea) en cada
  llamada, asi dos semillas pedidas en el mismo instante tampoco coinciden.
- El xorshift32 de Marsaglia (genera mas de 4000 millones de estados 
distintos antes de repetirse) final mezcla los bits para que semillas parecidas den resultados
muy distintos.

Ejemplo real rechazando stats de un Pikachu nivel 50 tres veces seguidas:

| HP | Ataque | Defensa | Velocidad |
|---|---|---|---|
| 102 | 63 | 57 | 74 |
| 94 | 61 | 38 | 85 |
| 113 | 50 | 42 | 89 |
| 113 | 43 | 50 | 78 |


