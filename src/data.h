//
// Created by akovez on 4/7/25.
//

#ifndef DATA_H
#define DATA_H
#include <uthash.h>

#include "../deps/tinycthread/tinycthread.h"
#include "GL/glew.h"
#include "config.h"
#include "GLFW/glfw3.h"


typedef struct {
    unsigned int fps;
    unsigned int frames;
    double since;
} FPS;

#define MAX_CHUNKS DELETE_CHUNK_RADIUS * 2 * DELETE_CHUNK_RADIUS * 2
#define MAX_PLAYERS 128
#define WORKERS 4
#define MAX_TEXT_LENGTH 256
#define MAX_NAME_LENGTH 32
#define MAX_PATH_LENGTH 256
#define MAX_ADDR_LENGTH 256

#define ALIGN_LEFT 0
#define ALIGN_CENTER 1
#define ALIGN_RIGHT 2

#define MODE_OFFLINE 0
#define MODE_ONLINE 1

#define WORKER_IDLE 0
#define WORKER_BUSY 1
#define WORKER_DONE 2
#define MAX_SIGN_LENGTH 64


typedef struct {
    int p;
    int q;
} ChunkKey;

typedef union {
    unsigned int value;
    struct {
        unsigned char x;
        unsigned char y;
        unsigned char z;
        char w;
    } e;
} MapEntry;

typedef struct {
    int dx;
    int dy;
    int dz;
    unsigned int mask;
    unsigned int size;
    MapEntry *data;
} Map;

typedef struct {
    int x;
    int y;
    int z;
    int face;
    char text[MAX_SIGN_LENGTH];
} Sign;

typedef struct {
    unsigned int capacity;
    unsigned int size;
    Sign *data;
} SignList;

typedef struct {
    ChunkKey key;
    Map map;
    Map lights;
    SignList signs;
    int faces;
    int sign_faces;
    int dirty;
    int miny;
    int maxy;
    GLuint buffer;
    GLuint indices_buffer;
    GLuint sign_buffer;
    UT_hash_handle hh;
} Chunk;

typedef struct {
    unsigned int xyz;
    unsigned int uvts;
} VertexData;

typedef struct {
    int p;
    int q;
    int load;
    Map *block_maps[3][3];
    Map *light_maps[3][3];
    int miny;
    int maxy;
    int faces;
    VertexData *data;
    int *indices_data;
} WorkerItem;

typedef struct {
    int index;
    int state;
    thrd_t thrd;
    mtx_t mtx;
    cnd_t cnd;
    WorkerItem item;
} Worker;

typedef struct {
    int x;
    int y;
    int z;
    int w;
} Block;

typedef struct {
    float x;
    float y;
    float z;
    float rx;
    float ry;
    float t;
} State;

typedef struct {
    int id;
    char name[MAX_NAME_LENGTH];
    State state;
    State state1;
    State state2;
    GLuint buffer;
} Player;

typedef struct {
    GLuint program;
    GLuint position;
    GLuint normal;
    GLuint uv;
    GLuint matrix;
    GLuint sampler;
    GLuint camera;
    GLuint timer;
    GLuint extra1;
    GLuint extra2;
    GLuint extra3;
    GLuint extra4;
    GLuint chunk_pos;
    GLuint position_uint;
    GLuint uvts;
    GLuint chunk_size;
    GLuint uvScales;

    GLuint compute_program;
    GLuint compute_planes_input;
    GLuint compute_output;
    GLsync compute_sync;
    char* lookup_table;
} Attrib;

typedef struct Model {
    GLFWwindow *window;
    Worker workers[WORKERS];
    Chunk chunks[MAX_CHUNKS];
    Chunk *chunks_hash;
    int chunk_count;
    int create_radius;
    int render_radius;
    int delete_radius;
    int sign_radius;
    Player players[MAX_PLAYERS];
    int player_count;
    int typing;
    char typing_buffer[MAX_TEXT_LENGTH];
    int message_index;
    char messages[MAX_MESSAGES][MAX_TEXT_LENGTH];
    int width;
    int height;
    int observe1;
    int observe2;
    int flying;
    int item_index;
    int scale;
    int ortho;
    float fov;
    int suppress_char;
    int mode;
    int mode_changed;
    char db_path[MAX_PATH_LENGTH];
    char server_addr[MAX_ADDR_LENGTH];
    int server_port;
    int day_length;
    int time_changed;
    Block block0;
    Block block1;
    Block copy0;
    Block copy1;
} Model;


#endif //DATA_H
