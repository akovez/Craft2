#include "config.h"
#include "../deps/noise/noise.h"
#include "world.h"

#include <math.h>

#include "client.h"
#include "data.h"
#include "db.h"
#include "item.h"
#include "main.h"
#include "map.h"
#include "util.h"

void create_world(int p, int q, world_func func, void *arg) {
    int pad = 1;
    for (int dx = -pad; dx < CHUNK_SIZE + pad; dx++) {
        for (int dz = -pad; dz < CHUNK_SIZE + pad; dz++) {
            int flag = 1;
            if (dx < 0 || dz < 0 || dx >= CHUNK_SIZE || dz >= CHUNK_SIZE) {
                flag = -1;
            }
            int x = p * CHUNK_SIZE + dx;
            int z = q * CHUNK_SIZE + dz;
            float f = simplex2(x * 0.01, z * 0.01, 4, 0.5, 2);
            float g = simplex2(-x * 0.01, -z * 0.01, 2, 0.9, 2);
            int mh = g * 32 + 16;
            int h = f * mh;
            int w = 1;
            int t = 12;
            if (h <= t) {
                h = t;
                w = 2;
            }
            // sand and grass terrain
            for (int y = 0; y < h; y++) {
                func(x, y, z, w * flag, arg);
            }
            if (w == 1) {
                if (SHOW_PLANTS) {
                    // grass
                    if (simplex2(-x * 0.1, z * 0.1, 4, 0.8, 2) > 0.6) {
                        func(x, h, z, 17 * flag, arg);
                    }
                    // flowers
                    if (simplex2(x * 0.05, -z * 0.05, 4, 0.8, 2) > 0.7) {
                        int w = 18 + simplex2(x * 0.1, z * 0.1, 4, 0.8, 2) * 7;
                        func(x, h, z, w * flag, arg);
                    }
                }
                // trees
                int ok = SHOW_TREES;
                if (dx - 4 < 0 || dz - 4 < 0 ||
                    dx + 4 >= CHUNK_SIZE || dz + 4 >= CHUNK_SIZE)
                {
                    ok = 0;
                }
                if (ok && simplex2(x, z, 6, 0.5, 2) > 0.84) {
                    for (int y = h + 3; y < h + 8; y++) {
                        for (int ox = -3; ox <= 3; ox++) {
                            for (int oz = -3; oz <= 3; oz++) {
                                int d = (ox * ox) + (oz * oz) +
                                    (y - (h + 4)) * (y - (h + 4));
                                if (d < 11) {
                                    func(x + ox, y, z + oz, 15, arg);
                                }
                            }
                        }
                    }
                    for (int y = h; y < h + 7; y++) {
                        func(x, y, z, 5, arg);
                    }
                }
            }
            // clouds
            if (SHOW_CLOUDS) {
                for (int y = 64; y < 72; y++) {
                    if (simplex3(
                        x * 0.01, y * 0.1, z * 0.01, 8, 0.5, 2) > 0.75)
                    {
                        func(x, y, z, 16 * flag, arg);
                    }
                }
            }
        }
    }
}

void array(Block *b1, Block *b2, int xc, int yc, int zc) {
    if (b1->w != b2->w) {
        return;
    }
    int w = b1->w;
    int dx = b2->x - b1->x;
    int dy = b2->y - b1->y;
    int dz = b2->z - b1->z;
    xc = dx ? xc : 1;
    yc = dy ? yc : 1;
    zc = dz ? zc : 1;
    for (int i = 0; i < xc; i++) {
        int x = b1->x + dx * i;
        for (int j = 0; j < yc; j++) {
            int y = b1->y + dy * j;
            for (int k = 0; k < zc; k++) {
                int z = b1->z + dz * k;
                builder_block(x, y, z, w);
            }
        }
    }
}

void cube(Block *b1, Block *b2, int fill) {
    if (b1->w != b2->w) {
        return;
    }
    int w = b1->w;
    int x1 = MIN(b1->x, b2->x);
    int y1 = MIN(b1->y, b2->y);
    int z1 = MIN(b1->z, b2->z);
    int x2 = MAX(b1->x, b2->x);
    int y2 = MAX(b1->y, b2->y);
    int z2 = MAX(b1->z, b2->z);
    int a = (x1 == x2) + (y1 == y2) + (z1 == z2);
    for (int x = x1; x <= x2; x++) {
        for (int y = y1; y <= y2; y++) {
            for (int z = z1; z <= z2; z++) {
                if (!fill) {
                    int n = 0;
                    n += x == x1 || x == x2;
                    n += y == y1 || y == y2;
                    n += z == z1 || z == z2;
                    if (n <= a) {
                        continue;
                    }
                }
                builder_block(x, y, z, w);
            }
        }
    }
}

void sphere(Block *center, int radius, int fill, int fx, int fy, int fz) {
    static const float offsets[8][3] = {
        {-0.5, -0.5, -0.5},
        {-0.5, -0.5, 0.5},
        {-0.5, 0.5, -0.5},
        {-0.5, 0.5, 0.5},
        {0.5, -0.5, -0.5},
        {0.5, -0.5, 0.5},
        {0.5, 0.5, -0.5},
        {0.5, 0.5, 0.5}
    };
    int cx = center->x;
    int cy = center->y;
    int cz = center->z;
    int w = center->w;
    for (int x = cx - radius; x <= cx + radius; x++) {
        if (fx && x != cx) {
            continue;
        }
        for (int y = cy - radius; y <= cy + radius; y++) {
            if (fy && y != cy) {
                continue;
            }
            for (int z = cz - radius; z <= cz + radius; z++) {
                if (fz && z != cz) {
                    continue;
                }
                int inside = 0;
                int outside = fill;
                for (int i = 0; i < 8; i++) {
                    float dx = x + offsets[i][0] - cx;
                    float dy = y + offsets[i][1] - cy;
                    float dz = z + offsets[i][2] - cz;
                    float d = sqrt(dx * dx + dy * dy + dz * dz);
                    if (d < radius) {
                        inside = 1;
                    }
                    else {
                        outside = 1;
                    }
                }
                if (inside && outside) {
                    builder_block(x, y, z, w);
                }
            }
        }
    }
}

void cylinder(Block *b1, Block *b2, int radius, int fill) {
    if (b1->w != b2->w) {
        return;
    }
    int w = b1->w;
    int x1 = MIN(b1->x, b2->x);
    int y1 = MIN(b1->y, b2->y);
    int z1 = MIN(b1->z, b2->z);
    int x2 = MAX(b1->x, b2->x);
    int y2 = MAX(b1->y, b2->y);
    int z2 = MAX(b1->z, b2->z);
    int fx = x1 != x2;
    int fy = y1 != y2;
    int fz = z1 != z2;
    if (fx + fy + fz != 1) {
        return;
    }
    Block block = {x1, y1, z1, w};
    if (fx) {
        for (int x = x1; x <= x2; x++) {
            block.x = x;
            sphere(&block, radius, fill, 1, 0, 0);
        }
    }
    if (fy) {
        for (int y = y1; y <= y2; y++) {
            block.y = y;
            sphere(&block, radius, fill, 0, 1, 0);
        }
    }
    if (fz) {
        for (int z = z1; z <= z2; z++) {
            block.z = z;
            sphere(&block, radius, fill, 0, 0, 1);
        }
    }
}

void tree(Block *block) {
    int bx = block->x;
    int by = block->y;
    int bz = block->z;
    for (int y = by + 3; y < by + 8; y++) {
        for (int dx = -3; dx <= 3; dx++) {
            for (int dz = -3; dz <= 3; dz++) {
                int dy = y - (by + 4);
                int d = (dx * dx) + (dy * dy) + (dz * dz);
                if (d < 11) {
                    builder_block(bx + dx, y, bz + dz, 15);
                }
            }
        }
    }
    for (int y = by; y < by + 7; y++) {
        builder_block(bx, y, bz, 5);
    }
}

void builder_block(int x, int y, int z, int w) {
    if (y <= 0 || y >= 256) {
        return;
    }
    if (is_destructable(get_block(x, y, z))) {
        set_block(x, y, z, 0);
    }
    if (w) {
        set_block(x, y, z, w);
    }
}

int get_block(int x, int y, int z) {
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->map;
        return map_get(map, x, y, z);
    }
    return 0;
}


void _set_block(int p, int q, int x, int y, int z, int w, int dirty) {
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->map;
        if (map_set(map, x, y, z, w)) {
            if (dirty) {
                dirty_chunk(chunk);
            }
            db_insert_block(p, q, x, y, z, w);
        }
    }
    else {
        db_insert_block(p, q, x, y, z, w);
    }
    if (w == 0 && chunked(x) == p && chunked(z) == q) {
        unset_sign(x, y, z);
        set_light(p, q, x, y, z, 0);
    }
}

void set_block(int x, int y, int z, int w) {
    int p = chunked(x);
    int q = chunked(z);
    _set_block(p, q, x, y, z, w, 1);
    for (int dx = -1; dx <= 1; dx++) {
        for (int dz = -1; dz <= 1; dz++) {
            if (dx == 0 && dz == 0) {
                continue;
            }
            if (dx && chunked(x + dx) == p) {
                continue;
            }
            if (dz && chunked(z + dz) == q) {
                continue;
            }
            _set_block(p + dx, q + dz, x, y, z, -w, 1);
        }
    }
    client_block(x, y, z, w);
}

int has_lights(Chunk *chunk) {
    if (!SHOW_LIGHTS) {
        return 0;
    }
    for (int dp = -1; dp <= 1; dp++) {
        for (int dq = -1; dq <= 1; dq++) {
            Chunk *other = chunk;
            if (dp || dq) {
                other = find_chunk(chunk->key.p + dp, chunk->key.q + dq);
            }
            if (!other) {
                continue;
            }
            Map *map = &other->lights;
            if (map->size) {
                return 1;
            }
        }
    }
    return 0;
}

void dirty_chunk(Chunk *chunk) {
    chunk->dirty = 1;
    if (has_lights(chunk)) {
        for (int dp = -1; dp <= 1; dp++) {
            for (int dq = -1; dq <= 1; dq++) {
                Chunk *other = find_chunk(chunk->key.p + dp, chunk->key.q + dq);
                if (other) {
                    other->dirty = 1;
                }
            }
        }
    }
}