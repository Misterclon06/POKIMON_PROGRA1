# El Gran Torneo Pokémon

Un torneo completo en C para **Programación I**: consulta la Pokédex, crea tu
entrenador y su equipo, pelea contra el bot o administra los 32 participantes
hasta conocer al campeón y los otros tres integrantes del podio.


## Empezar

Necesitas **GCC, GNU Make y una terminal**. Ejecuta los comandos desde la raíz
del proyecto: las rutas de `DB/` y `src/hash_config.bin` se resuelven desde ese directorio.

En Linux:

```sh
make
./main
```

En Windows, usa el entorno **MSYS2 de 64 bits** comprobado por el proyecto:

```sh
make
./main.exe
```

**`make` compila; `./main` abre el juego.** Si aparece que no hay nada que hacer,
el ejecutable ya está actualizado. Para recompilar sin borrar datos:

```sh
make -B all
```

No es necesario reconstruir el hash para jugar. Se entregan el catálogo, los
cruces, la efectividad y las semillas que necesita el programa.

## Tu primera partida

Oak ofrece cuatro caminos al abrir:

| Opción | Para qué sirve |
|---|---|
| 1. Crear tu personaje | Jugar tus encuentros contra el bot; elegir equipo automático, un Pokémon manual o completarlo después |
| 2. Ya tengo datos guardados | Continuar desde `save.txt`, con equipos, resultados y personaje restaurados |
| 3. Importar entrenadores | Cargar tus entrenadores desde texto; ID 0 activa el modo organizador |
| 4. Organizar torneo automático | Administrar un torneo entre bots desde cero |

Para empezar sin preparar equipos: **opción 4 de Oak → Torneo → Iniciar torneo**.
Elige las reglas de los equipos y el programa completará los 32 participantes.
Después puedes jugar/ver el siguiente encuentro, avanzar una fase o simular
todo el torneo. El podio aparece en **Consultas → Mostrar podio** al resolver
tercer puesto y final.

Al iniciar con menos de 32 participantes, se preguntan reglas comunes para los
rivales faltantes. Se conservan los equipos registrados. Si algún entrenador
ya registrado tiene el equipo pendiente, debes completarlo antes de iniciar.

## Dónde está cada cosa

| Menú principal | Funciones |
|---|---|
| 1. Pokédex | Mostrar las 151 especies y buscar por número o nombre |
| 2. Entrenadores y equipos | Registrar, agregar Pokémon, generar/reemplazar equipos y buscar entrenadores por ID o nombre parcial |
| 3. Torneo | Iniciar, jugar/ver encuentros, avanzar fases, registrar resultados y alternar entre personaje y organizador |
| 4. Consultas | Clasificación, enfrentamientos, historial y podio |
| 5. Archivos | Importar/exportar entrenadores y resultados, exportar clasificación y guardar/restaurar partida |
| 6. Guardar partida | Guardar en `save.txt` |
| 0. Salir | Terminar la ejecución |

Cada submenú tiene **0. Volver**. El registro manual de resultados permite
buscar por encuentro o entrenador; los clasificados los calcula el programa.
No se puede colar un eliminado en la final porque venga recomendado xd.

## Cómo se juega un combate

Primero seleccionas un Pokémon vivo. En cada turno aparecen cuatro acciones
con números fijos:

1. **Atacar → tipo → potencia**: muestra las potencias disponibles y sus usos.
2. **Bloquear**: evita el ataque rival; no puedes hacerlo dos turnos seguidos.
3. **Aumentar estadística → estadística disponible**: hasta dos aumentos de 25%
   de la estadística original; solo entre las mayores del ejemplar.
4. **Cambiar → compañero vivo**: abre una lista propia con nivel y HP.

**0. Volver** cancela una selección dentro de los submenús sin gastar el turno.
Al cambiar se pierden los aumentos, pero se conservan los usos gastados. Las
reservas se comparten entre ambos tipos del ejemplar:

| Potencia | Se desbloquea | Usos por combate y Pokémon |
|---|---|---|
| 25 | Desde nivel 1 | Ilimitados |
| 50 | Nivel 50 | 4 |
| 75 | Nivel 75 | 2 |
| 100 | Nivel 100 | 1 |

Ataca primero el más rápido; la igualdad se sortea. El daño incluye tipos,
estadísticas y variación aleatoria. El bot explora hasta dos turnos mediante
backtracking. Consulta [Combate](DOCS/combate.md) para las fórmulas y excepciones.

Entre bots puedes elegir resultados resumidos o todos los turnos. En detalle,
**Enter avanza una ronda**; en resumen, un encuentro. El recorrido completo
espera también entre fases. **0 vuelve al menú**: una pelea interrumpida queda
pendiente y se reinicia al retomarla.

## Guardar y continuar

**Guardar partida** escribe `save.txt`. También hay guardado automático tras
las acciones del submenú Torneo y al salir con participantes registrados.
Se restaura entre encuentros, no en mitad de una ronda. El archivo es local y
no se versiona. Si falta o está corrupto, Oak explica el problema y permite
volver a elegir; una carga fallida conserva la partida actual.

Los archivos de entrenadores, resultados parciales y clasificación siguen
siendo independientes. Sus formatos y ejemplos están en
[Torneo y archivos](DOCS/torneo.md).

## Comandos de compilación

```sh
make                         # compilar lo que cambió
make -B all                  # recompilar toda la aplicación
make clean                   # eliminar main y main.exe
```

`make clean` conserva la DB y tu partida. Los archivos `save.txt`, `save.txt.tmp` y
`save.txt.bak` del guardado local se excluyen del control de versiones, igual que los
ejecutables y objetos generados.

La compilación habitual usa `-O2` para optimizar el ejecutable. Para estudiar
el código con el depurador, puedes recompilar sin optimización:

```sh
make -B all CFLAGS="-Wall -Wextra -g -O0"
```

Después, `make -B all` recupera la compilación habitual.

## Guía de documentación

| Documento | Qué explica |
|---|---|
| [Enunciado del profesor](<DOCS/Proyecto- 2026.pdf>) | Requisitos oficiales y evaluación |
| [Pokédex](DOCS/pokedex.md) | Lectura, validación y búsquedas con hash perfecto |
| [Entidades](DOCS/entidades.md) | Identidad de ejemplares, estadísticas y separación respecto a especies |
| [Equipos automáticos](DOCS/equipo_automatico.md) | Restricciones, poda y backtracking |
| [Efectividad](DOCS/efectividad.md) | Tipos y relaciones cargadas desde texto |
| [Combate](DOCS/combate.md) | Turnos, daño, reservas, aumentos, esquiva y bot |
| [Torneo](DOCS/torneo.md) | Grupos, cruces, desempates y formatos de archivos |
| [Biblioteca LPC](LPC/README.md) | Listas, pilas y colas propias |

`main.c` coordina la aplicación; `src/` contiene los módulos y las semillas del
hash. `DB/` contiene especies, efectividad y cruces. Las especies y relaciones
de tipos se leen desde esos archivos.

Las reglas de clasificacion, empate por inmunidad y ataque de emergencia se
explican en [Torneo](DOCS/torneo.md) y [Combate](DOCS/combate.md). Una igualdad
circular o un limite tecnico pueden dejar el torneo pendiente: las reglas
actuales no asignan un ganador arbitrario.
