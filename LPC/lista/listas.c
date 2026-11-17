#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include "listas.h"


// Incluso sin memoria queda un estado reconocible: cabecera NULL y longitud cero.
void lista_crear(lista *x) {
    if (x != NULL) {
        x->longitud = 0;
        x->cabecera = crearNodo(0);
    }
}

nodo *lista_agregar(lista *x, dato valor) {
    nodo *nuevo = NULL;
    if (x != NULL) nuevo = lista_insert(x, valor, x->longitud);
    return nuevo;
}

// Cero elementos no significa leer arr[0] por si acaso xd. Ante falta de memoria
// conserva el prefijo agregado y una longitud coherente; no enlaza nodos NULL.
void lista_extend(lista *x, dato arr[], int tam) {
    int valido = x != NULL && x->cabecera != NULL && arr != NULL && tam > 0;
    nodo *ultimo = valido ? x->cabecera : NULL;
    if (valido) while (ultimo->siguiente != NULL) ultimo = ultimo->siguiente;
    for (int i = 0; valido && i < tam; i++) {
        nodo *nuevo = crearNodo(arr[i]);
        valido = nuevo != NULL;
        if (valido) {
            ultimo->siguiente = nuevo;
            ultimo = nuevo;
            x->longitud++;
        }
    }
}

// inserta un elemento en una posicion
nodo *lista_insert(lista *x, dato valor, int pos) {
    nodo *nuevo = NULL;
    if (x != NULL && x->cabecera != NULL && pos >= 0 && pos <= x->longitud) {
        nodo *anterior = x->cabecera;
        for (int i = 0; i < pos; i++) anterior = anterior->siguiente;
        nuevo = crearNodo(valor);
        if (nuevo != NULL) {
            nuevo->siguiente = anterior->siguiente;
            anterior->siguiente = nuevo;
            x->longitud++;
        }
    }
    return nuevo;
}

// Elimina la primera ocurrencia del valor especificado
void lista_remove(lista *x, dato dato){

	lista_popIndex(x, lista_index(x, dato));

}

// Elimina y retorna el ultimo elemento
dato lista_pop(lista *x){
	return lista_popIndex(x, x != NULL ? x->longitud - 1 : -1);
}

// Elimina y retorna el elemento en la posición dada
dato lista_popIndex(lista *x, int pos) {
    dato resultado = -1;
    if (x != NULL && x->cabecera != NULL && pos >= 0 && pos < x->longitud) {
        nodo *anterior = x->cabecera;
        for (int i = 0; i < pos; i++) anterior = anterior->siguiente;
        nodo *eliminado = anterior->siguiente;
        anterior->siguiente = eliminado->siguiente;
        resultado = eliminado->dato;
        free(eliminado);
        x->longitud--;
    }
    return resultado;
}

// Copia el tramo [inicio, fin): incluye inicio y excluye fin, con indices desde 0.
// Los nodos son propios de la nueva lista; invalidar el rango devuelve una vacia.
// Quien recibe la copia debe liberarla; la lista original no se recorta.
lista lista_slice(lista *x, int inicio, int fin) {
    lista nueva;
    lista_crear(&nueva);
    if (x != NULL && x->cabecera != NULL && inicio >= 0 && inicio <= fin && fin <= x->longitud) {
        nodo *actual = x->cabecera->siguiente;
        for (int i = 0; i < inicio; i++) actual = actual->siguiente;
        int valido = nueva.cabecera != NULL;
        for (int i = inicio; valido && i < fin; i++) {
            valido = lista_agregar(&nueva, actual->dato) != NULL;
            actual = actual->siguiente;
        }
        if (!valido) lista_clear(&nueva);
    }
    return nueva;
}

// retorna el indice del dato a consultar
int lista_index(lista *x, dato valor) {
    int resultado = -1, posicion = 0;
    nodo *actual = x != NULL && x->cabecera != NULL ? x->cabecera->siguiente : NULL;
    while (actual != NULL && resultado == -1) {
        if (actual->dato == valor) resultado = posicion;
        actual = actual->siguiente;
        posicion++;
    }
    return resultado;
}

// ordena el arreglo con bubble sort
void lista_sort(lista *x) {

    int i, j;
	nodo *nodo_sig;


    for (i = 0; x != NULL && x->cabecera != NULL && i < x->longitud - 1; i++) {
		nodo_sig = x->cabecera;
		for (j = 0; j < x->longitud - i - 1; j++) {

			nodo_sig = nodo_sig->siguiente;

			if (nodo_sig->dato > nodo_sig->siguiente->dato) {
				swapNodo_dato(nodo_sig, nodo_sig->siguiente);
			}
		}
    }

}

// Anexa copias de nodos en una pasada. Quien llama decide si conservar la copia
// parcial cuando falta memoria; el origen siempre conserva sus propios nodos.
static int copiar_nodos(lista *destino, const lista *origen) {
    int completo = destino != NULL && destino->cabecera != NULL &&
        origen != NULL && origen->cabecera != NULL;
    nodo *ultimo = completo ? destino->cabecera : NULL;
    if (completo) while (ultimo->siguiente != NULL) ultimo = ultimo->siguiente;
    const nodo *actual = completo ? origen->cabecera->siguiente : NULL;
    while (completo && actual != NULL) {
        nodo *nuevo = crearNodo(actual->dato);
        completo = nuevo != NULL;
        if (completo) {
            ultimo->siguiente = nuevo;
            ultimo = nuevo;
            destino->longitud++;
            actual = actual->siguiente;
        }
    }
    return completo;
}

// Prepara la copia antes de vaciar el destino. Si falla una reserva, conserva
// ambos originales; copiar sobre la misma lista no tiene nada que hacer.
void lista_copy(lista *origen, lista *copia) {
    if (origen != NULL && copia != NULL && origen != copia &&
        origen->cabecera != NULL && copia->cabecera != NULL) {
        lista preparada;
        lista_crear(&preparada);
        if (copiar_nodos(&preparada, origen)) {
            lista_clear(copia);
            copia->cabecera->siguiente = preparada.cabecera->siguiente;
            copia->longitud = preparada.longitud;
            preparada.cabecera->siguiente = NULL;
            preparada.longitud = 0;
        }
        lista_clear(&preparada);
        free(preparada.cabecera);
    }
}

// Devuelve nodos independientes. Una reserva fallida descarta el resultado
// parcial; concatenar no puede anunciar media lista como si estuviera completa.
lista lista_concat(lista *x, lista *y) {
    lista nueva;
    lista_crear(&nueva);
    int completo = copiar_nodos(&nueva, x);
    if (completo) completo = copiar_nodos(&nueva, y);
    if (!completo) lista_clear(&nueva);
    return nueva;
}

// invertir lista
void lista_reverse(lista *x){

	nodo *anterior = NULL, *actual = x != NULL && x->cabecera != NULL ? x->cabecera->siguiente : NULL, *siguiente = NULL;
		while (actual != NULL) {
			siguiente = actual->siguiente;
			actual->siguiente = anterior;
			anterior = actual;
			actual = siguiente;
		}
		if (x != NULL && x->cabecera != NULL) x->cabecera->siguiente = anterior;

}

// intercambia dos elementos de la lista
void lista_intercambiar(lista *x, int pos1, int pos2){
	if (x == NULL || x->cabecera == NULL || pos1 >= x->longitud || pos1 < 0 || pos2 >= x->longitud || pos2 < 0)
	{
		printf("posicion a eliminar es invalida \n");
	}else{

		nodo *nodo_sig = x->cabecera;

		for(int i = 0; i < pos1; i++){
			nodo_sig = nodo_sig->siguiente;
		}

		nodo *nodo_pos1 = nodo_sig->siguiente;

		nodo_sig = x->cabecera;

		for(int i = 0; i < pos2; i++){
			nodo_sig = nodo_sig->siguiente;
		}

		nodo *nodo_pos2 = nodo_sig->siguiente;

		swapNodo_dato(nodo_pos1, nodo_pos2);
		
	}
	
}

// Cuenta sin modificar nodos; una lista ausente o vacia aporta cero ocurrencias.
int lista_count(lista *x, dato valor) {
    int cantidad = 0;
    const nodo *actual = x != NULL && x->cabecera != NULL ? x->cabecera->siguiente : NULL;
    while (actual != NULL) {
        if (actual->dato == valor) cantidad++;
        actual = actual->siguiente;
    }
    return cantidad;
}

// En una lista vacia no existe maximo/minimo: conservamos -1 como valor de ausencia.
// Si la lista admite -1 como dato, el llamador distingue el caso con lista_EsVacia().
dato lista_max(lista *x) {
    nodo *actual = x != NULL && x->cabecera != NULL ? x->cabecera->siguiente : NULL;
    dato maximo = actual != NULL ? actual->dato : -1;
    while (actual != NULL) {
        if (actual->dato > maximo) maximo = actual->dato;
        actual = actual->siguiente;
    }
    return maximo;
}

dato lista_min(lista *x) {
    nodo *actual = x != NULL && x->cabecera != NULL ? x->cabecera->siguiente : NULL;
    dato minimo = actual != NULL ? actual->dato : -1;
    while (actual != NULL) {
        if (actual->dato < minimo) minimo = actual->dato;
        actual = actual->siguiente;
    }
    return minimo;
}

// Verifica el margen ANTES de sumar: comprobar despues del desbordamiento llega tarde.
// Un fallo conserva la salida; una lista vacia valida suma cero.
int lista_sum_segura(lista *x, dato *resultado) {
    int valido = x != NULL && x->cabecera != NULL && resultado != NULL;
    dato suma = 0;
    nodo *actual = valido ? x->cabecera->siguiente : NULL;
    while (valido && actual != NULL) {
        dato valor = actual->dato;
        valido = !((valor > 0 && suma > INT_MAX - valor) ||
            (valor < 0 && suma < INT_MIN - valor));
        if (valido) {
            suma += valor;
            actual = actual->siguiente;
        }
    }
    if (valido) *resultado = suma;
    return valido;
}

// Interfaz anterior: -1 ante error. La variante segura distingue error de una suma -1.
dato lista_sum(lista *x) {
    dato suma = -1;
    lista_sum_segura(x, &suma);
    return suma;
}

// convierte un arreglo a una lista
lista arreglo_a_lista(dato arr[], int tam){

	lista nueva;
	lista_crear(&nueva);
	
	lista_extend(&nueva, arr, tam);

	return nueva;
	
}

// limpia la lista
void lista_clear(lista *x) {
    if (x != NULL && x->cabecera != NULL) {
        nodo *actual = x->cabecera->siguiente;
        while (actual != NULL) {
            nodo *siguiente = actual->siguiente;
            free(actual);
            actual = siguiente;
        }
        x->cabecera->siguiente = NULL;
        x->longitud = 0;
    }
}

// confirma si la lista esta vacia
bool lista_EsVacia(lista *x) {
    return x == NULL || x->longitud == 0;
}

// muestra por pantalla el tamaño de la lista
int lista_size(lista *x) {
    return x != NULL ? x->longitud : 0;
}

// confirma si un dato pertenece a la lista
bool lista_pertenece(lista *x, dato valor) {
    return lista_index(x, valor) != -1;
}

// confirma si dos listas son iguales
bool lista_igual(lista *x, lista *y) {
    int iguales = x != NULL && y != NULL;
    if (iguales) iguales = x->longitud == y->longitud && x->cabecera != NULL && y->cabecera != NULL;
    if (iguales) {
        nodo *a = x->cabecera->siguiente, *b = y->cabecera->siguiente;
        while (iguales && a != NULL && b != NULL) {
            iguales = a->dato == b->dato;
            a = a->siguiente;
            b = b->siguiente;
        }
        iguales = iguales && a == NULL && b == NULL;
    }
    return iguales;
}

// consulta el dato en la posicion dada
dato lista_get(lista *x, int pos) {
    dato resultado = -1;
    if (x != NULL && x->cabecera != NULL && pos >= 0 && pos < x->longitud) {
        nodo *actual = x->cabecera->siguiente;
        for (int i = 0; i < pos; i++) actual = actual->siguiente;
        resultado = actual->dato;
    }
    return resultado;
}


void lista_print(lista *x) {
    const nodo *actual = x != NULL && x->cabecera != NULL ? x->cabecera->siguiente : NULL;
    printf("[ ");
    while (actual != NULL) {
        printf("%d ", actual->dato);
        actual = actual->siguiente;
    }
    printf("]\n");
}
