#include <stdio.h>
#include <stdlib.h>
#include "pilas.h"


// Crea una pila vacía
void pila_crear(pila *x){

    if (x != NULL) {
        x->cabecera = NULL;
        x->longitud = 0;
    }
}

// Agrega un elemento al tope de la pila
void pila_apilar(pila *x, dato valor) {
    if (x != NULL) {
        nodo *nuevo = crearNodo(valor);
        if (nuevo != NULL) {
            nuevo->siguiente = x->cabecera;
            x->cabecera = nuevo;
            x->longitud++;
        }
    }
}


// desapila el elemento en el tope de la pila y lo devuelve
dato pila_desapilar(pila *x) {
    dato resultado = -1;
    if (x != NULL && x->longitud > 0) {
        nodo *eliminado = x->cabecera;
        resultado = eliminado->dato;
        x->cabecera = eliminado->siguiente;
        free(eliminado);
        x->longitud--;
    }
    return resultado;
}


// devuelve el elemento en el tope de la pila sin desapilarlo
dato pila_top(pila *x) {
    return x != NULL && x->longitud > 0 ? x->cabecera->dato : -1;
}


// devuelve el número de elementos en la pila
int pila_size(pila *x){
	return x != NULL ? x->longitud : 0;
}

// Extender conserva el orden de transferencia anterior: el origen se vacia
// y sus elementos se apilan en el destino. Una pila no se transfiere a si misma.
void pila_extender(pila *x, pila *y) {
    pila_transferir(y, x);
}

void pila_clear(pila *x) {
    if (x != NULL) {
        while (x->longitud > 0) pila_desapilar(x);
    }
}

// Los nodos ya existen. Reenlazarlos evita perder elementos si falla malloc
// entre desapilar y volver a apilar: una mudanza no necesita fabricar otra casa xd.
void pila_transferir(pila *origen, pila *destino) {
    if (origen != NULL && destino != NULL && origen != destino) {
        while (origen->cabecera != NULL) {
            nodo *movido = origen->cabecera;
            origen->cabecera = movido->siguiente;
            origen->longitud--;
            movido->siguiente = destino->cabecera;
            destino->cabecera = movido;
            destino->longitud++;
        }
    }
}

// Consultar recorre los nodos sin reservar, extraer ni reconstruir elementos.
int pila_count(pila *x, dato valor) {
    int cantidad = 0;
    const nodo *actual = x != NULL ? x->cabecera : NULL;
    while (actual != NULL) {
        if (actual->dato == valor) cantidad++;
        actual = actual->siguiente;
    }
    return cantidad;
}

// devuelve la posicion de la primera aparición de un valor específico en la pila, o -1 si no se encuentra
// El tope ocupa la posicion 1. Recorremos nodos sin desapilar: consultar un dato
// no necesita desmontar toda la torre y volverla a armar xd.
int pila_posicion(pila *x, dato valor) {
    int resultado = -1, posicion = 1;
    nodo *actual = x != NULL ? x->cabecera : NULL;
    while (actual != NULL && resultado == -1) {
        if (actual->dato == valor) resultado = posicion;
        actual = actual->siguiente;
        posicion++;
    }
    return resultado;
}


// devuelve true si la pila contiene un valor específico, false en caso contrario
bool pila_contiene(pila *x, dato valor) {
    return pila_posicion(x, valor) != -1;
}


// Copia en el mismo orden con nodos propios. Un fallo descarta la copia parcial;
// el original no se toca y una pila vacia nunca promete contener medio resultado.
pila pila_copy(pila *original) {
    pila nueva;
    pila_crear(&nueva);
    const nodo *actual = original != NULL ? original->cabecera : NULL;
    nodo *ultimo = NULL;
    int completa = 1;
    while (completa && actual != NULL) {
        nodo *nuevo = crearNodo(actual->dato);
        completa = nuevo != NULL;
        if (completa) {
            if (ultimo == NULL) nueva.cabecera = nuevo;
            else ultimo->siguiente = nuevo;
            ultimo = nuevo;
            nueva.longitud++;
            actual = actual->siguiente;
        }
    }
    if (!completa) pila_clear(&nueva);
    return nueva;
}

// invierte el orden de los elementos en la pila
void pila_reverse(pila *x){

	if (x == NULL || x->longitud == 0)
	{
		printf("la pila esta vacia \n");
	}else{
		
		pila pila_auxiliar1, pila_auxiliar2;
		pila_crear(&pila_auxiliar1);
		pila_crear(&pila_auxiliar2);
		
		pila_transferir(x, &pila_auxiliar1);
		pila_transferir(&pila_auxiliar1, &pila_auxiliar2);
		pila_transferir(&pila_auxiliar2, x);

	}
}

// apila los elementos de un array en la pila
void pila_apilar_array(pila *x, dato array[], int size){

	for (int i = 0; array != NULL && i < size; i++)
	{
		pila_apilar(x, array[i]);
	}
}

// crea una nueva pila a partir de un array de elementos
pila array_to_pila(dato array[], int size){

	pila nueva_pila;

	pila_crear(&nueva_pila);

	for (int i = 0; array != NULL && i < size; i++)
	{
		pila_apilar(&nueva_pila, array[i]);
	}

	return nueva_pila;
}

// devuelve true si la pila está vacía, false en caso contrario
bool pila_esVacia(pila *x) {
	return x == NULL || x->longitud == 0;
}

// imprime los elementos de la pila desde el tope hasta el fondo
void pila_print(pila *x){

	nodo *nodo_ant = x != NULL ? x->cabecera : NULL;

	printf("/ ");

	while(nodo_ant != NULL){
		printf("%d ", nodo_ant->dato);
		nodo_ant = nodo_ant->siguiente;
	}

	printf("| \n");

}
