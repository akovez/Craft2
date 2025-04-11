#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "../deps/lodepng/lodepng.h"
#include "matrix.h"
#include "util.h"

#include <uthash.h>

#include "item.h"
#include "main.h"
#include "map.h"
#include "GL/glew.h"
#include "GL/gl.h"
#include "GLFW/glfw3.h"

int rand_int(int n) {
    int result;
    while (n <= (result = rand() / (RAND_MAX / n)));
    return result;
}

double rand_double() {
    return (double)rand() / (double)RAND_MAX;
}

void update_fps(FPS *fps) {
    fps->frames++;
    double now = glfwGetTime();
    double elapsed = now - fps->since;
    if (elapsed >= 1) {
        fps->fps = round(fps->frames / elapsed);
        fps->frames = 0;
        fps->since = now;
    }
}

char *load_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "fopen %s failed: %d %s\n", path, errno, strerror(errno));
        exit(1);
    }
    fseek(file, 0, SEEK_END);
    int length = ftell(file);
    rewind(file);
    char *data = calloc(length + 1, sizeof(char));
    fread(data, 1, length, file);
    fclose(file);
    return data;
}

GLuint gen_buffer(GLsizei size, GLfloat *data) {
    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return buffer;
}

// TODODO
GLuint gen_buffer_new(GLsizei size, void* data) {
    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return buffer;
}

void del_buffer(GLuint buffer) {
    glDeleteBuffers(1, &buffer);
}

//6, because there is 6 vertices per face.
GLfloat *malloc_faces(int components, int faces) {
    return malloc(sizeof(GLfloat) * 6 * components * faces);
}

void *malloc_faces_new( unsigned long long vertexDataSize, int faces) {
    return malloc(vertexDataSize * 4 * faces);
}

void *malloc_faces_new_player( unsigned long long vertexDataSize, int faces) {
    return malloc(vertexDataSize * 6 * faces);
}

//6, because there is 6 vertices per face, components is the amount of floats in the data.
GLuint gen_faces(int components, int faces, GLfloat *data) {
    GLuint buffer = gen_buffer(
        sizeof(GLfloat) * 6 * components * faces, data);
    free(data);
    return buffer;
}

// TODODO
GLuint gen_faces_chunk(unsigned long long  vertexDataSize, int faces, void * data) {
    GLuint buffer = gen_buffer_new(
        vertexDataSize * 4 * faces, data);
    free(data);
    return buffer;
}

// TODODO
GLuint gen_indices_chunk(int faces, void * data) {
    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int) * 6 * faces, data, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    free(data);
    return buffer;
}

GLuint gen_faces_new(unsigned long long  vertexDataSize, int faces, void * data) {
    GLuint buffer = gen_buffer_new(
        vertexDataSize * 6 * faces, data);
    free(data);
    return buffer;
}

GLuint make_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        GLchar *info = calloc(length, sizeof(GLchar));
        glGetShaderInfoLog(shader, length, NULL, info);
        fprintf(stderr, "glCompileShader failed:\n%s\n", info);
        free(info);
    }
    return shader;
}

GLuint load_shader(GLenum type, const char *path) {
    char *data = load_file(path);
    GLuint result = make_shader(type, data);
    free(data);
    return result;
}

GLuint make_program(GLuint shader1, GLuint shader2) {
    GLuint program = glCreateProgram();
    glAttachShader(program, shader1);
    glAttachShader(program, shader2);
    glLinkProgram(program);
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        GLchar *info = calloc(length, sizeof(GLchar));
        glGetProgramInfoLog(program, length, NULL, info);
        fprintf(stderr, "glLinkProgram failed: %s\n", info);
        free(info);
    }
    glDetachShader(program, shader1);
    glDetachShader(program, shader2);
    glDeleteShader(shader1);
    glDeleteShader(shader2);
    return program;
}

GLuint createComputeBuffer(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(target, buffer);
    glBufferData(target, size, data, usage);
    glBindBuffer(target, 0);
    return buffer;
}

void updateComputeBuffer(GLuint buffer, GLsizeiptr size, const void* data) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, size, data);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void dispatchComputeShader(GLuint program, GLuint outputBuffer, GLuint inputBuffer) {
    glUseProgram(program);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, inputBuffer); //input buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, outputBuffer);
    
    glDispatchCompute(RENDER_CHUNK_RADIUS * 2 / 32 /4 , RENDER_CHUNK_RADIUS * 2 / 32, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void readBuffer(GLuint buffer, GLsizeiptr size, void* data) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    void* ptr = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    memcpy(data, ptr, size);
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

GLuint load_compute_program(const char *path1){
    GLuint program = glCreateProgram();
    GLuint compute_shader = load_shader(GL_COMPUTE_SHADER, path1);
    glAttachShader(program, compute_shader);
    glLinkProgram(program);
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        GLchar *info = calloc(length, sizeof(GLchar));
        glGetProgramInfoLog(program, length, NULL, info);
        fprintf(stderr, "glLinkProgram failed: %s\n", info);
        free(info);
    }
    glDetachShader(program, compute_shader);
    glDeleteShader(compute_shader);
    return program;
}

GLuint load_program(const char *path1, const char *path2) {
    GLuint shader1 = load_shader(GL_VERTEX_SHADER, path1);
    GLuint shader2 = load_shader(GL_FRAGMENT_SHADER, path2);
    GLuint program = make_program(shader1, shader2);
    return program;
}

void flip_image_vertical(
    unsigned char *data, unsigned int width, unsigned int height)
{
    unsigned int size = width * height * 4;
    unsigned int stride = sizeof(char) * width * 4;
    unsigned char *new_data = malloc(sizeof(unsigned char) * size);
    for (unsigned int i = 0; i < height; i++) {
        unsigned int j = height - i - 1;
        memcpy(new_data + j * stride, data + i * stride, stride);
    }
    memcpy(data, new_data, size);
    free(new_data);
}

void load_png_texture(const char *file_name) {
    unsigned int error;
    unsigned char *data;
    unsigned int width, height;
    error = lodepng_decode32_file(&data, &width, &height, file_name);
    if (error) {
        fprintf(stderr, "load_png_texture %s failed, error %u: %s\n", file_name, error, lodepng_error_text(error));
        exit(1);
    }
    flip_image_vertical(data, width, height);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
        GL_UNSIGNED_BYTE, data);
    free(data);
}

char *tokenize(char *str, const char *delim, char **key) {
    char *result;
    if (str == NULL) {
        str = *key;
    }
    str += strspn(str, delim);
    if (*str == '\0') {
        return NULL;
    }
    result = str;
    str += strcspn(str, delim);
    if (*str) {
        *str++ = '\0';
    }
    *key = str;
    return result;
}

int char_width(char input) {
    static const int lookup[128] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        4, 2, 4, 7, 6, 9, 7, 2, 3, 3, 4, 6, 3, 5, 2, 7,
        6, 3, 6, 6, 6, 6, 6, 6, 6, 6, 2, 3, 5, 6, 5, 7,
        8, 6, 6, 6, 6, 6, 6, 6, 6, 4, 6, 6, 5, 8, 8, 6,
        6, 7, 6, 6, 6, 6, 8,10, 8, 6, 6, 3, 6, 3, 6, 6,
        4, 7, 6, 6, 6, 6, 5, 6, 6, 2, 5, 5, 2, 9, 6, 6,
        6, 6, 6, 6, 5, 6, 6, 6, 6, 6, 6, 4, 2, 5, 7, 0
    };
    return lookup[input];
}

int string_width(const char *input) {
    int result = 0;
    int length = strlen(input);
    for (int i = 0; i < length; i++) {
        result += char_width(input[i]);
    }
    return result;
}

int wrap(const char *input, int max_width, char *output, int max_length) {
    *output = '\0';
    char *text = malloc(sizeof(char) * (strlen(input) + 1));
    strcpy(text, input);
    int space_width = char_width(' ');
    int line_number = 0;
    char *key1, *key2;
    char *line = tokenize(text, "\r\n", &key1);
    while (line) {
        int line_width = 0;
        char *token = tokenize(line, " ", &key2);
        while (token) {
            int token_width = string_width(token);
            if (line_width) {
                if (line_width + token_width > max_width) {
                    line_width = 0;
                    line_number++;
                    strncat(output, "\n", max_length - strlen(output) - 1);
                }
                else {
                    strncat(output, " ", max_length - strlen(output) - 1);
                }
            }
            strncat(output, token, max_length - strlen(output) - 1);
            line_width += token_width + space_width;
            token = tokenize(NULL, " ", &key2);
        }
        line_number++;
        strncat(output, "\n", max_length - strlen(output) - 1);
        line = tokenize(NULL, "\r\n", &key1);
    }
    free(text);
    return line_number;
}

int highest_block(float x, float z) {
    int result = -1;
    int nx = roundf(x);
    int nz = roundf(z);
    int p = chunked(x);
    int q = chunked(z);
    Chunk* chunk = find_chunk(p, q);
    if (chunk) {
        Map* map = &chunk->map;
        MAP_FOR_EACH(map, ex, ey, ez, ew) {
            if (is_obstacle(ew) && ex == nx && ez == nz) {
                result = MAX(result, ey);
            }
        } END_MAP_FOR_EACH;
    }
    return result;
}


int chunked(float x) {
    return floorf(roundf(x) / CHUNK_SIZE);
}

int is_plant(int w) {
    switch (w) {
        case TALL_GRASS:
        case YELLOW_FLOWER:
        case RED_FLOWER:
        case PURPLE_FLOWER:
        case SUN_FLOWER:
        case WHITE_FLOWER:
        case BLUE_FLOWER:
            return 1;
        default:
            return 0;
    }
}

int is_obstacle(int w) {
    w = ABS(w);
    if (is_plant(w)) {
        return 0;
    }
    switch (w) {
        case EMPTY:
        case CLOUD:
            return 0;
        default:
            return 1;
    }
}

int is_transparent(int w) {
    if (w == EMPTY) {
        return 1;
    }
    w = ABS(w);
    if (is_plant(w)) {
        return 1;
    }
    switch (w) {
        case EMPTY:
        case GLASS:
        case LEAVES:
            return 1;
        default:
            return 0;
    }
}

int is_destructable(int w) {
    switch (w) {
        case EMPTY:
        case CLOUD:
            return 0;
        default:
            return 1;
    }
}


void get_motion_vector(int flying, int sz, int sx, float rx, float ry,
    float *vx, float *vy, float *vz) {
    *vx = 0; *vy = 0; *vz = 0;
    if (!sz && !sx) {
        return;
    }
    float strafe = atan2f(sz, sx);
    if (flying) {
        float m = cosf(ry);
        float y = sinf(ry);
        if (sx) {
            if (!sz) {
                y = 0;
            }
            m = 1;
        }
        if (sz > 0) {
            y = -y;
        }
        *vx = cosf(rx + strafe) * m;
        *vy = y;
        *vz = sinf(rx + strafe) * m;
    }
    else {
        *vx = cosf(rx + strafe);
        *vy = 0;
        *vz = sinf(rx + strafe);
    }
}



int collide(int height, float *x, float *y, float *z) {
    int result = 0;
    int p = chunked(*x);
    int q = chunked(*z);
    Chunk *chunk = find_chunk(p, q);
    if (!chunk) {
        return result;
    }
    Map *map = &chunk->map;
    int nx = roundf(*x);
    int ny = roundf(*y);
    int nz = roundf(*z);
    float px = *x - nx;
    float py = *y - ny;
    float pz = *z - nz;
    float pad = 0.25;
    for (int dy = 0; dy < height; dy++) {
        if (px < -pad && is_obstacle(map_get(map, nx - 1, ny - dy, nz))) {
            *x = nx - pad;
        }
        if (px > pad && is_obstacle(map_get(map, nx + 1, ny - dy, nz))) {
            *x = nx + pad;
        }
        if (py < -pad && is_obstacle(map_get(map, nx, ny - dy - 1, nz))) {
            *y = ny - pad;
            result = 1;
        }
        if (py > pad && is_obstacle(map_get(map, nx, ny - dy + 1, nz))) {
            *y = ny + pad;
            result = 1;
        }
        if (pz < -pad && is_obstacle(map_get(map, nx, ny - dy, nz - 1))) {
            *z = nz - pad;
        }
        if (pz > pad && is_obstacle(map_get(map, nx, ny - dy, nz + 1))) {
            *z = nz + pad;
        }
    }
    return result;
}

Chunk* find_chunk(int p, int q) {
    Chunk* chunk;
    ChunkKey key = {p, q};

    // printf("Key to find: %d, %d\n", key.p, key.q);

    HASH_FIND(hh, game->chunks_hash, &key, sizeof(int) * 2, chunk);
    // printf("Key found\n");
    return chunk;
}


int _hit_test(
    Map *map, float max_distance, int previous,
    float x, float y, float z,
    float vx, float vy, float vz,
    int *hx, int *hy, int *hz)
{
    int m = 32;
    int px = 0;
    int py = 0;
    int pz = 0;
    for (int i = 0; i < max_distance * m; i++) {
        int nx = roundf(x);
        int ny = roundf(y);
        int nz = roundf(z);
        if (nx != px || ny != py || nz != pz) {
            int hw = map_get(map, nx, ny, nz);
            if (hw > 0) {
                if (previous) {
                    *hx = px; *hy = py; *hz = pz;
                }
                else {
                    *hx = nx; *hy = ny; *hz = nz;
                }
                return hw;
            }
            px = nx; py = ny; pz = nz;
        }
        x += vx / m; y += vy / m; z += vz / m;
    }
    return 0;
}

int hit_test(
    int previous, float x, float y, float z, float rx, float ry,
    int *bx, int *by, int *bz)
{
    int result = 0;
    float best = 0;
    int p = chunked(x);
    int q = chunked(z);
    float vx, vy, vz;
    get_sight_vector(rx, ry, &vx, &vy, &vz);
    for (int i = 0; i < game->chunk_count; i++) {
        Chunk *chunk = game->chunks + i;
        if (chunk_distance(chunk, p, q) > 1) {
            continue;
        }
        int hx, hy, hz;
        int hw = _hit_test(&chunk->map, 8, previous,
            x, y, z, vx, vy, vz, &hx, &hy, &hz);
        if (hw > 0) {
            float d = sqrtf(
                powf(hx - x, 2) + powf(hy - y, 2) + powf(hz - z, 2));
            if (best == 0 || d < best) {
                best = d;
                *bx = hx; *by = hy; *bz = hz;
                result = hw;
            }
        }
    }
    return result;
}

int hit_test_face(Player *player, int *x, int *y, int *z, int *face) {
    State *s = &player->state;
    int w = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, x, y, z);
    if (is_obstacle(w)) {
        int hx, hy, hz;
        hit_test(1, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
        int dx = hx - *x;
        int dy = hy - *y;
        int dz = hz - *z;
        if (dx == -1 && dy == 0 && dz == 0) {
            *face = 0; return 1;
        }
        if (dx == 1 && dy == 0 && dz == 0) {
            *face = 1; return 1;
        }
        if (dx == 0 && dy == 0 && dz == -1) {
            *face = 2; return 1;
        }
        if (dx == 0 && dy == 0 && dz == 1) {
            *face = 3; return 1;
        }
        if (dx == 0 && dy == 1 && dz == 0) {
            int degrees = roundf(DEGREES(atan2f(s->x - hx, s->z - hz)));
            if (degrees < 0) {
                degrees += 360;
            }
            int top = ((degrees + 45) / 90) % 4;
            *face = 4 + top; return 1;
        }
    }
    return 0;
}

void get_sight_vector(float rx, float ry, float *vx, float *vy, float *vz) {
    float m = cosf(ry);
    *vx = cosf(rx - RADIANS(90)) * m;
    *vy = sinf(ry);
    *vz = sinf(rx - RADIANS(90)) * m;
}

int chunk_distance(Chunk *chunk, int p, int q) {
    int dp = ABS(chunk->key.p - p);
    int dq = ABS(chunk->key.q - q);
    return MAX(dp, dq);
}
