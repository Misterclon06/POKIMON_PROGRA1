# Modulo Equipos automaticos

## 1. Descripcion general

Este modulo resuelve el punto 1.5: armar un equipo usando **recursividad y
backtracking**. Porque poner seis Pikachu en un arreglo era demasiado facil vrd.

Recibe unas reglas, busca especies que las cumplan y reemplaza el equipo de
un entrenador cuando encuentra una solucion completa. La Pokedex sigue siendo
la fuente de especies y Entidades sigue calculando las estadisticas. Aca se
decide **quienes entran al equipo**, no se inventa otro sistema de stats.

## 2. Arquitectura y estructura de archivos

```text
src/
  equipo_automatico.h    Reglas, resultados y funcion publica
  equipo_automatico.c    Candidatos, mezcla, recursion y podas
  pokedex.h             buscar_id(): especies reales de la base
  entidades.h           Generacion de stats e incorporacion al equipo
  efectividad.h         Catalogo compartido de tipos
  utils.h               gen_semilla() y alterador_semilla()
```

La Pokedex debe estar cargada **antes** de llamar al generador.

## 3. Estructuras

### `reglas_equipo_automatico`

| Campo | Para que sirve |
|---|---|
| `cantidad_pokemon` | Integrantes: entre 1 y `MAX_EQUIPO` (6) |
| `nivel_maximo_individual` | Tope de nivel, entre 1 y 100 |
| `nivel_comun` | `0`: niveles individuales aleatorios; mayor que cero: todos usan ese nivel |
| `tipos_distintos_minimos` | Diversidad minima contando tipos primarios y secundarios |

El nivel comun no puede superar el maximo individual. El minimo de tipos debe
estar entre 0 y 18 y no puede superar `2 * cantidad_pokemon`: un Pokemon puede
aportar como mucho dos tipos, no trae una tercera nacionalidad escondida.

me dio flojera poner mas restricciones la vrd xddd

### `contexto_generacion_equipo` (trabajo interno)

Guarda candidatos livianos, indices del equipo en prueba y de la solucion,
reglas, semilla local y cantidad de nodos visitados. Se crea dentro de la llamada; quien usa el modulo no tiene que armar este desastre a mano.

## 4. APi expuesta

```c
resultado_generacion_equipo generar_equipo_automatico(
    const reglas_equipo_automatico *reglas,
    entrenador *destino,
    unsigned long *nodos_visitados);
```

`reglas` y `destino` deben existir. `nodos_visitados` puede ser `NULL` si no
interesa contar las ramas. Si hay exito, se reemplaza el equipo y se actualiza
`n_pokemon`; nombre, id, puntuacion e historial del entrenador se conservan.

| Resultado | Significado |
|---|---|
| `GENERACION_EQUIPO_OK` | Equipo encontrado y guardado |
| `GENERACION_EQUIPO_ARGUMENTO_INVALIDO` | Punteros o reglas invalidas |
| `GENERACION_EQUIPO_DATOS_NO_DISPONIBLES` | Fallo al preparar candidatos o construir los ejemplares finales |
| `GENERACION_EQUIPO_SIN_SOLUCION` | No se encontro una combinacion valida |
| `GENERACION_EQUIPO_LIMITE_BUSQUEDA` | Se supero el limite de nodos |

**Ojo:** `GENERACION_EQUIPO_OK` vale **0**. Se compara con el enum; usarlo como
booleano y celebrar cuando da distinto de cero seria festejar el error xd.
Ante un fallo el equipo anterior se conserva. El contador de nodos se escribe
al terminar la busqueda, con exito, sin solucion o por limite; ante argumentos
invalidos o datos faltantes no se debe asumir que fue actualizado.

## 5. Funcion del algoritmo

```text
Pokedex cargada
      |
      v
preparar un candidato por especie (CANTIDAD = 151)
      |  referencia a especie + nivel + dos indices de tipos
      v
mezclar candidatos
      |
      v
construir_equipo_recursivo(contexto, posicion, siguiente_candidato)
      |  elegir candidato -> probar siguiente posicion
      |  no sirve -> volver y probar otro
      v
combinacion valida -> crear solo los ejemplares elegidos
      |  copia temporal del entrenador
      v
constructores correctos -> publicar equipo completo
```

### Candidatos y aleatoriedad

`crear_candidato()` consulta `buscar_id()` y prepara un `candidato_equipo` con
referencia a la especie, nivel y dos indices de tipos. Los nombres y stats base
siguen en la Pokedex: no se copian en 151 ejemplares que terminarian descartados.
`obtener_tipo()` se llama al preparar los candidatos, nunca dentro de las ramas.

El backtracking guarda los indices elegidos. **Sigue siendo backtracking**:
elige un candidato, actualiza contadores, explora recursivamente y deshace lo
agregado si esa rama falla. Encontrar la primera solucion no es un sorteo sin
validacion a lo loco xd.

Solo al encontrar una combinacion completa se llama a `crear_pokemon()` para
sus 1..6 integrantes. Se incorporan primero a una copia del entrenador; si falla
algun constructor, el equipo original se conserva. Las stats siguen usando la
formula y semillas de Entidades, sin copiar ejemplares de otro entrenador.

Con `nivel_comun == 0`, se sortea cada nivel entre 1 y el maximo permitido.
Si hay nivel comun, se usa directamente. Para varios equipos al mismo nivel,
se reutiliza el mismo valor en las reglas. Pasar cero dos veces no obliga a
que terminen al mismo nivel, eso seria telepatia.

`mezclar_candidatos()` intercambia cada posicion con una elegida entre las
posiciones pendientes, al estilo Fisher-Yates (me entere que se llamaba asi 
y lo queria flexear xd). Los niveles individuales y la mezcla usan una secuencia 
local que avanza con `alterador_semilla()`; las stats reciben semillas de `gen_semilla()`.

La mezcla evita favorecer siempre al primer ID. Se devuelve la **primera
solucion valida** encontrada: no se calcula el equipo mas poderoso ni se
garantiza que todas las combinaciones tengan la misma probabilidad.

### Contadores de tipos

El contexto guarda `veces_tipo[18]` y una cantidad de tipos distintos.
Al elegir un Pokemon se suman sus tipos; si un contador pasa de cero a uno,
aumenta la diversidad. Al retroceder se restan y se reduce la diversidad si
un contador vuelve a cero.

```text
Elegir Charizard: Fuego=1, Volador=1 -> 2 tipos
Elegir Pidgeot:   Fuego=1, Volador=2, Normal=1 -> 3 tipos
Retirar Pidgeot:  Fuego=1, Volador=1, Normal=0 -> 2 tipos
```

Volador no desaparece porque Pidgeot se fue: Charizard sigue pagando su
cuota del club de los que vuelan xd. Los indices se consultan una vez en el catalogo compartido con
`obtener_tipo()` y se guardan en cada candidato.

### Recursion, backtracking y podas

Cada llamada intenta llenar una posicion. La siguiente solo considera los
indices posteriores al candidato elegido: asi no se repiten especies ni se
prueban las mismas selecciones en todos sus ordenes posibles.

Si esa seleccion no lleva a una solucion, se vuelve a la posicion anterior y
se prueba otra especie. Eso es el backtracking: deshacer una mala decision,
habilidad que el codigo tiene y uno en la vida real a veces no xd.

Antes de seguir bajando se revisa:

1. Que queden suficientes especies para llenar los espacios.
2. Que sumando dos tipos por espacio todavia se pueda alcanzar el minimo.

Si falla cualquiera, se corta esa rama. El contador aumenta al entrar a cada
llamada recursiva; al alcanzar `LIMITE_NODOS_BUSQUEDA` (2.000.000), se devuelve
el resultado de limite, no una prueba de que no existe solucion.

La busqueda sigue siendo combinatoria en el peor caso. Las podas ahorran
trabajo y el limite acota las llamadas; ninguno convierte el algoritmo en O(1)
por arte de magia.

## 6. Ejecutar el generador

Compila con `make` desde la raiz y abre `./main` (`./main.exe` en Windows).
En **Entrenadores y equipos** puedes generar o reemplazar un equipo. Oak
ofrece el mismo formulario durante la creacion del personaje.

## Costo de crear y explorar candidatos

Se evita generar stats de 151 candidatos por llamada: solo se construyen
los 1..6 elegidos. Ademas, los arreglos de equipo en prueba y resultado guardan
indices enteros, no copias de estructuras `pokemon`.

Cada candidato guarda sus dos indices de tipos. Al elegir o
retroceder, la recursion suma y resta esos indices en los contadores, sin
repetir busquedas por nombre. No hay otra tabla hash, mascaras ni estructuras
avanzadas. La Pokedex se conserva y cada nuevo ejemplar mantiene ID propio.

Los candidatos no consumen IDs de ejemplares: solo los constructores de los
integrantes finales lo hacen. Si falla uno despues de haber creado otros,
puede haber huecos en los IDs; el equipo anterior sigue intacto. No se afirma
un porcentaje de aceleracion sin medirlo.
