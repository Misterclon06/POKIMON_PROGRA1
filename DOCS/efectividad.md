# Modulo Efectividad

## 1. Que resuelve

Relaciona el tipo de un ataque con los tipos del defensor. Fuego contra
Planta puede ser buena idea; Electrico contra Tierra sigue siendo una
decision emocional, no una estrategia xd.

El catalogo y las relaciones viven en `DB/EFECTIVIDAD.txt`. El codigo ya no
tiene escrita una matriz de 324 valores ni un arreglo de nombres de tipos.

## 2. Archivo de relaciones: guardar excepciones, consultar una matriz

El formato es:

```text
18
normal
fuego
... los otros nombres, uno por linea ...
hada
normal roca 0.5
normal fantasma 0
fuego planta 2
... resto de relaciones distintas de 1 ...
```

Las primeras 18 entradas definen el orden del catalogo. Cada relacion tiene
tipo atacante, tipo defensor y factor `0`, `0.5` o `2`. Una relacion omitida
vale x1. No se repiten pares.

Los 18 tipos se conservan porque el PDF los contempla, aunque la Pokedex de
primera generacion no tenga ejemplares de todos ellos.

La carga arma una matriz temporal con valores iniciales x1 y aplica las
excepciones. Solo publica nombres y matriz cuando todo el archivo es valido.
Una carga fallida conserva el catalogo anterior. Se rechazan nombres
duplicados, tipos desconocidos, registros malformados y factores no admitidos.
Tambien se rechazan lineas demasiado largas mediante el lector compartido
`leer_linea()`: la cabecera no puede completarse con el primer nombre
disfrazado de una segunda lectura xd. No hay fallback silencioso a una tabla
escrita en C.

## 3. Consultas y costo

La primera consulta carga el archivo; las siguientes usan memoria. Tambien
se puede cargar explicitamente:

```c
int cargar_efectividad();
int cargar_efectividad_desde(const char *ruta);
int obtener_tipo(const char *nombre);
int es_tipo_pokemon_valido(const char *nombre);
```

Las cargas retornan 1 al tener exito. `obtener_tipo()` retorna indice 0..17,
o -1 si no existe; reutiliza `tolower_string()` y libera la copia.
No distingue mayusculas. Los archivos usan nombres sin tildes; convertir a
minusculas no elimina acentos.

Resolver un nombre recorre como maximo 18 entradas. Con los indices, la
matriz se consulta directamente. Sacar datos del `.c` no obliga a buscar en
un archivo cada vez que alguien lanza un golpe.

## 4. Calcular efectividad

```c
int calcular_efectividad(const pokemon *atacante, const char *tipo_ataque,
    const pokemon *defensor, double *multiplicador);
```

Retorna 1 si tuvo exito; ante error no modifica la salida. El tipo elegido
debe ser propio del atacante. El segundo tipo vacio o `-` significa que no
hay segundo tipo. Para dos tipos defensores se multiplican ambos factores:

| Factores | Resultado |
|---|---:|
| 2 y 2 | x4 |
| 2 y 0.5 | x1 |
| 0.5 y 0.5 | x0.25 |
| Cualquiera y 0 | x0 |

Las filas son ataques y las columnas defensas. No se intercambian: que Fuego
le haga daño a Planta no significa que Planta tenga el mismo multiplicador
contra Fuego.

El modulo no aplica STAB, no calcula daño y no modifica HP. El ataque neutral
de emergencia lo administra el motor de combate, porque es una regla del
encuentro y no una relacion permanente entre tipos.

## 5. Datos de ejecucion

La aplicacion lee exclusivamente `DB/EFECTIVIDAD.txt`. Conserva el catalogo
anterior si falla una recarga: los tipos no se quedan a medio construir.
