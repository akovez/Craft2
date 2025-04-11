//
// Created by akovez on 4/7/25.
//

#ifndef MAIN_H
#define MAIN_H
#include "data.h"

extern  Model* game;

void add_message(const char *text);

void login();

void copy();

void paste();

void set_sign(int x, int y, int z, int face, const char *text);

void unset_sign(int x, int y, int z);

void set_light(int p, int q, int x, int y, int z, int w);

void record_block(int x, int y, int z, int w);

int player_intersects_block(
    int height,
    float x, float y, float z,
    int hx, int hy, int hz);
#endif //MAIN_H
