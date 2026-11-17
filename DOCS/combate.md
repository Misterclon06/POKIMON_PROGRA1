# Modulo Combate

## 1. Que hace

Dos entrenadores entran, uno sale con Pokemon disponibles. El otro conserva
sus ejemplares, pero con bastante menos autoestima y HP xd.


## 2. Especie, ejemplar y estado temporal

```text
Pokedex (stats base, intocable)
    |
    v
pokemon (ejemplar: nivel, stats, HP maximo)
    |
    v
posicion_combate
    +-- referencias a los dos entrenadores
    +-- HP actual por ejemplar
    +-- aumento elegido y sus usos
    +-- tres contadores de ataques gastados por ejemplar
    +-- activo y posibilidad de bloquear por entrenador
```

Las fichas se comparten por referencia. La recursion copia solo el estado
pequeño de combate, no los equipos completos. No se agregaron movimientos
ni reservas al `struct pokemon`.

Al terminar, se escribe el HP actual y el estado `normal`/`debilitado` en los
ejemplares. Las stats base y `hp_max` permanecen iguales. Cada nuevo combate
recupera HP y reservas. Un error de entrada durante el combate recupera HP;
un dato invalido detectado antes de comenzar no modifica el equipo.

## 3. Potencia: las cuatro tallas del golpe xd

| Potencia | Disponible desde nivel | Usos por ejemplar y combate |
|---|---:|---:|
| 25 | 1 | Ilimitados |
| 50 | 50 | 4 |
| 75 | 75 | 2 |
| 100 | 100 | 1 |

El nivel 25 no desbloquea otra potencia: la de 25 ya estaba disponible.
Un Pokemon nivel 50 tiene 25/50; uno de nivel 80 tiene 25/50/75.
No hay experiencia ni subida de nivel durante el torneo.

Las reservas se comparten entre sus dos tipos. Retirar al Pokemon no las
recupera. Un ataque ejecutado consume uso aunque sea bloqueado, esquivado o
inmune; si el Pokemon cae antes de ejecutar la accion, no consume nada.
El menu solo ofrece movimientos desbloqueados y con usos disponibles.

## 4. Daño y orden de acciones

```text
factor_nivel = floor(2 * nivel / 5) + 2
base = floor(floor(factor_nivel * potencia * ataque / defensa) / 50) + 2
daño_tipos = floor(base * efectividad)
daño_final = floor(daño_tipos * aleatorio / 255)    // aleatorio: 217..255
```
Fuente: rutinas [`CalculateDamage`](https://github.com/pret/pokered/blob/master/engine/battle/core.asm#L4188-L4349)
y [`RandomizeDamage`](https://github.com/pret/pokered/blob/master/engine/battle/core.asm#L5279-L5314)
de la reconstruccion del codigo de Pokemon Rojo/Azul del proyecto pret/pokered.

La formula de pokemon de toda la vida ps xd

Cada division de la base descarta su parte fraccionaria. Se suma 2 antes de
aplicar la efectividad; luego se aplica la variacion aleatoria. El codigo usa
intermediarios decimales y `floor()` para respetar esos pasos sin desbordar `int`.
Tras aplicar los tipos se conserva cero si hay inmunidad, o se impone minimo
uno en los demas casos, antes de sortear la variacion. Daño 0 o 1 no necesita
sorteo. No hay STAB ni criticos.

Esta adaptacion conserva las stats completas del proyecto y la efectividad
combinada de ambos tipos. No reproduce las limitaciones de registros, el escalado
de stats ni el tope de daño base de Rojo/Azul. El limite de salida
es el rango de `int`; al aplicarlo, el daño recibido tampoco supera el HP actual.

Las decisiones de ambos se recogen antes de resolver la ronda. Cambiar tiene
prioridad; el entrante puede recibir el golpe. Bloquear protege toda esa
ronda. Despues actua primero el Pokemon con mayor velocidad efectiva; si
empatan, se sortea. Un aumento de velocidad afecta el orden de rondas
posteriores, porque no tenemos presupuesto para viajar en el tiempo.

Si el primer ataque debilita al rival, su accion se cancela. El reemplazo
entra para la ronda siguiente; no hereda un ataque del difunto.

## 5. Bloqueo, aumentos y cambios

- Bloquear anula el ataque recibido y no puede hacerse en rondas consecutivas.
- Se aumenta una de las stats originalmente mas altas. Si empatan, el jugador
  tiene opciones separadas para elegir cual. El bot tambien puede explorarlas.
- Cada aumento suma 25% de la stat original: x1.25 y x1.50, maximo dos.
  No es interes compuesto de un banco xd.
- Los aumentos desaparecen al retirarse o caer; las reservas de ataques no.
- Los cambios voluntarios consumen la accion de la ronda.

## 6. Esquiva por velocidad

```text
porcentaje = parte entera de (10 * velocidad_defensor
                              / (velocidad_atacante + velocidad_defensor))
```
pues, por meter aleatoriedad y algo de sazon al asunto, metimos las esquivas.

A igual velocidad hay 5%. Se usan las velocidades efectivas, incluyendo
los aumentos. Un sorteo de 0..99 menor que ese porcentaje produce esquiva.
Dos velocidades cero dan 0%; no se divide entre cero para demostrar valentia.

Primero se comprueba bloqueo; si bloquea, no se sortea esquiva. Un ataque
esquivado causa cero de daño y conserva el gasto de su reserva.
La semilla se puede fijar por la API para repetir una pelea con las mismas
fichas y decisiones; cero solicita una semilla nueva.

## 7. Bot por backtracking: explorar, deshacer, probar otra

```text
probar secuencias de una ronda
    |
    +-- si ninguna derrota al activo rival: probar hasta dos rondas
            |
            +-- elegir accion legal del bot
            +-- suponer que el rival ataca con su mayor daño disponible
            +-- resolver una copia con el mismo motor
            +-- si el bot cae: descartar
            +-- si cae el rival: encontrada
            +-- si queda profundidad: continuar
            +-- volver y probar la siguiente accion
```
es un intento de un bot de pokemon, solo que lobotomizado, xd. investigue un rato
y vi cosas como heurística y cosas muy rebuscadas, asi que se me ocurrio hacerlo con
backtracking por los jajas. es mas pesado pero entra dentro de lo evaluado.

No suma bonos arbitrarios de riesgo, defensa o "ganas de vivir". El objetivo
es concreto: derrotar al activo rival dentro del horizonte y seguir vivo.
Prueba primero soluciones de una ronda; dentro de cada busqueda ofrece los
ataques de menor potencia antes de los mayores. No optimiza globalmente el
consumo de reservas ni promete la mejor estrategia de todo el combate.

Si no encuentra solucion, ataca con el mayor daño disponible. Si sus ataques
no pueden afectar al activo y un compañero si, cambia al primero que pueda.
Los iniciales y reemplazos automaticos usan el primer integrante vivo.

La prediccion asume golpes acertados y, si empatan en velocidad, deja
empezar al rival. No conoce la decision que acaba de elegir el usuario.
Despues de cada ronda vuelve a buscar con el estado real, incluyendo las
esquivas, el daño realmente recibido y las reservas gastadas. Backtracking no significa adivinar el futuro.

Hay como maximo 17 decisiones legales. La profundidad maxima es dos, con
hasta 323 nodos por decision contando las busquedas de una y dos rondas.

## 8. Empates por inmunidad y ataque de emergencia

Solo se declara empate cuando **ningun** Pokemon vivo de ninguno de los
equipos puede causar daño a ningun rival vivo. Se consideran tambien los que
estan en la banca. La comprobacion se repite al cambiar el conjunto de vivos,
no en cada ataque.

En grupos, `permitir_empate` habilita ese resultado. Alcanzar el limite de
rondas devuelve pendiente; una pelea larga no se convierte en empate.

En eliminatorias, ante bloqueo total se habilita un ataque
**neutral de emergencia de potencia 25**, es como el "forcejeo" de pokemon
cuando te quedas sin pp xd, ilimitado. Usa la misma formula
con efectividad x1 y conserva orden por velocidad, bloqueo y esquiva.
Es una excepcion al uso de los tipos propios para evitar un bloqueo por
inmunidades; no se ofrece
antes del bloqueo total. Si aun alcanza el limite tecnico, queda pendiente.

## 9. API y resultados

```c
opciones_combate opciones_combate_iniciales(modo_combate modo);
resultado_combate combatir_configurado(entrenador *primero, entrenador *segundo,
    const opciones_combate *opciones, resumen_combate *resumen);
```

Las opciones controlan modo, seleccion automatica, mensajes, esquiva, limite,
semilla, pausas, empates y ataque de emergencia. El limite inicial es de 1.000
rondas. `COMBATE_AUTOMATICO` selecciona tambien los reemplazos sin pedir teclado;
`COMBATE_CONTRA_BOT` automatiza el segundo lado y `seleccion_automatica`
automatiza ambos en los otros modos. En lados controlados automaticamente, `ataque_directo[2]` permite elegir
la politica de ataque directo por lado y compararla con el bot por backtracking.
Inicializa ambos
valores en cero para mantener el comportamiento habitual. No cambia decisiones
del jugador; en `COMBATE_AUTOMATICO` ambos lados siguen usando ataque directo.

| Resultado | Significado |
|---|---|
| `COMBATE_ERROR` | Datos o entrada invalidos |
| `COMBATE_GANADOR_ENTRENADOR_1` | Gana el primero |
| `COMBATE_GANADOR_ENTRENADOR_2` | Gana el segundo |
| `COMBATE_PENDIENTE` | Sin resultado oficial, por limite, pausa interrumpida o bloqueo no resuelto |
| `COMBATE_EMPATE` | Empate permitido y sin daño posible entre ambos equipos |

`resumen_combate` entrega rondas, ataques, esquivas, bloqueos, usos de cada
potencia limitada, nodos del bot y bajas por equipo. El torneo usa esas bajas
para clasificar; el motor no asigna puntos por su cuenta.

`calcular_dano_con_potencia()` recibe la potencia elegida y valida que el nivel
la tenga desbloqueada. El combate y el bot consultan la misma formula interna;
las reservas se comprueban en las decisiones legales y se gastan al ejecutar.

## Variacion de daño e interfaz por rondas

Los golpes reales aplican al daño ya ajustado por tipos un factor entero de 217 a 255 dividido
entre 255 y truncan el resultado. Daño 0 o 1 permanece igual. Es la variacion
usada en Rojo/Azul, documentada en la rutina original
[`RandomizeDamage`](https://github.com/pret/pokered/blob/master/engine/battle/core.asm#L5280).
El motor no aplica criticos ni STAB.

`variar_dano_combate()` consume la semilla de la pelea; la misma semilla y las
mismas decisiones reproducen el combate. Las consultas de daño base conservan
su resultado determinista. El backtracking del bot tampoco tira dados al
examinar ramas: la variacion solo se aplica al ejecutar rondas reales.
El bot estima con daño base y puede necesitar otro golpe si la tirada reduce
el daño. Predecir no es ver el futuro, por mucho backtracking que tenga xd.

La pantalla separa cada ronda, muestra ambos activos con HP actual/maximo y
barras ASCII, y presenta las acciones con usos restantes. Los mensajes distinguen
inmunidad, esquiva, bloqueo, aumento y debilitamiento. Al terminar se identifica
al ganador o se explica que el encuentro sigue pendiente.

