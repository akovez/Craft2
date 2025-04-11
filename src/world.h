#ifndef _world_h_
#define _world_h_
#include "data.h"

typedef void (*world_func)(int, int, int, int, void *);

void create_world(int p, int q, world_func func, void *arg);
int get_block(int x, int y, int z);
void set_block(int x, int y, int z, int w);
void builder_block(int x, int y, int z, int w);
void dirty_chunk(Chunk *chunk);
void _set_block(int p, int q, int x, int y, int z, int w, int dirty);
void sphere(Block *center, int radius, int fill, int fx, int fy, int fz);
void cube(Block *b1, Block *b2, int fill);
void cylinder(Block *b1, Block *b2, int radius, int fill);
void array(Block *b1, Block *b2, int xc, int yc, int zc);
void tree(Block *block);
#endif
