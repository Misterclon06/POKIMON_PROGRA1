#include "colas.h"

void cola_crear(cola *x) {
    if (x != NULL) {
        x->inicio = crearNodo(0); // centinela: organiza la fila, no compite xd
        x->final = x->inicio;
        x->longitud = 0;
    }
}

void cola_encolar(cola *x, dato valor) {
    if (x != NULL && x->final != NULL) {
        nodo *nuevo = crearNodo(valor);
        if (nuevo != NULL) {
            x->final->siguiente = nuevo;
            x->final = nuevo;
            x->longitud++;
        }
    }
}

dato cola_desencolar(cola *x) {
    dato resultado = -1;
    if (x != NULL && x->inicio != NULL && x->longitud > 0) {
        nodo *primero = x->inicio->siguiente;
        resultado = primero->dato;
        x->inicio->siguiente = primero->siguiente;
        if (x->final == primero) x->final = x->inicio;
        free(primero);
        x->longitud--;
    }
    return resultado;
}

int cola_size(cola *x) {
    return x != NULL ? x->longitud : 0;
}

bool cola_esVacia(cola *x) {
    return cola_size(x) == 0;
}

int cola_count(cola *x, dato valor) {
    int cantidad = 0;
    nodo *actual = x != NULL && x->inicio != NULL ? x->inicio->siguiente : NULL;
    while (actual != NULL) {
        if (actual->dato == valor) cantidad++;
        actual = actual->siguiente;
    }
    return cantidad;
}

// Cuenta desde 1, sin incluir el centinela; -1 indica que no encontro el valor.
int cola_posicion(cola *x, dato valor) {
    int posicion = 0, encontrada = -1;
    nodo *actual = x != NULL && x->inicio != NULL ? x->inicio->siguiente : NULL;
    while (actual != NULL && encontrada == -1) {
        posicion++; // conserva la numeracion original desde 1
        if (actual->dato == valor) encontrada = posicion;
        actual = actual->siguiente;
    }
    return encontrada;
}

void cola_destruir(cola *x) {
    if (x != NULL) {
        nodo *actual = x->inicio;
        while (actual != NULL) {
            nodo *siguiente = actual->siguiente;
            free(actual);
            actual = siguiente;
        }
        x->inicio = NULL;
        x->final = NULL;
        x->longitud = 0;
    }
}

// Cada paso mueve el primero al final conservando el nodo. Normaliza la cantidad
// al largo de la cola; un valor negativo equivale al giro contrario.
void cola_rotacionar(cola *x, int cantidad) {
    if (x != NULL && x->longitud > 0) {
        cantidad %= x->longitud;
        if (cantidad < 0) cantidad += x->longitud;
        // Movemos el nodo, sin pedir memoria nueva para cambiarse de puesto.
        for (int i = 0; i < cantidad; i++) {
            nodo *primero = x->inicio->siguiente;
            x->inicio->siguiente = primero->siguiente;
            x->final->siguiente = primero;
            primero->siguiente = NULL;
            x->final = primero;
        }
    }
}

dato cola_frente(cola *x) {
    dato resultado = -1;
    if (x != NULL && x->inicio != NULL && x->longitud > 0) resultado = x->inicio->siguiente->dato;
    return resultado;
}

dato cola_final(cola *x) {
    dato resultado = -1;
    if (x != NULL && x->final != NULL && x->longitud > 0) resultado = x->final->dato;
    return resultado;
}

bool cola_contiene(cola *x, dato valor) {
    return cola_posicion(x, valor) != -1;
}

// Construye antes de sustituir. Si no alcanza la memoria, la copia anterior
// conserva sus nodos; un duplicado incompleto no se publica como si fuera entero.
void cola_copiar(cola *origen, cola *copia) {
    if (origen != NULL && copia != NULL && origen != copia && origen->inicio != NULL) {
        cola preparada;
        cola_crear(&preparada);
        int completa = preparada.inicio != NULL;
        nodo *actual = origen->inicio->siguiente;
        while (completa && actual != NULL) {
            int cantidad_anterior = preparada.longitud;
            cola_encolar(&preparada, actual->dato);
            completa = preparada.longitud == cantidad_anterior + 1;
            actual = actual->siguiente;
        }
        if (completa) {
            cola_destruir(copia);
            *copia = preparada;
        } else cola_destruir(&preparada);
    }
}

// Crea nodos nuevos en orden x seguido de y; las colas originales conservan
// sus elementos. Quien recibe el resultado debe destruirlo al terminar.
cola cola_concatenar(cola *x, cola *y) {
    cola resultado;
    cola_crear(&resultado);
    int completa = resultado.inicio != NULL;
    cola *origenes[] = {x, y};
    for (int i = 0; completa && i < 2; i++) {
        nodo *actual = origenes[i] != NULL && origenes[i]->inicio != NULL ? origenes[i]->inicio->siguiente : NULL;
        while (completa && actual != NULL) {
            int cantidad_anterior = resultado.longitud;
            cola_encolar(&resultado, actual->dato);
            completa = resultado.longitud == cantidad_anterior + 1;
            actual = actual->siguiente;
        }
    }
    if (!completa) cola_destruir(&resultado);
    return resultado;
}

void cola_encolar_array(cola *destino, dato valores[], int cantidad) {
    if (valores != NULL) {
        for (int i = 0; i < cantidad; i++) cola_encolar(destino, valores[i]);
    }
}

void cola_print(cola *x) {
    printf("<= ");
    nodo *actual = x != NULL && x->inicio != NULL ? x->inicio->siguiente : NULL;
    while (actual != NULL) {
        printf("%d ", actual->dato);
        actual = actual->siguiente;
    }
    puts("<=");
}
