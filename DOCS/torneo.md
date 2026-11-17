# Modulo Torneo

## 1. claramente sabes que es lo dice el nombre

`torneo.c` administra entrenadores, encuentros, resultados y clasificacion.
El torneo del PDF tiene 32 entrenadores, ocho grupos de cuatro, 48 encuentros
de grupos y 16 eliminatorios. Los grupos se forman por orden de registro:
los primeros cuatro van a A, los siguientes a B, y asi hasta H.
Los equipos quedan cerrados al iniciar.

## 2. TDA

```text
torneo
  +-- entrenadores[32]      fichas y equipos
  +-- encuentros[64]        participantes, resultado y bajas
  +-- estadisticas[32]      solo fase de grupos
  +-- posiciones[8][4]      clasificacion de cada grupo
  +-- fuentes[16][2]        procedencia de los clasificados
  +-- pendientes           cola LPC de encuentros habilitados
```

Los participantes se guardan por ID; `torneo_indice_entrenador()` traduce a
posicion del arreglo. Un ID 400 no significa que haya que acceder al
entrenador numero 400. El arreglo no tiene tanta fe xd.

Hay una cola que contiene numeros de encuentros habilitados y sin resultado.
Al incorporar resultados se reconstruye con los que ya pueden jugarse.
Son como maximo 64: recalcular esta cola y la clasificacion mantiene simple
la consistencia sin estructuras avanzadas. No se copian nodos al copiar datos.
La cola se destruye al salir.

## 3. Registro y equipos

`torneo_registrar()` rechaza registro cerrado, capacidad excedida, IDs
invalidos o repetidos, equipos malformados y ejemplares repetidos por ID.
Dos entrenadores pueden tener la misma especie, pero no el mismo ejemplar.
Un registro inicial puede no tener equipo; para empezar el torneo todos
necesitan entre uno y seis Pokemon validos.

El constructor de ejemplares usa IDs correlativos independientes de la
especie. Al restaurar entrenadores se actualizan los contadores para que
los siguientes IDs no vuelvan a empezar en uno. Se permiten huecos: rechazar
una tirada de stats no obliga a reciclar una identidad.

## 4. Grupos y clasificacion

Cada grupo juega sus seis pares posibles, una sola vez. Victoria suma 3,
empate 1 y derrota 0. El orden de clasificacion aplica:

1. Mas puntos.
2. Mas victorias.
3. Mas Pokemon rivales derrotados.
4. Menos Pokemon propios derrotados.
5. Resultado directo o minitabla de los empatados.

Los tres primeros criterios vienen del enunciado. Los dos siguientes completan
el orden de clasificacion cuando aquellos no separan a los entrenadores. Las
reglas de empate por inmunidad y ataque de emergencia se explican en
[Combate](combate.md).

La minitabla usa solo encuentros entre los que siguen empatados y aplica
los mismos cuatro criterios en ese orden. No altera los puntos oficiales.
Si sigue existiendo igualdad, el grupo queda provisional: no se inventa un
clasificado por orden de registro. Mantenerlo pendiente evita
asignar un clasificado que las reglas actuales no pueden distinguir. Una
moneda lanzada a escondidas no cuenta como criterio xd.

Cada grupo debe tener sus seis resultados y un orden sin ambiguedad para
habilitar sus clasificados 1A/2A, 1B/2B, etc. La clasificacion se muestra
tambien mientras falten encuentros, identificada como provisional.

Los contadores `n_victorias`, `n_empates` y `n_derrotas` de la ficha incluyen
todos los encuentros resueltos; los puntos y la tabla de clasificacion
corresponden exclusivamente a grupos. Ganar la final no da tres puntos mas
para retroceder en el tiempo y cambiar la fase anterior.

## 5. Cruces exactos del PDF

`DB/CRUCES.txt` almacena las 16 parejas, desde 49 hasta 64. Las fuentes son:

- `1A`/`2A`: puesto y grupo.
- `G49`: ganador del encuentro 49.
- `P61`: perdedor de la semifinal 61.

Ejemplo:

```text
49 1A 2B
57 G49 G50
61 G57 G58
63 P61 P62
64 G61 G62
```

El archivo real contiene todos los cruces. No se reemplaza la peculiar
combinacion de cuartos del PDF por una llave que parezca mas bonita.
Antes de iniciar se valida la estructura: cada clasificado aparece una sola
vez en octavos, los cruces enfrentan primeros contra segundos de otro grupo,
y cada ganador se usa una sola vez en la ronda siguiente. Tercer puesto usa
perdedores de semifinales; final usa sus ganadores. Se rechazan fuentes de
otra ronda y plazas duplicadas antes de modificar el torneo. Las parejas
concretas permanecen en el archivo: validar no vuelve a escribir el calendario
en C por la puerta de atras xd.

Los participantes se resuelven automaticamente a partir de esos datos;
el usuario no puede escribir quienes pasan a una ronda.

En eliminatorias no se admite empate. El ataque neutral de emergencia se
habilita solo si ambos equipos estan completamente bloqueados por tipos.
Un limite tecnico sigue dejando el encuentro pendiente.

## 6. Resultados y validacion

```c
int torneo_registrar_resultado(torneo *liga, int numero, int primero,
    int segundo, int ganador, int bajas_primero, int bajas_segundo);
```

Retorna 1 si se registro. Valida encuentro existente, participantes
oficiales, ronda habilitada, resultado no registrado y ganador compatible.
En una victoria el perdedor debe haber perdido todo su equipo y el ganador
debe conservar al menos un integrante. Las bajas no pueden ser negativas
ni exceder los integrantes de cada equipo. En un empate ambos deben conservar
al menos un Pokemon: si uno ya no tiene disponibles, corresponde una victoria
del otro, no un empate de consolacion xd.

Un participante eliminado no puede aparecer en la siguiente ronda porque
las fuentes G/P determinan las plazas. La excepcion son los perdedores de
semifinales, enviados automaticamente al tercer puesto.

Los empates manuales de grupos se aceptan como resultados oficiales
externos; el archivo no contiene la secuencia de turnos para demostrar la
imposibilidad mutua de causar daño. Los combates ejecutados por el sistema
comprueban esa condicion directamente.

## 7. Archivos

### Resultados

Cada linea tiene siete campos:

```text
numero entrenador1 entrenador2 VICTORIA_o_EMPATE ganador bajas1 bajas2
1 10 20 VICTORIA 10 1 3
2 10 30 EMPATE 0 0 0
```

Los ejemplos muestran el formato; los IDs y las bajas deben corresponder a
la liga cargada. `ganador = 0` representa empate solo en grupos.
El archivo puede ser parcial y sus lineas pueden venir desordenadas.
Se validan y aplican por numero de encuentro para respetar dependencias.

Reimportar un registro exactamente igual no suma puntos de nuevo. Un
registro contradictorio se rechaza. Si cualquier linea falla, se recuperan
todos los resultados anteriores: nada de medio archivo aprobado por lastima.
Los resultados faltantes se pueden completar por teclado o jugando.

Los lectores de resultados, cruces, tipos y especies reutilizan `leer_linea()`.
Si una linea excede el buffer, se consume y se rechaza entera. No se interpreta
su siguiente fragmento como otro registro; de lo contrario una linea podria
colarse con dos resultados disfrazados de uno. Se conserva el estado anterior
cuando la carga falla.

### Entrenadores y equipos

El primer renglon es la cantidad. Por entrenador se guarda:

```text
ID cantidad_pokemon
Nombre completo
```

Por cada ejemplar se agregan tres lineas:

```text
ID nivel HP_max ataque defensa velocidad
Especie
Apodo completo
```

Los tipos se reconstruyen desde la Pokedex. Se conservan los IDs y las stats
exactas; se restaura HP maximo porque los encuentros empiezan recuperados.
Solo se importa sobre un registro vacio, antes de iniciar. Si falla, no se
incorpora una mitad de los entrenadores. Los resultados van en su archivo
separado y deben cargarse despues de iniciar el torneo restaurado.

### Clasificacion

Incluye grupo, posicion, ID, nombre, victorias, empates, derrotas, puntos y
Pokemon rivales derrotados y propios derrotados (bajas). La columna de bajas
tambien aparece en el menu, para poder comprobar el desempate por menos bajas. Las tablas incompletas o ambiguas se marcan
provisionales. La exportacion refleja la misma clasificacion que el menu.


### Completar los participantes

`completar_entrenadores()` vive en `src/preparacion_torneo.c`, fuera del TDA.
Recibe el torneo y las reglas actuales del generador de equipos. Admite un
registro vacio para organizar entre bots y exige equipo a todos los existentes. Completa
hasta 32, sin modificar a quienes ya estaban. Con 32 no requiere reglas ni
genera nada. Rechaza un torneo ya iniciado.

Los nuevos se llaman `Entrenador automatico <ID>`. Los IDs usan el constructor
compartido y se sincronizan con los mayores del registro, incluidos ejemplares.
Se pueden repetir especies entre entrenadores; no se admiten dentro del equipo
automatico. Identidad y especie siguen siendo cosas distintas, como siempre xd.

Primero se generan todos los nuevos y luego se registran por la API del torneo.
Si falla, se retiran los incorporados en esa llamada y se conserva el registro
original. Los contadores de identidad no retroceden: puede haber huecos, nunca
reciclaje silencioso. Devuelve los resultados de `resultado_generacion_equipo`.

El menu pide las reglas una sola vez para los rivales faltantes. Si un registrado
no tiene equipo, muestra nombre e ID y vuelve sin generar rivales. Si los rivales
se crearon pero despues falla el archivo de cruces, siguen registrados y el
torneo no comienza. Un reintento aprovecha esos participantes sin duplicarlos.

## Modos y avance por fases

Oak permite crear personaje, continuar directamente `save.txt`, importar
entrenadores de un archivo o entrar como organizador sin personaje.
El organizador usa jugador 0 y todos los encuentros son entre bots. El modo
jugador conserva el ID del personaje y pide sus decisiones en sus encuentros.
No hay que cargar un archivo de demostracion para poder organizar un torneo.

En Torneo, opcion 5 cambia entre ambos modos. Opcion 2 muestra el siguiente
encuentro completo; opcion 3 avanza solo la fase actual; opcion 6 recorre todo
el torneo entre bots. Las opciones 3 y 6 permiten resumen por encuentro o todos
los turnos. La fase de grupos termina en 48, octavos en 56, cuartos en 60,
semifinales en 62, tercer puesto en 63 y final en 64.

Cada resultado muestra ganador y bajas, incluso sin detalle de turnos. Tras los
grupos se muestra clasificacion. Una fase con empates de clasificacion pendientes
se informa sin inventar plazas. La opcion de siguiente fase vuelve al menu para
que puedas consultar la tabla antes de continuar; la final no se juega a escondidas xd.

## Partida unica: save.txt

`torneo_guardar_partida()` y `torneo_cargar_partida()` reciben la ruta; el menu
usa `save.txt`. Jugador 0 conserva el modo organizador. Un ID positivo conserva
el personaje. El guardado registra equipos y resultados; los contadores y la
cola se reconstruyen al restaurar para evitar guardar estados duplicados.

Formato versionado:

```text
POKIMON_SAVE 1
JUGADOR <ID o 0>
INICIADO <0 o 1>
ENTRENADORES
<cantidad y fichas en el formato habitual>
RESULTADOS
<cantidad de resultados>
<registros oficiales del formato habitual>
FIN_PARTIDA
```

Los lectores y escritores privados de `FILE *` se comparten con las funciones
de archivos anteriores. Sus formatos independientes siguen aceptandose.
La carga valida version, metadatos, equipos, resultados, final y ausencia de
contenido sobrante en una liga temporal con cola propia. Solo si todo es valido
reemplaza la liga y el jugador. No se reanudan turnos dentro de una pelea.

El guardado escribe primero `.tmp`, cierra el archivo y conserva el anterior
como `.bak` mientras lo sustituye. Si el reemplazo falla, intenta restaurar el
anterior; si ya existe un `.bak`, rechaza el reemplazo para no borrar un respaldo
que pudiera hacer falta. Los errores se informan y la partida en memoria sigue
disponible. Guardar manualmente esta en el menu principal (6) y Archivos (6).
Tambien se guarda al terminar una accion de Torneo y al salir con participantes.

Oak opcion 2 carga directamente el archivo y sus metadatos, sin solicitar ruta
ni ID otra vez. Si no existe o es invalido, informa el problema y conserva la
bienvenida. Archivos opcion 7 permite restaurarlo desde una sesion abierta.


## Consulta individual y seleccion de resultados

Entrenadores y equipos conserva la lista completa (4) y agrega ID (6) y nombre
(7). El nombre admite coincidencias parciales sin distinguir mayusculas; varias
coincidencias muestran sus IDs y permiten elegir una ficha. Se usa recorrido
lineal, apropiado para el limite de 32 participantes.

Registrar resultado por teclado (Torneo > 4) primero ofrece siguiente habilitado,
lista de la fase actual, numero o encuentros de un entrenador. Las listas muestran
nombres y estado. Antes del acta se muestra la ficha del encuentro. Una seleccion
fuera de la fase o del entrenador consultado se rechaza antes de pedir resultado.
Siguen vigentes las validaciones de rondas, ganador, bajas y resultado repetido.

En las simulaciones, Enter avanza una ronda si se muestran turnos y un encuentro
si se eligio resumen. El torneo completo espera tambien entre fases. Escribir 0
regresa al menu sin reanudar automaticamente otro encuentro o fase. Los resultados
ya registrados se conservan; una pelea interrumpida queda pendiente.

