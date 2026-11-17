CC := gcc
CFLAGS := -Wall -Wextra -O2
LDLIBS := -lm

SRC := main.c src/preparacion_torneo.c src/pokedex.c src/funcion_hasheo.c \
       src/entidades.c src/combate.c src/efectividad.c src/equipo_automatico.c \
       src/torneo.c LPC/colas/colas.c LPC/nodo/nodo.c
HEADERS := $(wildcard src/*.h) LPC/colas/colas.h LPC/nodo/nodo.h

# MinGW/MSYS2 agrega .exe: el objetivo debe coincidir con el archivo generado.
ifeq ($(OS),Windows_NT)
BIN := main.exe
else
BIN := main
endif

.PHONY: all clean

# Compila desde la raiz; la ejecucion se inicia con ./main o ./main.exe.
all: $(BIN)

$(BIN): $(SRC) $(HEADERS)
	$(CC) $(CFLAGS) $(SRC) -o $@ $(LDLIBS)

clean:
	rm -f main main.exe
