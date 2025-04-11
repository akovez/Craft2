#ifndef _sign_h_
#define _sign_h_


#include "data.h"


void sign_list_alloc(SignList *list, int capacity);
void sign_list_free(SignList *list);
void sign_list_grow(SignList *list);
void sign_list_add(
    SignList *list, int x, int y, int z, int face, const char *text);
int sign_list_remove(SignList *list, int x, int y, int z, int face);
int sign_list_remove_all(SignList* list, int x, int y, int z);

#endif
