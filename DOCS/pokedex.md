# Modulo Pokedex

## 1. Descripcion general

El modulo Pokedex es el encargado de cargar y consultar los datos de los 151 Pokemon de
primera generacion, desde un archivo de texto (`DB/POKEDEX_DB.txt`) a
memoria, y permite consultarlos por iD o por nombre desde un menu interactivo.

La pieza central de diseño es la estructura de tablas hash: en vez de recorrer el
arreglo comparando strings, el proyecto usa un **hash perfecto de dos
niveles** construido de antemano para el conjunto fijo de 151 nombres. 

## 2. Arquitectura y estructura de archivos

```
main.c                     Menu principal + menu pokedex 
src/
  pokedex.h                Struct pokedata + declaracion de funciones de consulta y carga (practicamente la APi)
  pokedex.c                Carga de datos, impresion, busqueda por id/nombre
  funcion_hasheo.h          APi del hash perfecto MPHF
  funcion_hasheo.c          implementacion del hash perfecto
  construir_hash.c          genera hash_config.bin (no se usa en el codigo pero esta para demostrar su origen)
  hash_config.bin           Semillas del hash, generadas por construir_hash.c
  utils.h                   funciones varias de utilidad (no sabia donde ponerlas)
DB/
  POKEDEX_DB.txt            Datos fuente: 151 registros pokemon

Makefile                    make / make clean
```

`src/construir_hash.c` **no** forma parte del binario del programa: tiene su
propio `main()` y es una herramienta de desarrollo que se corre por separado
para regenerar `hash_config.bin` si el set de nombres cambia, porque es fijo.

## 3. Flujo de datos

```
DB/POKEDEX_DB.txt                 src/hash_config.bin
        |                                 |
        v                                 v
  cargar_pokedex()  <----- cargar_semilla() [antes de la primera carga]
        |
        | por cada registro:
        |   1. lee id, nombre, tipos, stats
        |   2. nombre -> tolower_string() -> nombre_lower
        |   3. idx = slot_de(nombre_lower)   [hash perfecto -> O(1)]
        |   4. temporal[idx] = registro
        |   5. posiciones[id - 1] = idx
        |
        | catalogo completo y valido -> publicar ambos arreglos
        v
  pokedex[151]  +  id_a_slot[151]   (en memoria, arreglos estaticos del modulo)
        |
        +--> buscar_id(id)      : id_a_slot[id-1] -> pokedex[...]   O(1)
        +--> buscar_nombre(nom) : slot_de(nom) -> pokedex[...] + verificacion   O(1)
        +--> mostrar_pokedex()  : recorre 1..151 via buscar_id()
```

`pokedex[]` e `id_a_slot[]` son `static` dentro de `pokedex.c`: no se exponen
directamente, todo acceso externo pasa por las funciones publicas de
`pokedex.h`.

## 4. APi expuesta

### `src/pokedex.h`

```c
int cargar_pokedex();
```
Carga la pokedex desde `DB/POKEDEX_DB.txt` a memoria.
**Retorna:** `0` si tuvo exito. 

```c
void mostrar_pokedata(const pokedata *poke);
```
imprime un registro de pokemon de la pokedex.

```c
void mostrar_pokedex();
```
imprime los 151 registros cargados.

```c
pokedata *buscar_nombre(const char *nombre);
```
Busca un Pokemon por nombre, sin distinguir mayusculas o minusculas.
**Retorna:** puntero al registro, o `NULL` si no existe, `nombre` es `NULL`
o todavia no se cargo un catalogo valido.

```c
pokedata *buscar_id(int id);
```
Busca un Pokemon por su iD numerico.
**Retorna:** puntero al registro, o `NULL` si `id` esta fuera del rango
`[1, 151]` o todavia no se cargo un catalogo valido.

### `src/funcion_hasheo.h`

```c
int cargar_semilla();
```
Carga las semillas del hash perfecto desde `src/hash_config.bin`.
**Retorna:** `0` si tuvo exito, codigo de error si el archivo falta, esta
truncado o tiene un numero de grupos inesperado.

```c
int slot_de(const char *nombre);
```
Calcula el slot de hash perfecto para un nombre (en minusculas).
**Retorna:** slot en `[0, 150]`, o `-1` si `nombre` es `NULL`.

## 8. Por que hash perfecto (y no otra cosa)

### El problema

`buscar_nombre` necesita, dado un string, encontrar su registro entre 151
Pokemon. El conjunto de nombres es **fijo y conocido de antemano**: no se
agregan ni quitan Pokemon en tiempo de ejecucion, pues, tomandolo en cuenta
como el juego de pokemon original, las especies conocidas siempre son las
que hay. Esa propiedad es la que hace posible usar un hash perfecto en vez
de una estructura de proposito general. no es necesario que usar memoria
dinamica en este caso. entonces tenemos prioridad:

- Estructura fija: No se necesita editar datos ni memoria dinamica.
- Busqueda rapida y directa: sin algoritmos de busqueda lentos y sobrepensados.
- Sin uso de memoria excesivo como las tablas hash convencionales.

### Solucion planteada 

Entonces concluimos en usar una tabla hash, haciendo los **Lookups** mas
rapidos, pero debia entrar "cada una en su cubeta" por asi decirlo. asi
que se  utilizo un algoritmo: Minimal Perfect Hash Function (MPHF). con
variante displace de dos niveles. utilizando un array de struct para 
almacenar los pokedatos (me encanta ese nombre), y otro para los ids
para la busqueda de id por pokemon, usamos un poco mas de memoria, pero
asi las busquedas seran mucho mas rapidas.

### referencias

Esto se basa en el trabajo de Fredman, Komlós y Szemerédi (1984), con la 
variante de búsqueda de semilla descrita por Sager (1985) y Czech, Havas y 
Majewski (1997). La función hash de strings emplea el esquema clásico 
h = h·31 + c popularizado por Java (String.hashCode), y el finalizador de mezcla 
de bits corresponde a MurmurHash3 de Austin Appleby (2008). La implementación 
concreta es una adaptación propia para 151 pokemones.

## Funcion del algoritmo

Es el encargado de generar un hash sin colisiones para un conjunto fijo 
de claves, es "el minimo perfecto" pues busca uno que se acople 
completamente al un numero fijo de espacios. se utiliza una tecnica
de construccion que usa buckets + semillas por bucket, es decir, reparte
en grupos los nombres, y para cada grupo hace un tanteo de semillas (numero)
tal que cada nombre caiga en espacios libres, marca esos slots como ocupados 
y pasa al siguiente grupo. al terminar, cada nombre tiene un espacio unico.

el chiste es que no busca una funcion para las 151 claves, sino que busca 23 
funciones pequeñas (una por grupo), cada una resolviendo solo 5–8 nombres. 
Eso es mucho mas facil. lo digo porque lo intente y no funciono xd.

### Comparacion de complejidad algoritmica

| Metodo | Busqueda (caso promedio) | Busqueda (peor caso) | Colisiones | Memoria extra |
|---|---|---|---|---|
| Busqueda lineal | O(n) | O(n) | N/A | Ninguna |
| arbol binario de busqueda balanceado | O(log n) | O(log n) | N/A | Punteros izq/der por nodo |
| Tabla hash generica | O(1) | O(n) si hay muchas colisiones | Si, requiere manejo aparte | Buckets y listas (depende vea) + posible resize dinamico |
| **Hash perfecto (usado aca)** | **O(1)** | **O(1)** | **Ninguna, por construccion** | Solo el arreglo de 23 semillas |

Con n = 151, la diferencia entre O(n) y O(1) ya es medible, y creceria mas si se 
usa todos los Pokemones actuales (1025+) por ejemplo.

La ventaja del hash perfecto sobre una tabla hash generica no es la
complejidad asintotica, que ambas son O(1) en promedio sino la **garantia**: al
construirse local para un conjunto cerrado de claves, no puede haber
colisiones, asi que no hace falta encadenamiento, sondeo, factor de carga ni
rehash, y en el peor caso es igual de rapido que el promedio. Esto solo
funciona porque el conjunto de nombres no cambia, si cambiara,
`src/construir_hash.c` tendria que correr de nuevo para regenerar
`hash_config.bin`.

### Datos reales analisis de logs

Una corrida previa del programa incluye una comparacion directa hash vs
lineal, promediada sobre 3000 rondas × 151 claves cada una:

```
Buscar por iD     -> hash perfecto: 0.001468 us | lineal: 0.064718 us
Buscar por nombre -> hash perfecto: 0.096830 us | lineal: 0.888441 us
```

Esta comparacion, demuesra la gran diferencia que hay entre una busqueda
lineal y el hash perfecto diseñado, aunque es obvio que seria mas rapido
(O(n) vs O(1) xd), hace una alusion de la razon del porque se eligio para
el proyecto. esto viene de una corrida de una version anterior del codigo, 
pero las funciones de hash y busqueda que usa son las mismas que se sigue
usando, asi que los numeros siguen siendo representativos del beneficio real.

## 10. Compilar y probar

```sh
make          # compila ./main
make clean    # limpia binarios
```

Debe correrse desde la raiz del repositorio: tanto `DB/POKEDEX_DB.txt` como
`src/hash_config.bin` se abren con rutas relativas al directorio de trabajo
actual, no al ejecutable.


## 11. Validar antes de publicar (la Pokedex no acepta medias jornadas xd)

`cargar_pokedex_desde(ruta)` permite validar otro archivo con el mismo formato.
La carga requiere exactamente 151 especies, IDs unicos, tipos validos,
estadisticas positivas, ocho campos por registro y slots sin colisiones con
el hash precalculado. La conversion de numeros detecta texto sobrante y
valores fuera del rango de `int`.

Se construyen arreglos temporales y se publican solo cuando todo es valido.
Si falla una recarga, las consultas siguen usando el catalogo anterior.
Antes de la primera carga exitosa, ambas busquedas devuelven `NULL`.
Una especie inexistente ya no hereda por accidente el slot cero.

El catalogo de tipos se lee de `DB/EFECTIVIDAD.txt`. Las semillas del hash se
cargan antes del primer catalogo y no se sustituyen durante una recarga de
especies. El formato binario conserva `unsigned long` de la implementacion
original. Se entrega la configuracion de semillas; el ejecutable se compila
con Make. Consulta el [README](../README.md) para las plataformas verificadas
y los limites de portabilidad.


Las especies se leen con `leer_linea()`, compartido con los otros archivos.
La carga rechaza lineas incompletas por exceso de longitud antes de interpretar
campos. El criterio es el mismo que usan resultados y efectividad: un registro
fisico no se divide en varios registros logicos.
