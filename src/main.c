#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <curl/curl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "auth.h"
#include "client.h"
#include "config.h"
#include "cube.h"
#include "db.h"
#include "item.h"
#include "map.h"
#include "matrix.h"
#include "../deps/noise/noise.h"
#include "sign.h"
#include "../deps/tinycthread/tinycthread.h"
#include "util.h"
#include "world.h"
#include "main.h"
#include "input.h"

#include <immintrin.h>

//Define the Hash function before importing uthash so it will use own hash function

#define SZUDIK_PAIR(key, keylen, hashv) {\
    ChunkKey *_key = (ChunkKey*) (key); uint a = (_key->p >= 0.0 ? 2.0 * _key->p : (-2.0 * _key->p) - 1.0);\
    uint b = (_key->q >= 0.0 ? 2.0 * _key->q : (-2.0 * _key->q) - 1.0);\
    hashv = (a >= b ? (a * a) + a + b : (b * b) + a);\
}


#define HASH_PRIME(key,keylen,hashv)\
do {\
  unsigned int prime1 = 73856093;\
  unsigned int prime2 = 19349663;\
  const ChunkKey *_key=(const ChunkKey*)(key);\
  hashv = (_key->p * prime1) ^ (_key->q * prime2);\
} while (0)

#define HASH_FUNC(key,keylen,hasv) SZUDIKPAIR(key, keylen, hasv)


#include "uthash.h"

//  global game state. Yay!
Model model;
Model *game = &model;


float time_of_day() {
    if (game->day_length <= 0) {
        return 0.5;
    }
    float t;
    t = glfwGetTime();
    t = t / game->day_length;
    t = t - (int) t;
    return t;
}

float get_daylight() {
    float timer = time_of_day();
    if (timer < 0.5) {
        float t = (timer - 0.25) * 100;
        return 1 / (1 + powf(2, -t));
    } else {
        float t = (timer - 0.85) * 100;
        return 1 - 1 / (1 + powf(2, -t));
    }
}

int get_scale_factor() {
    int window_width, window_height;
    int buffer_width, buffer_height;
    glfwGetWindowSize(game->window, &window_width, &window_height);
    glfwGetFramebufferSize(game->window, &buffer_width, &buffer_height);
    int result = buffer_width / window_width;
    result = MAX(1, result);
    result = MIN(2, result);
    return result;
}


GLuint gen_crosshair_buffer() {
    int x = game->width / 2;
    int y = game->height / 2;
    int p = 10 * game->scale;
    float data[] = {
        x, y - p, x, y + p,
        x - p, y, x + p, y
    };
    return gen_buffer(sizeof(data), data);
}

GLuint gen_wireframe_buffer(float x, float y, float z, float n) {
    float data[72];
    make_cube_wireframe(data, x, y, z, n);
    return gen_buffer(sizeof(data), data);
}

GLuint gen_sky_buffer() {
    float data[12288];
    make_sphere(data, 1, 3);
    return gen_buffer(sizeof(data), data);
}

GLuint gen_cube_buffer(float x, float y, float z, float n, int w) {
    // GLfloat *data = malloc_faces(8, 6);
    void *data = malloc_faces_new_player(sizeof(VertexData), 6);
    float ao[6][4] = {0};
    float light[6][4] = {
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5},
        {0.5, 0.5, 0.5, 0.5}
    };
    //I dont get what ao is, it just leads to a buffer with zeroes everytime.
    make_cube_old((VertexData *) data, ao, light, 1, 1, 1, 1, 1, 1, x, y, z, n, w);
    return gen_faces_new(sizeof(VertexData), 6, data);
}

GLuint gen_plant_buffer(float x, float y, float z, float n, int w) {
    void *data = malloc_faces_new_player(sizeof(VertexData), 4);
    float ao = 0;
    float light = 1;
    make_plant_old((VertexData *) data, ao, light, x, y, z, n, w, 45);
    return gen_faces_new(sizeof(VertexData), 4, data);
}

GLuint gen_player_buffer(float x, float y, float z, float rx, float ry) {
    // GLfloat *data = malloc_faces(10, 6);
    VertexData *data = malloc_faces_new_player(sizeof(VertexData), 6);
    make_player(data, x, y, z, rx, ry);
    return gen_faces_new(sizeof(VertexData), 6, data);
}

GLuint gen_text_buffer(float x, float y, float n, char *text) {
    int length = strlen(text);
    GLfloat *data = malloc_faces(4, length);
    for (int i = 0; i < length; i++) {
        make_character(data + i * 24, x, y, n / 2, n, text[i]);
        x += n;
    }
    return gen_faces(4, length, data);
}

void draw_chunk_triangles_3d_ao_new(Attrib *attrib, GLuint buffer, GLuint indices_buffer, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_buffer);

    glEnableVertexAttribArray(attrib->uvts);
    glEnableVertexAttribArray(attrib->position_uint);
    // glEnableVertexAttribArray(attrib->uvScales);

    glVertexAttribPointer(attrib->position_uint, 1, GL_FLOAT, GL_FALSE, sizeof(VertexData),
                          (GLvoid *) (offsetof(VertexData, xyz)));
    glVertexAttribPointer(attrib->uvts, 1, GL_FLOAT, GL_FALSE, sizeof(VertexData),
                          (GLvoid *) (offsetof(VertexData, uvts)));
    // glVertexAttribPointer(attrib->uvScales, 1, GL_FLOAT, GL_FALSE, sizeof(VertexData), (GLvoid *)(offsetof(VertexData, uvScales)));


    //     GLenum error;
    // if ((error = glGetError()) != GL_NO_ERROR) {
    //     printf("OpenGL error after setting vertex attributes: %d\n", error);
    // }
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
    glDisableVertexAttribArray(attrib->uvts);
    glDisableVertexAttribArray(attrib->position_uint);
    // glDisableVertexAttribArray(attrib->uvScales);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void draw_chunk_triangles_3d_ao(Attrib *attrib, GLuint buffer, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->uvts);
    glEnableVertexAttribArray(attrib->position_uint);
    // glEnableVertexAttribArray(attrib->uvScales);

    glVertexAttribPointer(attrib->position_uint, 1, GL_FLOAT, GL_FALSE, sizeof(VertexData),
                          (GLvoid *) (offsetof(VertexData, xyz)));
    glVertexAttribPointer(attrib->uvts, 1, GL_FLOAT, GL_FALSE, sizeof(VertexData),
                          (GLvoid *) (offsetof(VertexData, uvts)));
    // glVertexAttribPointer(attrib->uvScales, 1, GL_FLOAT, GL_FALSE, sizeof(VertexData), (GLvoid *)(offsetof(VertexData, uvScales)));
    glDrawArrays(GL_TRIANGLES, 0, count);

    glDisableVertexAttribArray(attrib->uvts);
    glDisableVertexAttribArray(attrib->position_uint);
    // glDisableVertexAttribArray(attrib->uvScales);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}


void draw_triangles_3d_ao(Attrib *attrib, GLuint buffer, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->normal);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 3, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 10, 0);
    glVertexAttribPointer(attrib->normal, 3, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 10, (GLvoid *) (sizeof(GLfloat) * 3));
    glVertexAttribPointer(attrib->uv, 4, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 10, (GLvoid *) (sizeof(GLfloat) * 6));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->normal);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_triangles_3d_text(Attrib *attrib, GLuint buffer, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 3, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 5, 0);
    glVertexAttribPointer(attrib->uv, 2, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 5, (GLvoid *) (sizeof(GLfloat) * 3));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_triangles_3d(Attrib *attrib, GLuint buffer, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->normal);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 3, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 8, 0);
    glVertexAttribPointer(attrib->normal, 3, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 8, (GLvoid *) (sizeof(GLfloat) * 3));
    glVertexAttribPointer(attrib->uv, 2, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 8, (GLvoid *) (sizeof(GLfloat) * 6));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->normal);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_triangles_2d(Attrib *attrib, GLuint buffer, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glEnableVertexAttribArray(attrib->uv);
    glVertexAttribPointer(attrib->position, 2, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 4, 0);
    glVertexAttribPointer(attrib->uv, 2, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 4, (GLvoid *) (sizeof(GLfloat) * 2));
    glDrawArrays(GL_TRIANGLES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glDisableVertexAttribArray(attrib->uv);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_lines(Attrib *attrib, GLuint buffer, int components, int count) {
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glEnableVertexAttribArray(attrib->position);
    glVertexAttribPointer(
        attrib->position, components, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_LINES, 0, count);
    glDisableVertexAttribArray(attrib->position);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw_chunk(Attrib *attrib, Chunk *chunk) {
    //Create own function to deal with custom data types
    glUniform2f(attrib->chunk_pos, chunk->key.p, chunk->key.q);
    draw_chunk_triangles_3d_ao_new(attrib, chunk->buffer, chunk->indices_buffer, chunk->faces * 6);
    //draw_triangles_3d_ao(attrib, chunk->buffer, chunk->faces * 6);
}

void draw_item(Attrib *attrib, GLuint buffer, int count) {
    glUniform2f(attrib->chunk_pos, 0, 0);
    draw_chunk_triangles_3d_ao(attrib, buffer, count);
}

void draw_text(Attrib *attrib, GLuint buffer, int length) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_triangles_2d(attrib, buffer, length * 6);
    glDisable(GL_BLEND);
}

void draw_signs(Attrib *attrib, Chunk *chunk) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-8, -1024);
    draw_triangles_3d_text(attrib, chunk->sign_buffer, chunk->sign_faces * 6);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void draw_sign(Attrib *attrib, GLuint buffer, int length) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-8, -1024);
    draw_triangles_3d_text(attrib, buffer, length * 6);
    glDisable(GL_POLYGON_OFFSET_FILL);
}

void draw_cube(Attrib *attrib, GLuint buffer) {
    draw_item(attrib, buffer, 36);
}

void draw_plant(Attrib *attrib, GLuint buffer) {
    draw_item(attrib, buffer, 24);
}

void draw_player(Attrib *attrib, Player *player) {
    draw_cube(attrib, player->buffer);
}

Player *find_player(int id) {
    for (int i = 0; i < game->player_count; i++) {
        Player *player = game->players + i;
        if (player->id == id) {
            return player;
        }
    }
    return 0;
}

void update_player(Player *player,
                   float x, float y, float z, float rx, float ry, int interpolate) {
    if (interpolate) {
        State *s1 = &player->state1;
        State *s2 = &player->state2;
        memcpy(s1, s2, sizeof(State));
        s2->x = x;
        s2->y = y;
        s2->z = z;
        s2->rx = rx;
        s2->ry = ry;
        s2->t = glfwGetTime();
        if (s2->rx - s1->rx > PI) {
            s1->rx += 2 * PI;
        }
        if (s1->rx - s2->rx > PI) {
            s1->rx -= 2 * PI;
        }
    } else {
        State *s = &player->state;
        s->x = x;
        s->y = y;
        s->z = z;
        s->rx = rx;
        s->ry = ry;
        del_buffer(player->buffer);
        player->buffer = gen_player_buffer(s->x, s->y, s->z, s->rx, s->ry);
    }
}

void interpolate_player(Player *player) {
    State *s1 = &player->state1;
    State *s2 = &player->state2;
    float t1 = s2->t - s1->t;
    float t2 = glfwGetTime() - s2->t;
    t1 = MIN(t1, 1);
    t1 = MAX(t1, 0.1);
    float p = MIN(t2 / t1, 1);
    update_player(
        player,
        s1->x + (s2->x - s1->x) * p,
        s1->y + (s2->y - s1->y) * p,
        s1->z + (s2->z - s1->z) * p,
        s1->rx + (s2->rx - s1->rx) * p,
        s1->ry + (s2->ry - s1->ry) * p,
        0);
}

void delete_player(int id) {
    Player *player = find_player(id);
    if (!player) {
        return;
    }
    int count = game->player_count;
    del_buffer(player->buffer);
    Player *other = game->players + (--count);
    memcpy(player, other, sizeof(Player));
    game->player_count = count;
}

void delete_all_players() {
    for (int i = 0; i < game->player_count; i++) {
        Player *player = game->players + i;
        del_buffer(player->buffer);
    }
    game->player_count = 0;
}

float player_player_distance(Player *p1, Player *p2) {
    State *s1 = &p1->state;
    State *s2 = &p2->state;
    float x = s2->x - s1->x;
    float y = s2->y - s1->y;
    float z = s2->z - s1->z;
    return sqrtf(x * x + y * y + z * z);
}

float player_crosshair_distance(Player *p1, Player *p2) {
    State *s1 = &p1->state;
    State *s2 = &p2->state;
    float d = player_player_distance(p1, p2);
    float vx, vy, vz;
    get_sight_vector(s1->rx, s1->ry, &vx, &vy, &vz);
    vx *= d;
    vy *= d;
    vz *= d;
    float px = s1->x + vx;
    float py = s1->y + vy;
    float pz = s1->z + vz;
    float x = s2->x - px;
    float y = s2->y - py;
    float z = s2->z - pz;
    return sqrtf(x * x + y * y + z * z);
}

Player *player_crosshair(Player *player) {
    Player *result = 0;
    float threshold = RADIANS(5);
    float best = 0;
    for (int i = 0; i < game->player_count; i++) {
        Player *other = game->players + i;
        if (other == player) {
            continue;
        }
        float p = player_crosshair_distance(player, other);
        float d = player_player_distance(player, other);
        if (d < 96 && p / d < threshold) {
            if (best == 0 || d < best) {
                best = d;
                result = other;
            }
        }
    }
    return result;
}


#if 1
int chunk_visible(__m256 planes[6][4], int p, int q, int miny, int maxy) {
    int x = p * CHUNK_SIZE - 1;
    int z = q * CHUNK_SIZE - 1;
    int d = CHUNK_SIZE + 1;
    const __m256 d1 = _mm256_set_ps(0, d, 0, d, 0, d, 0, d);
    const __m256 d2 = _mm256_set_ps(0, 0, d, d, 0, 0, d, d);

    __m256 points_x = _mm256_add_ps(_mm256_set1_ps(x), d1);
    __m256 points_y = _mm256_set_ps(miny, miny, miny, miny, maxy, maxy, maxy, maxy);
    __m256 points_z = _mm256_add_ps(_mm256_set1_ps(z), d2);

    int n = game->ortho ? 4 : 6;
    for (int i = 0; i < n; i++) {
        __m256 res_x = _mm256_mul_ps(planes[i][0], points_x);
        __m256 res_y = _mm256_mul_ps(planes[i][1], points_y);
        __m256 res_z = _mm256_mul_ps(planes[i][2], points_z);

        __m256 res_tot = _mm256_add_ps(_mm256_add_ps(_mm256_add_ps(res_x, res_y), res_z), planes[i][3]);
        __m256 res_mask = _mm256_cmp_ps(res_tot, _mm256_setzero_ps(), _CMP_GE_OQ);
        int mask = _mm256_movemask_ps(res_mask);
        if (mask == 0)
            return 0;
    }
    return 1;
}
#else
int chunk_visible(float planes[6][4], int p, int q, int miny, int maxy) {
    int x = p * CHUNK_SIZE - 1;
    int z = q * CHUNK_SIZE - 1;
    int d = CHUNK_SIZE + 1;
    float points[8][3] = {
        {x + 0, miny, z + 0},
        {x + d, miny, z + 0},
        {x + 0, miny, z + d},
        {x + d, miny, z + d},
        {x + 0, maxy, z + 0},
        {x + d, maxy, z + 0},
        {x + 0, maxy, z + d},
        {x + d, maxy, z + d}
    };
    int n = g->ortho ? 4 : 6;
    for (int i = 0; i < n; i++) {
        int in = 0;
        int out = 0;
        for (int j = 0; j < 8; j++) {
            float d =
                planes[i][0] * points[j][0] +
                planes[i][1] * points[j][1] +
                planes[i][2] * points[j][2] +
                planes[i][3];
            if (d < 0) {
                out++;
            }
            else {
                in++;
                break;
            }
        }
        if (in == 0) {
            return 0;
        }
    }
    return 1;
}
#endif


int player_intersects_block(
    int height,
    float x, float y, float z,
    int hx, int hy, int hz) {
    int nx = roundf(x);
    int ny = roundf(y);
    int nz = roundf(z);
    for (int i = 0; i < height; i++) {
        if (nx == hx && ny - i == hy && nz == hz) {
            return 1;
        }
    }
    return 0;
}

int _gen_sign_buffer(
    GLfloat *data, float x, float y, float z, int face, const char *text) {
    static const int glyph_dx[8] = {0, 0, -1, 1, 1, 0, -1, 0};
    static const int glyph_dz[8] = {1, -1, 0, 0, 0, -1, 0, 1};
    static const int line_dx[8] = {0, 0, 0, 0, 0, 1, 0, -1};
    static const int line_dy[8] = {-1, -1, -1, -1, 0, 0, 0, 0};
    static const int line_dz[8] = {0, 0, 0, 0, 1, 0, -1, 0};
    if (face < 0 || face >= 8) {
        return 0;
    }
    int count = 0;
    float max_width = 64;
    float line_height = 1.25;
    char lines[1024];
    int rows = wrap(text, max_width, lines, 1024);
    rows = MIN(rows, 5);
    int dx = glyph_dx[face];
    int dz = glyph_dz[face];
    int ldx = line_dx[face];
    int ldy = line_dy[face];
    int ldz = line_dz[face];
    float n = 1.0 / (max_width / 10);
    float sx = x - n * (rows - 1) * (line_height / 2) * ldx;
    float sy = y - n * (rows - 1) * (line_height / 2) * ldy;
    float sz = z - n * (rows - 1) * (line_height / 2) * ldz;
    char *key;
    char *line = tokenize(lines, "\n", &key);
    while (line) {
        int length = strlen(line);
        int line_width = string_width(line);
        line_width = MIN(line_width, max_width);
        float rx = sx - dx * line_width / max_width / 2;
        float ry = sy;
        float rz = sz - dz * line_width / max_width / 2;
        for (int i = 0; i < length; i++) {
            int width = char_width(line[i]);
            line_width -= width;
            if (line_width < 0) {
                break;
            }
            rx += dx * width / max_width / 2;
            rz += dz * width / max_width / 2;
            if (line[i] != ' ') {
                make_character_3d(
                    data + count * 30, rx, ry, rz, n / 2, face, line[i]);
                count++;
            }
            rx += dx * width / max_width / 2;
            rz += dz * width / max_width / 2;
        }
        sx += n * line_height * ldx;
        sy += n * line_height * ldy;
        sz += n * line_height * ldz;
        line = tokenize(NULL, "\n", &key);
        rows--;
        if (rows <= 0) {
            break;
        }
    }
    return count;
}

void gen_sign_buffer(Chunk *chunk) {
    SignList *signs = &chunk->signs;

    // first pass - count characters
    int max_faces = 0;
    for (int i = 0; i < signs->size; i++) {
        Sign *e = signs->data + i;
        max_faces += strlen(e->text);
    }

    // second pass - generate geometry
    GLfloat *data = malloc_faces(5, max_faces);
    int faces = 0;
    for (int i = 0; i < signs->size; i++) {
        Sign *e = signs->data + i;
        faces += _gen_sign_buffer(
            data + faces * 30, e->x, e->y, e->z, e->face, e->text);
    }

    del_buffer(chunk->sign_buffer);
    chunk->sign_buffer = gen_faces(5, faces, data);
    chunk->sign_faces = faces;
}


void occlusion_greedy(int face_dir, char self_light,
                      char neighbors[9], char lights[9], float shades[9],
                      float ao[4], float light[4]) {
    static const int lookup3[6][4][3] = {
        {{0, 1, 3}, {2, 1, 5}, {6, 3, 7}, {8, 5, 7}},
        {{18, 19, 21}, {20, 19, 23}, {24, 21, 25}, {26, 23, 25}},
        {{0, 1, 3}, {2, 1, 5}, {6, 3, 7}, {8, 5, 7}},
        //{{6, 7, 15}, {8, 7, 17}, {24, 15, 25}, {26, 17, 25}},
        {{0, 1, 9}, {2, 1, 11}, {18, 9, 19}, {20, 11, 19}},
        {{0, 3, 9}, {6, 3, 15}, {18, 9, 21}, {24, 15, 21}},
        {{2, 5, 11}, {8, 5, 17}, {20, 11, 23}, {26, 17, 23}}
    };
    static const int lookup4[6][4][4] = {
        {{0, 1, 3, 4}, {1, 2, 4, 5}, {3, 4, 6, 7}, {4, 5, 7, 8}},
        {{18, 19, 21, 22}, {19, 20, 22, 23}, {21, 22, 24, 25}, {22, 23, 25, 26}},
        //{{6, 7, 15, 16}, {7, 8, 16, 17}, {15, 16, 24, 25}, {16, 17, 25, 26}},
        {{0, 1, 3, 4}, {1, 2, 4, 5}, {3, 4, 6, 7}, {4, 5, 7, 8}},
        {{0, 1, 9, 10}, {1, 2, 10, 11}, {9, 10, 18, 19}, {10, 11, 19, 20}},
        {{0, 3, 9, 12}, {3, 6, 12, 15}, {9, 12, 18, 21}, {12, 15, 21, 24}},
        {{2, 5, 11, 14}, {5, 8, 14, 17}, {11, 14, 20, 23}, {14, 17, 23, 26}}
    };
    static const float curve[4] = {0.0, 0.25, 0.5, 0.75};
    for (int j = 0; j < 4; j++) {
        int corner = neighbors[lookup3[face_dir][j][0]];
        int side1 = neighbors[lookup3[face_dir][j][1]];
        int side2 = neighbors[lookup3[face_dir][j][2]];
        int value = side1 && side2 ? 3 : corner + side1 + side2;
        float shade_sum = 0;
        float light_sum = 0;
        int is_light = self_light == 15;
        for (int k = 0; k < 4; k++) {
            shade_sum += shades[lookup4[face_dir][j][k]];
            light_sum += lights[lookup4[face_dir][j][k]];
        }
        if (is_light) {
            light_sum = 15 * 4 * 10;
        }
        float total = curve[value] + shade_sum / 4.0;
        ao[j] = MIN(total, 1.0);
        light[j] = light_sum / 15.0 / 4.0;
    }
}

void occlusion(
    char neighbors[27], char lights[27], float shades[27],
    float ao[6][4], float light[6][4]) {
    static const int lookup3[6][4][3] = {
        {{0, 1, 3}, {2, 1, 5}, {6, 3, 7}, {8, 5, 7}},
        {{18, 19, 21}, {20, 19, 23}, {24, 21, 25}, {26, 23, 25}},
        {{6, 7, 15}, {8, 7, 17}, {24, 15, 25}, {26, 17, 25}},
        {{0, 1, 9}, {2, 1, 11}, {18, 9, 19}, {20, 11, 19}},
        {{0, 3, 9}, {6, 3, 15}, {18, 9, 21}, {24, 15, 21}},
        {{2, 5, 11}, {8, 5, 17}, {20, 11, 23}, {26, 17, 23}}
    };
    static const int lookup4[6][4][4] = {
        {{0, 1, 3, 4}, {1, 2, 4, 5}, {3, 4, 6, 7}, {4, 5, 7, 8}},
        {{18, 19, 21, 22}, {19, 20, 22, 23}, {21, 22, 24, 25}, {22, 23, 25, 26}},
        {{6, 7, 15, 16}, {7, 8, 16, 17}, {15, 16, 24, 25}, {16, 17, 25, 26}},
        {{0, 1, 9, 10}, {1, 2, 10, 11}, {9, 10, 18, 19}, {10, 11, 19, 20}},
        {{0, 3, 9, 12}, {3, 6, 12, 15}, {9, 12, 18, 21}, {12, 15, 21, 24}},
        {{2, 5, 11, 14}, {5, 8, 14, 17}, {11, 14, 20, 23}, {14, 17, 23, 26}}
    };
    static const float curve[4] = {0.0, 0.25, 0.5, 0.75};
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 4; j++) {
            int corner = neighbors[lookup3[i][j][0]];
            int side1 = neighbors[lookup3[i][j][1]];
            int side2 = neighbors[lookup3[i][j][2]];
            int value = side1 && side2 ? 3 : corner + side1 + side2;
            float shade_sum = 0;
            float light_sum = 0;
            int is_light = lights[13] == 15;
            for (int k = 0; k < 4; k++) {
                shade_sum += shades[lookup4[i][j][k]];
                light_sum += lights[lookup4[i][j][k]];
            }
            if (is_light) {
                light_sum = 15 * 4 * 10;
            }
            float total = curve[value] + shade_sum / 4.0;
            ao[i][j] = MIN(total, 1.0);
            light[i][j] = light_sum / 15.0 / 4.0;
        }
    }
}

#define XZ_SIZE (CHUNK_SIZE * 3 + 2)
#define XZ_LO (CHUNK_SIZE)
#define XZ_HI (CHUNK_SIZE * 2 + 1)
#define Y_SIZE 258
#define XYZ(x, y, z) ((y) * XZ_SIZE * XZ_SIZE + (x) * XZ_SIZE + (z))
#define XZ(x, z) ((x) * XZ_SIZE + (z))

void light_fill(
    char *opaque, char *light,
    int x, int y, int z, int w, int force) {
    if (x + w < XZ_LO || z + w < XZ_LO) {
        return;
    }
    if (x - w > XZ_HI || z - w > XZ_HI) {
        return;
    }
    if (y < 0 || y >= Y_SIZE) {
        return;
    }
    if (light[XYZ(x, y, z)] >= w) {
        return;
    }
    if (!force && opaque[XYZ(x, y, z)]) {
        return;
    }
    light[XYZ(x, y, z)] = w--;
    light_fill(opaque, light, x - 1, y, z, w, 0);
    light_fill(opaque, light, x + 1, y, z, w, 0);
    light_fill(opaque, light, x, y - 1, z, w, 0);
    light_fill(opaque, light, x, y + 1, z, w, 0);
    light_fill(opaque, light, x, y, z - 1, w, 0);
    light_fill(opaque, light, x, y, z + 1, w, 0);
}


void compute_chunk_greedy(WorkerItem *item) {
    char *opaque = (char *) calloc(XZ_SIZE * XZ_SIZE * Y_SIZE, sizeof(char));
    char *light = (char *) calloc(XZ_SIZE * XZ_SIZE * Y_SIZE, sizeof(char));
    char *highest = (char *) calloc(XZ_SIZE * XZ_SIZE, sizeof(char));

    int ox = item->p * CHUNK_SIZE - CHUNK_SIZE - 1;
    int oy = -1;
    int oz = item->q * CHUNK_SIZE - CHUNK_SIZE - 1;

    // check for lights
    int has_light = 0;
    if (SHOW_LIGHTS) {
        for (int a = 0; a < 3; a++) {
            for (int b = 0; b < 3; b++) {
                Map *map = item->light_maps[a][b];
                if (map && map->size) {
                    has_light = 1;
                    break;
                }
            }
            if (has_light) break;
        }
    }

    // populate opaque array
    for (int a = 0; a < 3; a++) {
        for (int b = 0; b < 3; b++) {
            Map *map = item->block_maps[a][b];
            if (!map) {
                continue;
            }
            MAP_FOR_EACH(map, ex, ey, ez, ew) {
                    int x = ex - ox;
                    int y = ey - oy;
                    int z = ez - oz;
                    int w = ew;
                    // TODO: this should be unnecessary
                    if (x < 0 || y < 0 || z < 0) {
                        continue;
                    }
                    if (x >= XZ_SIZE || y >= Y_SIZE || z >= XZ_SIZE) {
                        continue;
                    }
                    // END TODO
                    opaque[XYZ(x, y, z)] = !is_transparent(w);
                    if (opaque[XYZ(x, y, z)]) {
                        highest[XZ(x, z)] = MAX(highest[XZ(x, z)], y);
                    }
                }
            END_MAP_FOR_EACH;
        }
    }

    // flood fill light intensities
    if (has_light) {
        for (int a = 0; a < 3; a++) {
            for (int b = 0; b < 3; b++) {
                Map *map = item->light_maps[a][b];
                if (!map) {
                    continue;
                }
                MAP_FOR_EACH(map, ex, ey, ez, ew) {
                        int x = ex - ox;
                        int y = ey - oy;
                        int z = ez - oz;
                        light_fill(opaque, light, x, y, z, ew, 1);
                    }
                END_MAP_FOR_EACH;
            }
        }
    }

    Map *map = item->block_maps[1][1];
    int guess_faces = CHUNK_SIZE * CHUNK_SIZE * 256 / 4;
    VertexData *data = (VertexData *) malloc_faces_new(sizeof(VertexData), guess_faces);
    int *indices_data = (int *) malloc(sizeof(int) * 6 * guess_faces);

    int miny = 256;
    int maxy = 0;
    int faces = 0;
    int offset = 0, offset_indices = 0;

    // int faces[6] = {left, right, top, bottom, front, back};

    // 0=slices, 1=width, 2= depth
    static const int size[6][3] = {
        {CHUNK_SIZE, CHUNK_SIZE, 256},
        {CHUNK_SIZE, CHUNK_SIZE, 256},
        {256, CHUNK_SIZE, CHUNK_SIZE},
        {256, CHUNK_SIZE, CHUNK_SIZE},
        {CHUNK_SIZE, CHUNK_SIZE, 256},
        {CHUNK_SIZE, CHUNK_SIZE, 256},
    };

    static const int directions[6][3] = {
        {-1, 0, 0},
        {1, 0, 0},
        {0, 1, 0},
        {0, -1, 0},
        {0, 0, -1},
        {0, 0, 1},
    };

    // 0=x, 1=y, 2= z
    static const int slice_map[6][3] = {
        {0, 2, 1},
        {0, 2, 1},
        {1, 0, 2},
        {1, 0, 2},
        {1, 1, 1},
        {1, 1, 1},
    };

    for (int y = 0; y < 256; y++) {
        char covered[CHUNK_SIZE * CHUNK_SIZE] = {0};
        for (int x = 0; x < CHUNK_SIZE; x++) {
            int ix = x * CHUNK_SIZE;
            for (int z = 0; z < CHUNK_SIZE; z++) {
                int i = z + ix;

                int xl = x + map->dx + 1;
                int yl = y + map->dy + 1;
                int zl = z + map->dz + 1;

                int xw = xl - ox;
                int yw = yl - oy;
                int zw = zl - oz;

                int w = map_get(map, xl, yl, zl);
                // If already covered by greedy mesher, continue
                if (covered[i] || w == 0 || is_plant(w) || opaque[XYZ(xw, yw + 1, zw)]) continue;

                covered[i] = 1;

                // AO and light
                char neighbors[9] = {0};
                char lights[9] = {0};
                float shades[9] = {0};
                int index = 0;
                for (int dx = -1; dx <= 1; dx++) {
                    for (int dz = -1; dz <= 1; dz++) {
                        neighbors[index] = opaque[XYZ(xw + dx, yw + 1, zw + dz)];
                        lights[index] = light[XYZ(xw + dx, yw + 1, zw + dz)];
                        if (yw + 1 <= highest[XZ(xw + dx, zw + dz)]) {
                            for (int oy = 0; oy < 8; oy++) {
                                if (opaque[XYZ(xw + dx, yw + 1 + oy, zw + dz)]) {
                                    shades[index] = 1.0 - oy * 0.125;
                                    break;
                                }
                            }
                        }
                        index++;
                    }
                }

                // z-axes
                unsigned int z_length = 1;
                for (int zd = 1; zd < CHUNK_SIZE - z; zd++) {
                    int wd = map_get(map, xl, yl, zl + zd);
                    if (covered[i + zd] || wd != w || is_plant(wd) || opaque[XYZ(xw, yw + 1, zw + zd)]) break;

                    // Check if AO and light is the same
                    int valid = 1;
                    int index_other = 0;
                    for (int dx = -1; dx <= 1; dx++) {
                        for (int dz = -1; dz <= 1; dz++) {
                            // Check light n AO
                            if (neighbors[index_other] != opaque[XYZ(xw + dx, yw + 1, zw + dz + zd)] ||
                                lights[index_other] != light[XYZ(xw + dx, yw + 1, zw + dz+ zd)]) {
                                valid = 0;
                                break;
                            }

                            index_other++;
                        }
                        if (!valid)
                            break;
                    }
                    if (!valid)
                        break;

                    z_length++;
                    covered[i + zd] = 1;
                }

                // x-axes
                unsigned int x_length = 1;
                for (int xd = 1; xd < CHUNK_SIZE - x; xd++) {
                    int valid = 1;

                    // Check if all in next row is valid
                    for (int z_row = 0; z_row < z_length; z_row++) {
                        int i_row = (x + xd) * CHUNK_SIZE + (z + z_row);
                        int wd = map_get(map, xl + xd, yl, zl + z_row);
                        if (covered[i_row] || wd != w || is_plant(wd) || opaque[XYZ(xw + xd, yw + 1, zw + z_row)]) {
                            valid = 0;
                            break;
                        }

                        // Check if AO and light is the same
                        int index_other = 0;
                        for (int dx = -1; dx <= 1; dx++) {
                            for (int dz = -1; dz <= 1; dz++) {
                                if (neighbors[index_other] != opaque[XYZ(xw + dx + xd, yw + 1, zw + dz + z_row)] ||
                                    lights[index_other] != light[XYZ(xw + dx + xd, yw + 1, zw + dz+ z_row)]) {
                                    valid = 0;
                                    break;
                                }
                                index_other++;
                            }
                            if (!valid)
                                break;
                        }

                        if (!valid)
                            break;
                    }
                    if (valid == 0) break;
                    x_length++;

                    // All were valid, set to covered
                    for (int z_row = 0; z_row < z_length; z_row++) {
                        int i_row = (x + xd) * CHUNK_SIZE + (z + z_row);
                        covered[i_row] = 1;
                    }
                }

                float ao[4];
                float r_light[4];
                occlusion_greedy(2, light[XYZ(xw, yw, zw)], neighbors, lights, shades, ao, r_light);
                //make_cube_faces_new(data + offset, ao1, light1, 0,0,1,0,0,0,1,1,1,1,1,1,xw,yw,zw, .5f);
                make_cube_face_greedy(data + offset, indices_data + offset_indices, offset, ao, r_light, 2, w, xw, yw,
                                      zw, .5f, x_length, 0, z_length);
                offset += 4;
                offset_indices += 6;
                faces++;
                //z += z_length;
            }
        }
    }

    MAP_FOR_EACH(map, ex, ey, ez, ew) {
            if (ew <= 0) {
                continue;
            }

            int x = ex - ox;
            int y = ey - oy;
            int z = ez - oz;
            int f1 = !opaque[XYZ(x - 1, y, z)];
            int f2 = !opaque[XYZ(x + 1, y, z)];
            int f3 = !opaque[XYZ(x, y + 1, z)];
            int f4 = !opaque[XYZ(x, y - 1, z)] && (ey > 0);
            int f5 = !opaque[XYZ(x, y, z - 1)];
            int f6 = !opaque[XYZ(x, y, z + 1)];
            int total = f1 + f2 + f4 + f5 + f6;
            if ((total + f3) == 0) {
                continue;
            }
            if (is_plant(ew)) {
                //continue; //TODO: REMOVE THIS
                total = 4;
            }
            miny = MIN(miny, ey);
            maxy = MAX(maxy, ey);
            faces += total;
        }
    END_MAP_FOR_EACH;

    // generate geometry
    // GLfloat *data = malloc_faces(10, faces);
    //Size of VertexData * 6 for each face, as each face produces 6 vertices.
    data = (VertexData *) realloc(data, sizeof(VertexData) * 4 * faces);
    indices_data = (int *) realloc(indices_data, sizeof(int) * 6 * faces);
    MAP_FOR_EACH(map, ex, ey, ez, ew) {
            if (ew <= 0) {
                continue;
            }

            int x = ex - ox;
            int y = ey - oy;
            int z = ez - oz;
            int f1 = !opaque[XYZ(x - 1, y, z)];
            int f2 = !opaque[XYZ(x + 1, y, z)];
            // f3 = !opaque[XYZ(x, y + 1, z)];
            int f4 = !opaque[XYZ(x, y - 1, z)] && (ey > 0);
            int f5 = !opaque[XYZ(x, y, z - 1)];
            int f6 = !opaque[XYZ(x, y, z + 1)];
            int total = f1 + f2 + f4 + f5 + f6;
            if (total == 0) {
                continue;
            }
            char neighbors[27] = {0};
            char lights[27] = {0};
            float shades[27] = {0};
            int index = 0;
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dz = -1; dz <= 1; dz++) {
                        neighbors[index] = opaque[XYZ(x + dx, y + dy, z + dz)];
                        lights[index] = light[XYZ(x + dx, y + dy, z + dz)];
                        shades[index] = 0;
                        if (y + dy <= highest[XZ(x + dx, z + dz)]) {
                            for (int oy = 0; oy < 8; oy++) {
                                if (opaque[XYZ(x + dx, y + dy + oy, z + dz)]) {
                                    shades[index] = 1.0 - oy * 0.125;
                                    break;
                                }
                            }
                        }
                        index++;
                    }
                }
            }
            float ao[6][4];
            float light[6][4];
            occlusion(neighbors, lights, shades, ao, light);
            if (is_plant(ew)) {
                //continue;
                total = 4;
                float min_ao = 1;
                float max_light = 0;
                for (int a = 0; a < 6; a++) {
                    for (int b = 0; b < 4; b++) {
                        min_ao = MIN(min_ao, ao[a][b]);
                        max_light = MAX(max_light, light[a][b]);
                    }
                }
                float rotation = simplex2(ex, ez, 4, 0.5, 2) * 360;
                make_plant(
                    data + offset, indices_data + offset_indices, offset, min_ao, max_light,
                    ex, ey, ez, 0.5, ew, rotation);
            } else {
                make_cube(
                    data + offset, indices_data + offset_indices, offset, ao, light,
                    f1, f2, 0, f4, f5, f6,
                    ex, ey, ez, 0.5, ew);
            }
            //Offset is Total faces * 6, as the total amount of vertexdata increases by 6 for each face.
            offset += total * 4;
            offset_indices += total * 6;
        }
    END_MAP_FOR_EACH;

    free(opaque);
    free(light);
    free(highest);

    item->miny = miny;
    item->maxy = maxy;
    item->faces = faces;
    item->data = data;
    item->indices_data = indices_data;
}

void compute_chunk(WorkerItem *item) {
    char *opaque = (char *) calloc(XZ_SIZE * XZ_SIZE * Y_SIZE, sizeof(char));
    char *light = (char *) calloc(XZ_SIZE * XZ_SIZE * Y_SIZE, sizeof(char));
    char *highest = (char *) calloc(XZ_SIZE * XZ_SIZE, sizeof(char));

    int ox = item->p * CHUNK_SIZE - CHUNK_SIZE - 1;
    int oy = -1;
    int oz = item->q * CHUNK_SIZE - CHUNK_SIZE - 1;

    // check for lights
    int has_light = 0;
    if (SHOW_LIGHTS) {
        for (int a = 0; a < 3; a++) {
            for (int b = 0; b < 3; b++) {
                Map *map = item->light_maps[a][b];
                if (map && map->size) {
                    has_light = 1;
                    break;
                }
            }
            if (has_light) break;
        }
    }

    // populate opaque array
    for (int a = 0; a < 3; a++) {
        for (int b = 0; b < 3; b++) {
            Map *map = item->block_maps[a][b];
            if (!map) {
                continue;
            }
            MAP_FOR_EACH(map, ex, ey, ez, ew) {
                    int x = ex - ox;
                    int y = ey - oy;
                    int z = ez - oz;
                    int w = ew;
                    // TODO: this should be unnecessary
                    if (x < 0 || y < 0 || z < 0) {
                        continue;
                    }
                    if (x >= XZ_SIZE || y >= Y_SIZE || z >= XZ_SIZE) {
                        continue;
                    }
                    // END TODO
                    opaque[XYZ(x, y, z)] = !is_transparent(w);
                    if (opaque[XYZ(x, y, z)]) {
                        highest[XZ(x, z)] = MAX(highest[XZ(x, z)], y);
                    }
                }
            END_MAP_FOR_EACH;
        }
    }

    // flood fill light intensities
    if (has_light) {
        for (int a = 0; a < 3; a++) {
            for (int b = 0; b < 3; b++) {
                Map *map = item->light_maps[a][b];
                if (!map) {
                    continue;
                }
                MAP_FOR_EACH(map, ex, ey, ez, ew) {
                        int x = ex - ox;
                        int y = ey - oy;
                        int z = ez - oz;
                        light_fill(opaque, light, x, y, z, ew, 1);
                    }
                END_MAP_FOR_EACH;
            }
        }
    }

    Map *map = item->block_maps[1][1];

    // count exposed faces
    int miny = 256;
    int maxy = 0;
    int faces = 0;

    MAP_FOR_EACH(map, ex, ey, ez, ew) {
            if (ew <= 0) {
                continue;
            }

            int x = ex - ox;
            int y = ey - oy;
            int z = ez - oz;
            int f1 = !opaque[XYZ(x - 1, y, z)];
            int f2 = !opaque[XYZ(x + 1, y, z)];
            int f3 = !opaque[XYZ(x, y + 1, z)];
            int f4 = !opaque[XYZ(x, y - 1, z)] && (ey > 0);
            int f5 = !opaque[XYZ(x, y, z - 1)];
            int f6 = !opaque[XYZ(x, y, z + 1)];
            int total = f1 + f2 + f3 + f4 + f5 + f6;
            if (total == 0) {
                continue;
            }
            if (is_plant(ew)) {
                //continue; //TODO: REMOVE THIS
                total = 4;
            }
            miny = MIN(miny, ey);
            maxy = MAX(maxy, ey);
            faces += total;
        }
    END_MAP_FOR_EACH;

    // generate geometry
    // GLfloat *data = malloc_faces(10, faces);
    //Size of VertexData * 6 for each face, as each face produces 6 vertices.
    VertexData *data = (VertexData *) malloc_faces_new(sizeof(VertexData), faces);
    int *indices_data = (int *) malloc(sizeof(int) * 6 * faces);
    int offset = 0, offset_indices = 0;

    MAP_FOR_EACH(map, ex, ey, ez, ew) {
            if (ew <= 0) {
                continue;
            }

            int x = ex - ox;
            int y = ey - oy;
            int z = ez - oz;
            int f1 = !opaque[XYZ(x - 1, y, z)];
            int f2 = !opaque[XYZ(x + 1, y, z)];
            int f3 = !opaque[XYZ(x, y + 1, z)];
            int f4 = !opaque[XYZ(x, y - 1, z)] && (ey > 0);
            int f5 = !opaque[XYZ(x, y, z - 1)];
            int f6 = !opaque[XYZ(x, y, z + 1)];
            int total = f1 + f2 + f3 + f4 + f5 + f6;
            if (total == 0) {
                continue;
            }
            char neighbors[27] = {0};
            char lights[27] = {0};
            float shades[27] = {0};
            int index = 0;
            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dz = -1; dz <= 1; dz++) {
                        neighbors[index] = opaque[XYZ(x + dx, y + dy, z + dz)];
                        lights[index] = light[XYZ(x + dx, y + dy, z + dz)];
                        shades[index] = 0;
                        if (y + dy <= highest[XZ(x + dx, z + dz)]) {
                            for (int oy = 0; oy < 8; oy++) {
                                if (opaque[XYZ(x + dx, y + dy + oy, z + dz)]) {
                                    shades[index] = 1.0 - oy * 0.125;
                                    break;
                                }
                            }
                        }
                        index++;
                    }
                }
            }
            float ao[6][4];
            float light[6][4];
            occlusion(neighbors, lights, shades, ao, light);
            if (is_plant(ew)) {
                //continue;
                total = 4;
                float min_ao = 1;
                float max_light = 0;
                for (int a = 0; a < 6; a++) {
                    for (int b = 0; b < 4; b++) {
                        min_ao = MIN(min_ao, ao[a][b]);
                        max_light = MAX(max_light, light[a][b]);
                    }
                }
                float rotation = simplex2(ex, ez, 4, 0.5, 2) * 360;
                make_plant(
                    data + offset, indices_data + offset_indices, offset, min_ao, max_light,
                    ex, ey, ez, 0.5, ew, rotation);
            } else {
                make_cube(
                    data + offset, indices_data + offset_indices, offset, ao, light,
                    f1, f2, f3, f4, f5, f6,
                    ex, ey, ez, 0.5, ew);
            }
            //Offset is Total faces * 6, as the total amount of vertexdata increases by 6 for each face.

            offset += total * 4;
            offset_indices += total * 6;
        }
    END_MAP_FOR_EACH;

    free(opaque);
    free(light);
    free(highest);

    item->miny = miny;
    item->maxy = maxy;
    item->faces = faces;
    item->data = data;
    item->indices_data = indices_data;
}

// TODODO
void generate_chunk(Chunk *chunk, WorkerItem *item) {
    chunk->miny = item->miny;
    chunk->maxy = item->maxy;
    chunk->faces = item->faces;
    del_buffer(chunk->buffer);
    del_buffer(chunk->indices_buffer);
    // chunk->buffer = gen_faces(8, item->faces, item->data);
    // chunk-> buffer = gen_faces(9, item->faces, item->data);

    chunk->buffer = gen_faces_chunk(sizeof(VertexData), item->faces, item->data);
    chunk->indices_buffer = gen_indices_chunk(item->faces, item->indices_data);

    //gen_sign_buffer(chunk);
}

void gen_chunk_buffer(Chunk *chunk) {
    WorkerItem _item;
    WorkerItem *item = &_item;
    item->p = chunk->key.p;
    item->q = chunk->key.q;
    for (int dp = -1; dp <= 1; dp++) {
        for (int dq = -1; dq <= 1; dq++) {
            Chunk *other = chunk;
            if (dp || dq) {
                other = find_chunk(chunk->key.p + dp, chunk->key.q + dq);
            }
            if (other) {
                item->block_maps[dp + 1][dq + 1] = &other->map;
                item->light_maps[dp + 1][dq + 1] = &other->lights;
            } else {
                item->block_maps[dp + 1][dq + 1] = 0;
                item->light_maps[dp + 1][dq + 1] = 0;
            }
        }
    }
#if USE_GREEDY_MESHING
    compute_chunk_greedy(item);
#else
        compute_chunk(item);
#endif
    generate_chunk(chunk, item);
    chunk->dirty = 0;
}

void map_set_func(int x, int y, int z, int w, void *arg) {
    Map *map = (Map *) arg;
    map_set(map, x, y, z, w);
}

void load_chunk(WorkerItem *item) {
    int p = item->p;
    int q = item->q;
    Map *block_map = item->block_maps[1][1];
    Map *light_map = item->light_maps[1][1];
    create_world(p, q, map_set_func, block_map);
    db_load_blocks(block_map, p, q);
    db_load_lights(light_map, p, q);
}

void request_chunk(int p, int q) {
    int key = db_get_key(p, q);
    client_chunk(p, q, key);
}

void init_chunk(Chunk *chunk, int p, int q) {
    chunk->key.p = p;
    chunk->key.q = q;
    chunk->faces = 0;
    chunk->sign_faces = 0;
    chunk->buffer = 0;
    chunk->sign_buffer = 0;
    dirty_chunk(chunk);
    SignList *signs = &chunk->signs;
    sign_list_alloc(signs, 16);
    db_load_signs(signs, p, q);
    Map *block_map = &chunk->map;
    Map *light_map = &chunk->lights;
    int dx = p * CHUNK_SIZE - 1;
    int dy = 0;
    int dz = q * CHUNK_SIZE - 1;
    map_alloc(block_map, dx, dy, dz, 0x7fff);
    map_alloc(light_map, dx, dy, dz, 0xf);

    HASH_ADD(hh, game->chunks_hash, key, sizeof(ChunkKey), chunk);
}

void create_chunk(Chunk *chunk, int p, int q) {
    init_chunk(chunk, p, q);

    WorkerItem _item;
    WorkerItem *item = &_item;
    item->p = chunk->key.p;
    item->q = chunk->key.q;
    item->block_maps[1][1] = &chunk->map;
    item->light_maps[1][1] = &chunk->lights;
    load_chunk(item);

    request_chunk(p, q);
}

void delete_chunks() {
    int count = game->chunk_count;
    State *s1 = &game->players->state;
    State *s2 = &(game->players + game->observe1)->state;
    State *s3 = &(game->players + game->observe2)->state;
    State *states[3] = {s1, s2, s3};
    for (int i = 0; i < count; i++) {
        Chunk *chunk = game->chunks + i;
        int delete = 1;
        for (int j = 0; j < 1; j++) {
            State *s = states[j];
            int p = chunked(s->x);
            int q = chunked(s->z);
            if (chunk_distance(chunk, p, q) < game->delete_radius) {
                delete = 0;
                break;
            }
        }
        if (delete) {
            HASH_DEL(game->chunks_hash, chunk);
            map_free(&chunk->map);
            map_free(&chunk->lights);
            sign_list_free(&chunk->signs);
            del_buffer(chunk->buffer);
            del_buffer(chunk->sign_buffer);
            Chunk *other = game->chunks + (--count);

            //Reset the hashmap for the chunk thats getting moved ot the chunk position.
            HASH_DEL(game->chunks_hash, other);
            memcpy(chunk, other, sizeof(Chunk));
            HASH_ADD(hh, game->chunks_hash, key, sizeof(ChunkKey), chunk);
        }
    }
    game->chunk_count = count;
}

void delete_all_chunks() {
    for (int i = 0; i < game->chunk_count; i++) {
        Chunk *chunk = game->chunks + i;
        HASH_DEL(game->chunks_hash, chunk);
        map_free(&chunk->map);
        map_free(&chunk->lights);
        sign_list_free(&chunk->signs);
        del_buffer(chunk->buffer);
        del_buffer(chunk->sign_buffer);
    }
    game->chunk_count = 0;
}

void check_workers() {
    for (int i = 0; i < WORKERS; i++) {
        Worker *worker = game->workers + i;
        mtx_lock(&worker->mtx);
        if (worker->state == WORKER_DONE) {
            WorkerItem *item = &worker->item;
            Chunk *chunk = find_chunk(item->p, item->q);
            if (chunk) {
                if (item->load) {
                    Map *block_map = item->block_maps[1][1];
                    Map *light_map = item->light_maps[1][1];
                    map_free(&chunk->map);
                    map_free(&chunk->lights);
                    map_copy(&chunk->map, block_map);
                    map_copy(&chunk->lights, light_map);
                    request_chunk(item->p, item->q);
                }
                generate_chunk(chunk, item);
            }
            for (int a = 0; a < 3; a++) {
                for (int b = 0; b < 3; b++) {
                    Map *block_map = item->block_maps[a][b];
                    Map *light_map = item->light_maps[a][b];
                    if (block_map) {
                        map_free(block_map);
                        free(block_map);
                    }
                    if (light_map) {
                        map_free(light_map);
                        free(light_map);
                    }
                }
            }
            worker->state = WORKER_IDLE;
        }
        mtx_unlock(&worker->mtx);
    }
}

void force_chunks(Player *player) {
    State *s = &player->state;
    int p = chunked(s->x);
    int q = chunked(s->z);
    int r = 1;
    for (int dp = -r; dp <= r; dp++) {
        for (int dq = -r; dq <= r; dq++) {
            int a = p + dp;
            int b = q + dq;
            Chunk *chunk = find_chunk(a, b);
            if (chunk) {
                if (chunk->dirty) {
                    gen_chunk_buffer(chunk);
                }
            } else if (game->chunk_count < MAX_CHUNKS) {
                chunk = game->chunks + game->chunk_count++;
                create_chunk(chunk, a, b);
                gen_chunk_buffer(chunk);
            }
        }
    }
}

void ensure_chunks_worker(Player *player, Worker *worker) {
    State *s = &player->state;
    float matrix[16];
    set_matrix_3d(
        matrix, game->width, game->height,
        s->x, s->y, s->z, s->rx, s->ry, game->fov, game->ortho, game->render_radius);
    __m256 planes[6][4];
    frustum_planes(planes, game->render_radius, matrix);
    int p = chunked(s->x);
    int q = chunked(s->z);
    int r = game->create_radius;
    int start = 0x0fffffff;
    int best_score = start;
    int best_a = 0;
    int best_b = 0;
    for (int dp = -r; dp <= r; dp++) {
        for (int dq = -r; dq <= r; dq++) {
            int a = p + dp;
            int b = q + dq;
            int index = (ABS(a) ^ ABS(b)) % WORKERS;
            if (index != worker->index) {
                continue;
            }
            Chunk *chunk = find_chunk(a, b);
            if (chunk && !chunk->dirty) {
                continue;
            }
            int distance = MAX(ABS(dp), ABS(dq));
            int invisible = !chunk_visible(planes, a, b, 0, 256);
            int priority = 0;
            if (chunk) {
                priority = chunk->buffer && chunk->dirty;
            }
            int score = (invisible << 24) | (priority << 16) | distance;
            if (score < best_score) {
                best_score = score;
                best_a = a;
                best_b = b;
            }
        }
    }
    if (best_score == start) {
        return;
    }
    int a = best_a;
    int b = best_b;
    int load = 0;
    Chunk *chunk = find_chunk(a, b);
    if (!chunk) {
        load = 1;
        if (game->chunk_count < MAX_CHUNKS) {
            chunk = game->chunks + game->chunk_count++;
            init_chunk(chunk, a, b);
        } else {
            return;
        }
    }
    WorkerItem *item = &worker->item;
    item->p = chunk->key.p;
    item->q = chunk->key.q;
    item->load = load;
    for (int dp = -1; dp <= 1; dp++) {
        for (int dq = -1; dq <= 1; dq++) {
            Chunk *other = chunk;
            if (dp || dq) {
                other = find_chunk(chunk->key.p + dp, chunk->key.q + dq);
            }
            if (other) {
                Map *block_map = malloc(sizeof(Map));
                map_copy(block_map, &other->map);
                Map *light_map = malloc(sizeof(Map));
                map_copy(light_map, &other->lights);
                item->block_maps[dp + 1][dq + 1] = block_map;
                item->light_maps[dp + 1][dq + 1] = light_map;
            } else {
                item->block_maps[dp + 1][dq + 1] = 0;
                item->light_maps[dp + 1][dq + 1] = 0;
            }
        }
    }
    chunk->dirty = 0;
    worker->state = WORKER_BUSY;
    cnd_signal(&worker->cnd);
}

void ensure_chunks(Player *player) {
    check_workers();
    force_chunks(player);

    for (int i = 0; i < WORKERS; i++) {
        Worker *worker = game->workers + i;
        mtx_lock(&worker->mtx);
        if (worker->state == WORKER_IDLE) {
            ensure_chunks_worker(player, worker);
        }
        mtx_unlock(&worker->mtx);
    }
}

int worker_run(void *arg) {
    Worker *worker = (Worker *) arg;
    int running = 1;
    while (running) {
        mtx_lock(&worker->mtx);
        while (worker->state != WORKER_BUSY) {
            cnd_wait(&worker->cnd, &worker->mtx);
        }
        mtx_unlock(&worker->mtx);
        WorkerItem *item = &worker->item;
        if (item->load) {
            load_chunk(item);
        }
#if USE_GREEDY_MESHING
        compute_chunk_greedy(item);
#else
            compute_chunk(item);
#endif
        mtx_lock(&worker->mtx);
        worker->state = WORKER_DONE;
        mtx_unlock(&worker->mtx);
    }
    return 0;
}

void unset_sign(int x, int y, int z) {
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        SignList *signs = &chunk->signs;
        if (sign_list_remove_all(signs, x, y, z)) {
            chunk->dirty = 1;
            db_delete_signs(x, y, z);
        }
    } else {
        db_delete_signs(x, y, z);
    }
}

void unset_sign_face(int x, int y, int z, int face) {
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        SignList *signs = &chunk->signs;
        if (sign_list_remove(signs, x, y, z, face)) {
            chunk->dirty = 1;
            db_delete_sign(x, y, z, face);
        }
    } else {
        db_delete_sign(x, y, z, face);
    }
}

void _set_sign(
    int p, int q, int x, int y, int z, int face, const char *text, int dirty) {
    if (strlen(text) == 0) {
        unset_sign_face(x, y, z, face);
        return;
    }
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        SignList *signs = &chunk->signs;
        sign_list_add(signs, x, y, z, face, text);
        if (dirty) {
            chunk->dirty = 1;
        }
    }
    db_insert_sign(p, q, x, y, z, face, text);
}

void set_sign(int x, int y, int z, int face, const char *text) {
    int p = chunked(x);
    int q = chunked(z);
    _set_sign(p, q, x, y, z, face, text, 1);
    client_sign(x, y, z, face, text);
}

void toggle_light(int x, int y, int z) {
    int p = chunked(x);
    int q = chunked(z);
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->lights;
        int w = map_get(map, x, y, z) ? 0 : 15;
        map_set(map, x, y, z, w);
        db_insert_light(p, q, x, y, z, w);
        client_light(x, y, z, w);
        dirty_chunk(chunk);
    }
}

void set_light(int p, int q, int x, int y, int z, int w) {
    Chunk *chunk = find_chunk(p, q);
    if (chunk) {
        Map *map = &chunk->lights;
        if (map_set(map, x, y, z, w)) {
            dirty_chunk(chunk);
            db_insert_light(p, q, x, y, z, w);
        }
    } else {
        db_insert_light(p, q, x, y, z, w);
    }
}


void record_block(int x, int y, int z, int w) {
    memcpy(&game->block1, &game->block0, sizeof(Block));
    game->block0.x = x;
    game->block0.y = y;
    game->block0.z = z;
    game->block0.w = w;
}


int render_chunks(Attrib *attrib, Player *player) {
    int result = 0;
    State *s = &player->state;
    ensure_chunks(player);

    int p = chunked(s->x);
    int q = chunked(s->z);
    float light = get_daylight();
    float matrix[16];
    set_matrix_3d(
        matrix, game->width, game->height,
        s->x, s->y, s->z, s->rx, s->ry, game->fov, game->ortho, game->render_radius);

#if USE_GPU_FRUSTUM_CULLING
    float matrix_gpu[16];
    set_matrix_3d(
        matrix_gpu, g->width, g->height,
        s->x - (p * CHUNK_SIZE), s->y, s->z - (q * CHUNK_SIZE), s->rx, s->ry, g->fov, g->ortho, g->render_radius);
    // __m256 planes[6][4];
    // frustum_planes(planes, g->render_radius, matrix);

    float gpu_planes[6][4];
    frustum_planes_n(gpu_planes, g->render_radius, matrix_gpu);

    if (attrib->compute_sync == NULL) {
        printf("we startin");
        updateComputeBuffer(attrib->compute_planes_input, sizeof(gpu_planes), gpu_planes);
        dispatchComputeShader(attrib->compute_program, attrib->compute_output, attrib->compute_planes_input);
        glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT);
        attrib->compute_sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    }

    GLenum waitReturn = glClientWaitSync( attrib->compute_sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
    if (waitReturn == GL_ALREADY_SIGNALED || waitReturn == GL_CONDITION_SATISFIED) {
        char visibililitylookup[RENDER_CHUNK_RADIUS * RENDER_CHUNK_RADIUS * 4]; //4096 ints
        readBuffer(attrib->compute_output, sizeof(visibililitylookup), visibililitylookup);
        attrib->lookup_table = visibililitylookup;

        updateComputeBuffer(attrib->compute_planes_input, sizeof(gpu_planes), gpu_planes);
        dispatchComputeShader(attrib->compute_program, attrib->compute_output, attrib->compute_planes_input);
        glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT);
        attrib->compute_sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    } else {
        printf("compute not done yet\n");
    }

    if (attrib->lookup_table == NULL) {
        return result;
    }
#else
    __m256 planes[6][4];
    frustum_planes(planes, game->render_radius, matrix);
#endif

    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform3f(attrib->camera, s->x, s->y, s->z);
    glUniform1i(attrib->sampler, 0);
    glUniform1i(attrib->extra1, 2);
    glUniform1i(attrib->chunk_size, CHUNK_SIZE);
    glUniform1f(attrib->extra2, light);
    glUniform1f(attrib->extra3, game->render_radius * CHUNK_SIZE);
    glUniform1i(attrib->extra4, game->ortho);
    glUniform1f(attrib->timer, time_of_day());

    for (int i = 0; i < game->chunk_count; i++) {
        Chunk *chunk = game->chunks + i;
        if (chunk_distance(chunk, p, q) > game->render_radius) {
            continue;
        }
#if USE_GPU_FRUSTUM_CULLING
        if (attrib->lookup_table[chunk->key.p - p + RENDER_CHUNK_RADIUS + (chunk->key.q - q + RENDER_CHUNK_RADIUS) * RENDER_CHUNK_RADIUS * 2] == 0 ){
            continue;
        }
#else
        if (!chunk_visible(
            planes, chunk->key.p, chunk->key.q, chunk->miny, chunk->maxy)) {
            continue;
        }
#endif


        draw_chunk(attrib, chunk);
        result += chunk->faces;
    }
    return result;
}

void render_signs(Attrib *attrib, Player *player) {
    State *s = &player->state;
    int p = chunked(s->x);
    int q = chunked(s->z);
    float matrix[16];
    set_matrix_3d(
        matrix, game->width, game->height,
        s->x, s->y, s->z, s->rx, s->ry, game->fov, game->ortho, game->render_radius);
    __m256 planes[6][4];
    frustum_planes(planes, game->render_radius, matrix);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform1i(attrib->sampler, 3);
    glUniform1i(attrib->extra1, 1);
    for (int i = 0; i < game->chunk_count; i++) {
        Chunk *chunk = game->chunks + i;
        if (chunk_distance(chunk, p, q) > game->sign_radius) {
            continue;
        }
        if (!chunk_visible(
            planes, chunk->key.p, chunk->key.q, chunk->miny, chunk->maxy)) {
            continue;
        }
        draw_signs(attrib, chunk);
    }
}

void render_sign(Attrib *attrib, Player *player) {
    if (!game->typing || game->typing_buffer[0] != CRAFT_KEY_SIGN) {
        return;
    }
    int x, y, z, face;
    if (!hit_test_face(player, &x, &y, &z, &face)) {
        return;
    }
    State *s = &player->state;
    float matrix[16];
    set_matrix_3d(
        matrix, game->width, game->height,
        s->x, s->y, s->z, s->rx, s->ry, game->fov, game->ortho, game->render_radius);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform1i(attrib->sampler, 3);
    glUniform1i(attrib->extra1, 1);
    char text[MAX_SIGN_LENGTH];
    strncpy(text, game->typing_buffer + 1, MAX_SIGN_LENGTH);
    text[MAX_SIGN_LENGTH - 1] = '\0';
    GLfloat *data = malloc_faces(5, strlen(text));
    int length = _gen_sign_buffer(data, x, y, z, face, text);
    GLuint buffer = gen_faces(5, length, data);
    draw_sign(attrib, buffer, length);
    del_buffer(buffer);
}

void render_players(Attrib *attrib, Player *player) {
    State *s = &player->state;
    float matrix[16];
    set_matrix_3d(
        matrix, game->width, game->height,
        s->x, s->y, s->z, s->rx, s->ry, game->fov, game->ortho, game->render_radius);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform3f(attrib->camera, s->x, s->y, s->z);
    glUniform1i(attrib->sampler, 0);
    glUniform1f(attrib->timer, time_of_day());
    for (int i = 0; i < game->player_count; i++) {
        Player *other = game->players + i;
        //if (other != player) {
        draw_player(attrib, other);
        //}
    }
}

void render_sky(Attrib *attrib, Player *player, GLuint buffer) {
    State *s = &player->state;
    float matrix[16];
    set_matrix_3d(
        matrix, game->width, game->height,
        0, 0, 0, s->rx, s->ry, game->fov, 0, game->render_radius);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform1i(attrib->sampler, 2);
    glUniform1f(attrib->timer, time_of_day());
    draw_triangles_3d(attrib, buffer, 512 * 3);
}

void render_wireframe(Attrib *attrib, Player *player) {
    State *s = &player->state;
    float matrix[16];
    set_matrix_3d(
        matrix, game->width, game->height,
        s->x, s->y, s->z, s->rx, s->ry, game->fov, game->ortho, game->render_radius);
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (is_obstacle(hw)) {
        glUseProgram(attrib->program);
        glLineWidth(1);
        glEnable(GL_COLOR_LOGIC_OP);
        glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
        GLuint wireframe_buffer = gen_wireframe_buffer(hx, hy, hz, 0.53);
        draw_lines(attrib, wireframe_buffer, 3, 24);
        del_buffer(wireframe_buffer);
        glDisable(GL_COLOR_LOGIC_OP);
    }
}

void render_crosshairs(Attrib *attrib) {
    float matrix[16];
    set_matrix_2d(matrix, game->width, game->height);
    glUseProgram(attrib->program);
    glLineWidth(4 * game->scale);
    glEnable(GL_COLOR_LOGIC_OP);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    GLuint crosshair_buffer = gen_crosshair_buffer();
    draw_lines(attrib, crosshair_buffer, 2, 4);
    del_buffer(crosshair_buffer);
    glDisable(GL_COLOR_LOGIC_OP);
}

void render_item(Attrib *attrib) {
    float matrix[16];
    set_matrix_item(matrix, game->width, game->height, game->scale);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform3f(attrib->camera, 0, 0, 5);
    glUniform1i(attrib->sampler, 0);
    glUniform1f(attrib->timer, time_of_day());
    int w = items[game->item_index];
    if (is_plant(w)) {
        //IGNORE PLANTS FOR NOW
        GLuint buffer = gen_plant_buffer(0, 0, 0, 0.5, w);
        draw_plant(attrib, buffer);
        del_buffer(buffer);
    } else {
        GLuint buffer = gen_cube_buffer(0, 0, 0, 0.5, w);
        draw_cube(attrib, buffer);
        del_buffer(buffer);
    }
}

void render_text(
    Attrib *attrib, int justify, float x, float y, float n, char *text) {
    float matrix[16];
    set_matrix_2d(matrix, game->width, game->height);
    glUseProgram(attrib->program);
    glUniformMatrix4fv(attrib->matrix, 1, GL_FALSE, matrix);
    glUniform1i(attrib->sampler, 1);
    glUniform1i(attrib->extra1, 0);
    int length = strlen(text);
    x -= n * justify * (length - 1) / 2;
    GLuint buffer = gen_text_buffer(x, y, n, text);
    draw_text(attrib, buffer, length);
    del_buffer(buffer);
}

void add_message(const char *text) {
    printf("%s\n", text);
    snprintf(
        game->messages[game->message_index], MAX_TEXT_LENGTH, "%s", text);
    game->message_index = (game->message_index + 1) % MAX_MESSAGES;
}

void login() {
    char username[128] = {0};
    char identity_token[128] = {0};
    char access_token[128] = {0};
    if (db_auth_get_selected(username, 128, identity_token, 128)) {
        printf("Contacting login server for username: %s\n", username);
        if (get_access_token(
            access_token, 128, username, identity_token)) {
            printf("Successfully authenticated with the login server\n");
            client_login(username, access_token);
        } else {
            printf("Failed to authenticate with the login server\n");
            client_login("", "");
        }
    } else {
        printf("Logging in anonymously\n");
        client_login("", "");
    }
}

void copy() {
    memcpy(&game->copy0, &game->block0, sizeof(Block));
    memcpy(&game->copy1, &game->block1, sizeof(Block));
}

void paste() {
    Block *c1 = &game->copy1;
    Block *c2 = &game->copy0;
    Block *p1 = &game->block1;
    Block *p2 = &game->block0;
    int scx = SIGN(c2->x - c1->x);
    int scz = SIGN(c2->z - c1->z);
    int spx = SIGN(p2->x - p1->x);
    int spz = SIGN(p2->z - p1->z);
    int oy = p1->y - c1->y;
    int dx = ABS(c2->x - c1->x);
    int dz = ABS(c2->z - c1->z);
    for (int y = 0; y < 256; y++) {
        for (int x = 0; x <= dx; x++) {
            for (int z = 0; z <= dz; z++) {
                int w = get_block(c1->x + x * scx, y, c1->z + z * scz);
                builder_block(p1->x + x * spx, y + oy, p1->z + z * spz, w);
            }
        }
    }
}

void on_light() {
    State *s = &game->players->state;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (hy > 0 && hy < 256 && is_destructable(hw)) {
        toggle_light(hx, hy, hz);
    }
}


void on_char(GLFWwindow *window, unsigned int u) {
    if (game->suppress_char) {
        game->suppress_char = 0;
        return;
    }
    if (game->typing) {
        if (u >= 32 && u < 128) {
            char c = (char) u;
            int n = strlen(game->typing_buffer);
            if (n < MAX_TEXT_LENGTH - 1) {
                game->typing_buffer[n] = c;
                game->typing_buffer[n + 1] = '\0';
            }
        }
    } else {
        if (u == CRAFT_KEY_CHAT) {
            game->typing = 1;
            game->typing_buffer[0] = '\0';
        }
        if (u == CRAFT_KEY_COMMAND) {
            game->typing = 1;
            game->typing_buffer[0] = '/';
            game->typing_buffer[1] = '\0';
        }
        if (u == CRAFT_KEY_SIGN) {
            game->typing = 1;
            game->typing_buffer[0] = CRAFT_KEY_SIGN;
            game->typing_buffer[1] = '\0';
        }
    }
}

void on_scroll(GLFWwindow *window, double xdelta, double ydelta) {
    static double ypos = 0;
    ypos += ydelta;
    if (ypos < -SCROLL_THRESHOLD) {
        game->item_index = (game->item_index + 1) % item_count;
        ypos = 0;
    }
    if (ypos > SCROLL_THRESHOLD) {
        game->item_index--;
        if (game->item_index < 0) {
            game->item_index = item_count - 1;
        }
        ypos = 0;
    }
}

void on_mouse_button(GLFWwindow *window, int button, int action, int mods) {
    int control = mods & (GLFW_MOD_CONTROL | GLFW_MOD_SUPER);
    int exclusive =
            glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
    if (action != GLFW_PRESS) {
        return;
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (exclusive) {
            if (control) {
                on_right_click();
            } else {
                on_left_click();
            }
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (exclusive) {
            if (control) {
                on_light();
            } else {
                on_right_click();
            }
        }
    }
    if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        if (exclusive) {
            on_middle_click();
        }
    }
}

void create_window() {
    int window_width = WINDOW_WIDTH;
    int window_height = WINDOW_HEIGHT;
    GLFWmonitor *monitor = NULL;
    if (FULLSCREEN) {
        int mode_count;
        monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *modes = glfwGetVideoModes(monitor, &mode_count);
        window_width = modes[mode_count - 1].width;
        window_height = modes[mode_count - 1].height;
    }
    game->window = glfwCreateWindow(
        window_width, window_height, "Craft", monitor, NULL);
}

void handle_mouse_input() {
    int exclusive =
            glfwGetInputMode(game->window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
    static double px = 0;
    static double py = 0;
    State *s = &game->players->state;
    if (exclusive && (px || py)) {
        double mx, my;
        glfwGetCursorPos(game->window, &mx, &my);
        float m = 0.0025;
        s->rx += (mx - px) * m;
        if (INVERT_MOUSE) {
            s->ry += (my - py) * m;
        } else {
            s->ry -= (my - py) * m;
        }
        if (s->rx < 0) {
            s->rx += RADIANS(360);
        }
        if (s->rx >= RADIANS(360)) {
            s->rx -= RADIANS(360);
        }
        s->ry = MAX(s->ry, -RADIANS(90));
        s->ry = MIN(s->ry, RADIANS(90));
        px = mx;
        py = my;
    } else {
        glfwGetCursorPos(game->window, &px, &py);
    }
}


void parse_buffer(char *buffer) {
    Player *me = game->players;
    State *s = &game->players->state;
    char *key;
    char *line = tokenize(buffer, "\n", &key);
    while (line) {
        int pid;
        float ux, uy, uz, urx, ury;
        if (sscanf(line, "U,%d,%f,%f,%f,%f,%f",
                   &pid, &ux, &uy, &uz, &urx, &ury) == 6) {
            me->id = pid;
            s->x = ux;
            s->y = uy;
            s->z = uz;
            s->rx = urx;
            s->ry = ury;
            force_chunks(me);
            if (uy == 0) {
                s->y = highest_block(s->x, s->z) + 2;
            }
        }
        int bp, bq, bx, by, bz, bw;
        if (sscanf(line, "B,%d,%d,%d,%d,%d,%d",
                   &bp, &bq, &bx, &by, &bz, &bw) == 6) {
            _set_block(bp, bq, bx, by, bz, bw, 0);
            if (player_intersects_block(2, s->x, s->y, s->z, bx, by, bz)) {
                s->y = highest_block(s->x, s->z) + 2;
            }
        }
        if (sscanf(line, "L,%d,%d,%d,%d,%d,%d",
                   &bp, &bq, &bx, &by, &bz, &bw) == 6) {
            set_light(bp, bq, bx, by, bz, bw);
        }
        float px, py, pz, prx, pry;
        if (sscanf(line, "P,%d,%f,%f,%f,%f,%f",
                   &pid, &px, &py, &pz, &prx, &pry) == 6) {
            Player *player = find_player(pid);
            if (!player && game->player_count < MAX_PLAYERS) {
                player = game->players + game->player_count;
                game->player_count++;
                player->id = pid;
                player->buffer = 0;
                snprintf(player->name, MAX_NAME_LENGTH, "player%d", pid);
                update_player(player, px, py, pz, prx, pry, 1); // twice
            }
            if (player) {
                update_player(player, px, py, pz, prx, pry, 1);
            }
        }
        if (sscanf(line, "D,%d", &pid) == 1) {
            delete_player(pid);
        }
        int kp, kq, kk;
        if (sscanf(line, "K,%d,%d,%d", &kp, &kq, &kk) == 3) {
            db_set_key(kp, kq, kk);
        }
        if (sscanf(line, "R,%d,%d", &kp, &kq) == 2) {
            Chunk *chunk = find_chunk(kp, kq);
            if (chunk) {
                dirty_chunk(chunk);
            }
        }
        double elapsed;
        int day_length;
        if (sscanf(line, "E,%lf,%d", &elapsed, &day_length) == 2) {
            glfwSetTime(fmod(elapsed, day_length));
            game->day_length = day_length;
            game->time_changed = 1;
        }
        if (line[0] == 'T' && line[1] == ',') {
            char *text = line + 2;
            add_message(text);
        }
        char format[64];
        snprintf(
            format, sizeof(format), "N,%%d,%%%ds", MAX_NAME_LENGTH - 1);
        char name[MAX_NAME_LENGTH];
        if (sscanf(line, format, &pid, name) == 2) {
            Player *player = find_player(pid);
            if (player) {
                strncpy(player->name, name, MAX_NAME_LENGTH);
            }
        }
        snprintf(
            format, sizeof(format),
            "S,%%d,%%d,%%d,%%d,%%d,%%d,%%%d[^\n]", MAX_SIGN_LENGTH - 1);
        int face;
        char text[MAX_SIGN_LENGTH] = {0};
        if (sscanf(line, format,
                   &bp, &bq, &bx, &by, &bz, &face, text) >= 6) {
            _set_sign(bp, bq, bx, by, bz, face, text, 0);
        }
        line = tokenize(NULL, "\n", &key);
    }
}

void reset_model() {
    memset(game->chunks, 0, sizeof(Chunk) * MAX_CHUNKS);
    game->chunks_hash = NULL;
    game->chunk_count = 0;
    memset(game->players, 0, sizeof(Player) * MAX_PLAYERS);
    game->player_count = 0;
    game->observe1 = 0;
    game->observe2 = 0;
    game->flying = 0;
    game->item_index = 0;
    memset(game->typing_buffer, 0, sizeof(char) * MAX_TEXT_LENGTH);
    game->typing = 0;
    memset(game->messages, 0, sizeof(char) * MAX_MESSAGES * MAX_TEXT_LENGTH);
    game->message_index = 0;
    game->day_length = DAY_LENGTH;
    glfwSetTime(game->day_length / 3.0);
    game->time_changed = 1;
}

int main(int argc, char **argv) {
    // INITIALIZATION //
    curl_global_init(CURL_GLOBAL_DEFAULT);
    srand(time(NULL));
    rand();

    // WINDOW INITIALIZATION //
    if (!glfwInit()) {
        return -1;
    }
    create_window();
    if (!game->window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(game->window);
    // glfwSwapInterval(VSYNC);
    glfwSwapInterval(0);
    glfwSetInputMode(game->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetKeyCallback(game->window, on_key);
    glfwSetCharCallback(game->window, on_char);
    glfwSetMouseButtonCallback(game->window, on_mouse_button);
    glfwSetScrollCallback(game->window, on_scroll);

    if (glewInit() != GLEW_OK) {
        return -1;
    }

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glLogicOp(GL_INVERT);
    glClearColor(0, 0, 0, 1);

    // LOAD TEXTURES //
    GLuint texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    load_png_texture("textures/texture.png");

    GLuint font;
    glGenTextures(1, &font);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, font);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    load_png_texture("textures/font.png");

    GLuint sky;
    glGenTextures(1, &sky);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, sky);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    load_png_texture("textures/sky.png");

    GLuint sign;
    glGenTextures(1, &sign);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, sign);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    load_png_texture("textures/sign.png");

    // LOAD SHADERS //
    Attrib block_attrib = {0};
    Attrib line_attrib = {0};
    Attrib text_attrib = {0};
    Attrib sky_attrib = {0};
    GLuint program;

    program = load_program(
        "shaders/block_vertex.glsl", "shaders/block_fragment.glsl");
    block_attrib.program = program;
    block_attrib.position = glGetAttribLocation(program, "position");
    block_attrib.uvts = glGetAttribLocation(program, "uvts");
    block_attrib.uvScales = glGetAttribLocation(program, "uvScales");
    block_attrib.normal = glGetAttribLocation(program, "diffuse_bake");
    block_attrib.uv = glGetAttribLocation(program, "uv");
    block_attrib.position_uint = glGetAttribLocation(program, "position_uint");
    printf("block_attrib.position = %d\n", block_attrib.position);
    printf("block_attrib.uvts = %d\n", block_attrib.uvts);
    printf("block_attrib.uv = %d\n", block_attrib.uv);
    printf("block_attrib.position_uint = %d\n", block_attrib.position_uint);
    printf("HELLOOOOO \n");

    block_attrib.matrix = glGetUniformLocation(program, "matrix");
    block_attrib.sampler = glGetUniformLocation(program, "sampler");
    block_attrib.extra1 = glGetUniformLocation(program, "sky_sampler");
    block_attrib.extra2 = glGetUniformLocation(program, "daylight");
    block_attrib.extra3 = glGetUniformLocation(program, "fog_distance");
    block_attrib.extra4 = glGetUniformLocation(program, "ortho");
    block_attrib.camera = glGetUniformLocation(program, "camera");
    block_attrib.chunk_size = glGetUniformLocation(program, "chunk_size");
    block_attrib.timer = glGetUniformLocation(program, "timer");
    block_attrib.chunk_pos = glGetUniformLocation(program, "chunk_pos");

    //BIND COMPUTE SHADER THINGS
    program = load_compute_program("shaders/frustum_compute.glsl");
    block_attrib.compute_program = program;
    block_attrib.compute_output = createComputeBuffer(
        GL_SHADER_STORAGE_BUFFER, sizeof(char) * RENDER_CHUNK_RADIUS * RENDER_CHUNK_RADIUS * 4, NULL, GL_DYNAMIC_READ);
    block_attrib.compute_planes_input = createComputeBuffer(
        GL_SHADER_STORAGE_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);


    program = load_program(
        "shaders/line_vertex.glsl", "shaders/line_fragment.glsl");
    line_attrib.program = program;
    line_attrib.position = glGetAttribLocation(program, "position");
    line_attrib.matrix = glGetUniformLocation(program, "matrix");

    program = load_program(
        "shaders/text_vertex.glsl", "shaders/text_fragment.glsl");
    text_attrib.program = program;
    text_attrib.position = glGetAttribLocation(program, "position");
    text_attrib.uv = glGetAttribLocation(program, "uv");
    text_attrib.matrix = glGetUniformLocation(program, "matrix");
    text_attrib.sampler = glGetUniformLocation(program, "sampler");
    text_attrib.extra1 = glGetUniformLocation(program, "is_sign");

    program = load_program(
        "shaders/sky_vertex.glsl", "shaders/sky_fragment.glsl");
    sky_attrib.program = program;
    sky_attrib.position = glGetAttribLocation(program, "position");
    sky_attrib.normal = glGetAttribLocation(program, "normal");
    sky_attrib.uv = glGetAttribLocation(program, "uv");
    sky_attrib.matrix = glGetUniformLocation(program, "matrix");
    sky_attrib.sampler = glGetUniformLocation(program, "sampler");
    sky_attrib.timer = glGetUniformLocation(program, "timer");


    // CHECK COMMAND LINE ARGUMENTS //
    if (argc == 2 || argc == 3) {
        game->mode = MODE_ONLINE;
        strncpy(game->server_addr, argv[1], MAX_ADDR_LENGTH);
        game->server_port = argc == 3 ? atoi(argv[2]) : DEFAULT_PORT;
        snprintf(game->db_path, MAX_PATH_LENGTH,
                 "cache.%s.%d.db", game->server_addr, game->server_port);
    } else {
        game->mode = MODE_OFFLINE;
        snprintf(game->db_path, MAX_PATH_LENGTH, "%s", DB_PATH);
    }

    game->create_radius = CREATE_CHUNK_RADIUS;
    game->render_radius = RENDER_CHUNK_RADIUS;
    game->delete_radius = DELETE_CHUNK_RADIUS;
    game->sign_radius = RENDER_SIGN_RADIUS;

    // INITIALIZE WORKER THREADS
    for (int i = 0; i < WORKERS; i++) {
        Worker *worker = game->workers + i;
        worker->index = i;
        worker->state = WORKER_IDLE;
        mtx_init(&worker->mtx, mtx_plain);
        cnd_init(&worker->cnd);
        thrd_create(&worker->thrd, worker_run, worker);
    }

    //WIREFRAMEMODE
    int wire = 0;

    // OUTER LOOP //
    int running = 1;
    while (running) {
        // DATABASE INITIALIZATION //
        if (game->mode == MODE_OFFLINE || USE_CACHE) {
            db_enable();
            if (db_init(game->db_path)) {
                return -1;
            }
            if (game->mode == MODE_ONLINE) {
                // TODO: support proper caching of signs (handle deletions)
                db_delete_all_signs();
            }
        }

        // CLIENT INITIALIZATION //
        if (game->mode == MODE_ONLINE) {
            client_enable();
            client_connect(game->server_addr, game->server_port);
            client_start();
            client_version(1);
            login();
        }

        // LOCAL VARIABLES //
        reset_model();
        FPS fps = {0, 0, 0};
        double last_commit = glfwGetTime();
        double last_update = glfwGetTime();
        GLuint sky_buffer = gen_sky_buffer();

        Player *me = game->players;
        State *s = &game->players->state;
        me->id = 0;
        me->name[0] = '\0';
        me->buffer = 0;
        game->player_count = 1;

        // LOAD STATE FROM DATABASE //
        int loaded = db_load_state(&s->x, &s->y, &s->z, &s->rx, &s->ry);
        force_chunks(me);
        if (!loaded) {
            s->y = highest_block(s->x, s->z) + 2;
        }


        int chunked_p = 0;
        int chunked_q = 0;

        // BEGIN MAIN LOOP //
        double previous = glfwGetTime();
        while (1) {
            // HANDLE WIREFRAME TOGGLE
            glfwGetKey(game->window, 'U')
                ? glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)
                : glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

            // WINDOW SIZE AND SCALE //
            game->scale = get_scale_factor();
            glfwGetFramebufferSize(game->window, &game->width, &game->height);
            glViewport(0, 0, game->width, game->height);

            // FRAME RATE //
            if (game->time_changed) {
                game->time_changed = 0;
                last_commit = glfwGetTime();
                last_update = glfwGetTime();
                memset(&fps, 0, sizeof(fps));
            }
            update_fps(&fps);
            double now = glfwGetTime();
            double dt = now - previous;
            dt = MIN(dt, 0.2);
            dt = MAX(dt, 0.0);
            previous = now;

            // HANDLE MOUSE INPUT //
            handle_mouse_input();

            // HANDLE MOVEMENT //
            handle_movement(dt);

            // HANDLE DATA FROM SERVER //
            char *buffer = client_recv();
            if (buffer) {
                parse_buffer(buffer);
                free(buffer);
            }

            // FLUSH DATABASE //
            if (now - last_commit > COMMIT_INTERVAL) {
                last_commit = now;
                db_commit();
            }

            // SEND POSITION TO SERVER //
            if (now - last_update > 0.1) {
                last_update = now;
                client_position(s->x, s->y, s->z, s->rx, s->ry);
            }
            //FREEZE SOMEWHERE HERE
            // PREPARE TO RENDER //
            game->observe1 = game->observe1 % game->player_count;
            game->observe2 = game->observe2 % game->player_count;

            del_buffer(me->buffer);
            me->buffer = gen_player_buffer(s->x, s->y, s->z, s->rx, s->ry);
            for (int i = 1; i < game->player_count; i++) {
                interpolate_player(game->players + i);
            }
            Player *player = game->players + game->observe1;

            // UPDATE CHUNKED POS //
            int n_chunked_p = chunked(player->state.x);
            int n_chunked_q = chunked(player->state.z);
            if (n_chunked_p != chunked_p || n_chunked_q != chunked_q) {
                chunked_p = n_chunked_p;
                chunked_q = n_chunked_q;
            }
            delete_chunks();

            // printf("We get here");
            // RENDER 3-D SCENE //
            glClear(GL_COLOR_BUFFER_BIT);
            glClear(GL_DEPTH_BUFFER_BIT);
            render_sky(&sky_attrib, player, sky_buffer);
            glClear(GL_DEPTH_BUFFER_BIT);
            int face_count = render_chunks(&block_attrib, player);
            //int face_count = 0;
            render_signs(&text_attrib, player);
            render_sign(&text_attrib, player);

            // //TODO: FIX PALYERS RENDER
            render_players(&block_attrib, player);

            if (SHOW_WIREFRAME) {
                render_wireframe(&line_attrib, player);
            }
            // RENDER HUD //
            glClear(GL_DEPTH_BUFFER_BIT);
            if (SHOW_CROSSHAIRS) {
                render_crosshairs(&line_attrib);
            }

            if (SHOW_ITEM) {
                //TODO: TURN ON SHOW ITEM AGAIN
                render_item(&block_attrib);
            }

            //END OF FREEZE
            // RENDER TEXT //
            char text_buffer[1024];
            float ts = 12 * game->scale;
            float tx = ts / 2;
            float ty = game->height - ts;
            if (SHOW_INFO_TEXT) {
                int hour = time_of_day() * 24;
                char am_pm = hour < 12 ? 'a' : 'p';
                hour = hour % 12;
                hour = hour ? hour : 12;
                snprintf(
                    text_buffer, 1024,
                    "(%d, %d) (%.2f, %.2f, %.2f) [%d, %d, %d] %d%cm %dfps",
                    chunked(s->x), chunked(s->z), s->x, s->y, s->z,
                    game->player_count, game->chunk_count,
                    face_count * 2, hour, am_pm, fps.fps);
                render_text(&text_attrib, ALIGN_LEFT, tx, ty, ts, text_buffer);
                ty -= ts * 2;
            }
            if (SHOW_CHAT_TEXT) {
                for (int i = 0; i < MAX_MESSAGES; i++) {
                    int index = (game->message_index + i) % MAX_MESSAGES;
                    if (strlen(game->messages[index])) {
                        render_text(&text_attrib, ALIGN_LEFT, tx, ty, ts,
                                    game->messages[index]);
                        ty -= ts * 2;
                    }
                }
            }
            if (game->typing) {
                snprintf(text_buffer, 1024, "> %s", game->typing_buffer);
                render_text(&text_attrib, ALIGN_LEFT, tx, ty, ts, text_buffer);
                ty -= ts * 2;
            }
            if (SHOW_PLAYER_NAMES) {
                if (player != me) {
                    render_text(&text_attrib, ALIGN_CENTER,
                                game->width / 2, ts, ts, player->name);
                }
                Player *other = player_crosshair(player);
                if (other) {
                    render_text(&text_attrib, ALIGN_CENTER,
                                game->width / 2, game->height / 2 - ts - 24, ts,
                                other->name);
                }
            }
            // RENDER PICTURE IN PICTURE //
            if (game->observe2) {
                player = game->players + game->observe2;

                int pw = 256 * game->scale;
                int ph = 256 * game->scale;
                int offset = 32 * game->scale;
                int pad = 3 * game->scale;
                int sw = pw + pad * 2;
                int sh = ph + pad * 2;

                glEnable(GL_SCISSOR_TEST);
                glScissor(game->width - sw - offset + pad, offset - pad, sw, sh);
                glClear(GL_COLOR_BUFFER_BIT);
                glDisable(GL_SCISSOR_TEST);
                glClear(GL_DEPTH_BUFFER_BIT);
                glViewport(game->width - pw - offset, offset, pw, ph);

                game->width = pw;
                game->height = ph;
                game->ortho = 0;
                game->fov = 114;

                render_sky(&sky_attrib, player, sky_buffer);
                glClear(GL_DEPTH_BUFFER_BIT);
                render_chunks(&block_attrib, player);
                render_signs(&text_attrib, player);
                render_players(&block_attrib, player);
                glClear(GL_DEPTH_BUFFER_BIT);
                if (SHOW_PLAYER_NAMES) {
                    render_text(&text_attrib, ALIGN_CENTER,
                                pw / 2, ts, ts, player->name);
                }
            }
            // SWAP AND POLL //
            glfwSwapBuffers(game->window);
            glfwPollEvents();
            if (glfwWindowShouldClose(game->window)) {
                running = 0;
                break;
            }
            if (game->mode_changed) {
                game->mode_changed = 0;
                break;
            }
        }

        // SHUTDOWN //
        db_save_state(s->x, s->y, s->z, s->rx, s->ry);
        db_close();
        db_disable();
        client_stop();
        client_disable();
        del_buffer(sky_buffer);
        delete_all_chunks();
        delete_all_players();
    }

    glfwTerminate();
    curl_global_cleanup();
    return 0;
}
