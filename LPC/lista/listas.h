#include <stdio.h>
#include <stdlib.h>
#include "../nodo/nodo.h"
#include <stdbool.h>

#ifndef lista_h
#define lista_h

typedef struct {

	nodo *cabecera;
	int longitud;

}lista;

// Sección: Operaciones básicas de la lista
void lista_crear(lista *x);
nodo* lista_agregar(lista *x, dato dato);
void lista_extend(lista *x, dato arr[], int tam);
void lista_print(lista *x);
nodo* lista_insert(lista *x, dato dato, int pos);
void lista_remove(lista *x, dato dato);

// Sección: Consultas sobre la lista
bool lista_EsVacia(lista *x);
int lista_size(lista *x);
int lista_index(lista *x, dato dato);
int lista_count(lista *x, dato dato);
bool lista_pertenece(lista *x, dato dato);
bool lista_igual(lista *x, lista *y);
dato lista_get(lista *x, int pos);

// Sección: Operaciones de extracción y modificación
dato lista_pop(lista *x);
dato lista_popIndex(lista *x, int pos);
void lista_sort(lista *x);
void lista_reverse(lista *x);
void lista_clear(lista *x);
void lista_intercambiar(lista *x, int pos1, int pos2);

// Sección: Operaciones avanzadas y utilidades
lista lista_concat(lista *x, lista *y);
void lista_copy(lista *origen, lista *copia);
// Minimo y maximo: -1 ante lista vacia/ausente; consultar lista_EsVacia para distinguir ese dato.
dato lista_min(lista *x);
dato lista_max(lista *x);
// Suma anterior: -1 ante error; la variante segura distingue error de una suma negativa valida.
dato lista_sum(lista *x);
// Retorna 1 y publica la suma; 0 si falta la lista/salida o una suma parcial excede int.
int lista_sum_segura(lista *x, dato *resultado);
lista arreglo_a_lista(dato arr[], int tam);
lista lista_slice(lista *x, int inicio, int fin);

#endif
