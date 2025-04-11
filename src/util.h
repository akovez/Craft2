#ifndef _util_h_
#define _util_h_


#include "config.h"
#include "data.h"

#define PI 3.14159265359
#define DEGREES(radians) ((radians) * 180 / PI)
#define RADIANS(degrees) ((degrees) * PI / 180)
#define ABS(x) ((x) < 0 ? (-(x)) : (x))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define SIGN(x) (((x) > 0) - ((x) < 0))

#if DEBUG
    #define LOG(...) printf(__VA_ARGS__)
#else
    #define LOG(...)
#endif


int rand_int(int n);
double rand_double();
void update_fps(FPS *fps);

GLuint gen_buffer(GLsizei size, GLfloat *data);
void del_buffer(GLuint buffer);
GLfloat *malloc_faces(int components, int faces);
void *malloc_faces_new( unsigned long long vertexDataSize, int faces);
void *malloc_faces_new_player( unsigned long long vertexDataSize, int faces);

GLuint gen_faces(int components, int faces, GLfloat *data);
GLuint gen_faces_new(unsigned long long  vertexDataSize, int faces, void * data);
GLuint gen_faces_chunk(unsigned long long  vertexDataSize, int faces, void * data);
GLuint gen_indices_chunk(int faces, void * data);

GLuint make_shader(GLenum type, const char *source);
GLuint load_shader(GLenum type, const char *path);
GLuint make_program(GLuint shader1, GLuint shader2);
GLuint load_program(const char *path1, const char *path2);
GLuint load_compute_program(const char *path1);
GLuint createComputeBuffer(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
void dispatchComputeShader(GLuint program, GLuint outputBuffer, GLuint inputBuffer);
void updateComputeBuffer(GLuint buffer, GLsizeiptr size, const void* data);
void readBuffer(GLuint buffer, GLsizeiptr size, void* data);
void load_png_texture(const char *file_name);
char *tokenize(char *str, const char *delim, char **key);
int char_width(char input);
int string_width(const char *input);
int wrap(const char *input, int max_width, char *output, int max_length);
int chunked(float x);
int highest_block(float x, float z);
int is_plant(int w);
int is_obstacle(int w);
int is_transparent(int w);
int is_destructable(int w);
void get_motion_vector(int flying, int sz, int sx, float rx, float ry,
    float *vx, float *vy, float *vz);
int collide(int height, float *x, float *y, float *z);
Chunk* find_chunk(int p, int q);
int hit_test_face(Player *player, int *x, int *y, int *z, int *face);
int hit_test(
    int previous, float x, float y, float z, float rx, float ry,
    int *bx, int *by, int *bz);
void get_sight_vector(float rx, float ry, float *vx, float *vy, float *vz);
int chunk_distance(Chunk *chunk, int p, int q);
#endif
