//
// Created by akovez on 4/7/25.
//
#include <math.h>
#include <stdio.h>

#include "client.h"
#include "config.h"
#include "db.h"
#include "item.h"
#include "main.h"
#include "util.h"
#include "input.h"

#include "world.h"
#include "GLFW/glfw3.h"

void on_key(GLFWwindow *window, int key, int scancode, int action, int mods) {
    int control = mods & (GLFW_MOD_CONTROL | GLFW_MOD_SUPER);
    int exclusive =
        glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED;
    if (action == GLFW_RELEASE) {
        return;
    }
    if (key == GLFW_KEY_BACKSPACE) {
        if (game->typing) {
            int n = strlen(game->typing_buffer);
            if (n > 0) {
                game->typing_buffer[n - 1] = '\0';
            }
        }
    }
    if (action != GLFW_PRESS) {
        return;
    }
    if (key == GLFW_KEY_ESCAPE) {
        if (game->typing) {
            game->typing = 0;
        }
        else if (exclusive) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    if (key == GLFW_KEY_ENTER) {
        if (game->typing) {
            if (mods & GLFW_MOD_SHIFT) {
                int n = strlen(game->typing_buffer);
                if (n < MAX_TEXT_LENGTH - 1) {
                    game->typing_buffer[n] = '\r';
                    game->typing_buffer[n + 1] = '\0';
                }
            }
            else {
                game->typing = 0;
                if (game->typing_buffer[0] == CRAFT_KEY_SIGN) {
                    Player *player = game->players;
                    int x, y, z, face;
                    if (hit_test_face(player, &x, &y, &z, &face)) {
                        set_sign(x, y, z, face, game->typing_buffer + 1);
                    }
                }
                else if (game->typing_buffer[0] == '/') {
                    parse_command(game->typing_buffer, 1);
                }
                else {
                    client_talk(game->typing_buffer);
                }
            }
        }
        else {
            if (control) {
                on_right_click();
            }
            else {
                on_left_click();
            }
        }
    }
    if (control && key == 'V') {
        const char *buffer = glfwGetClipboardString(window);
        if (game->typing) {
            game->suppress_char = 1;
            strncat(game->typing_buffer, buffer,
                MAX_TEXT_LENGTH - strlen(game->typing_buffer) - 1);
        }
        else {
            parse_command(buffer, 0);
        }
    }
    if (!game->typing) {
        if (key == CRAFT_KEY_FLY) {
            game->flying = !game->flying;
        }
        if (key >= '1' && key <= '9') {
            game->item_index = key - '1';
        }
        if (key == '0') {
            game->item_index = 9;
        }
        if (key == CRAFT_KEY_ITEM_NEXT) {
            game->item_index = (game->item_index + 1) % item_count;
        }
        if (key == CRAFT_KEY_ITEM_PREV) {
            game->item_index--;
            if (game->item_index < 0) {
                game->item_index = item_count - 1;
            }
        }
        if (key == CRAFT_KEY_OBSERVE) {
            game->observe1 = (game->observe1 + 1) % game->player_count;
        }
        if (key == CRAFT_KEY_OBSERVE_INSET) {
            game->observe2 = (game->observe2 + 1) % game->player_count;
        }
    }
}

void on_left_click() {
    State *s = &game->players->state;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (hy > 0 && hy < 256 && is_destructable(hw)) {
        set_block(hx, hy, hz, 0);
        record_block(hx, hy, hz, 0);
        if (is_plant(get_block(hx, hy + 1, hz))) {
            set_block(hx, hy + 1, hz, 0);
        }
    }
}

void on_right_click() {
    State *s = &game->players->state;
    int hx, hy, hz;
    int hw = hit_test(1, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    if (hy > 0 && hy < 256 && is_obstacle(hw)) {
        if (!player_intersects_block(2, s->x, s->y, s->z, hx, hy, hz)) {
            set_block(hx, hy, hz, items[game->item_index]);
            record_block(hx, hy, hz, items[game->item_index]);
        }
    }
}

void on_middle_click() {
    State *s = &game->players->state;
    int hx, hy, hz;
    int hw = hit_test(0, s->x, s->y, s->z, s->rx, s->ry, &hx, &hy, &hz);
    for (int i = 0; i < item_count; i++) {
        if (items[i] == hw) {
            game->item_index = i;
            break;
        }
    }
}


void handle_movement(double dt) {
    static float dy = 0;
    State *s = &game->players->state;
    int sz = 0;
    int sx = 0;
    if (!game->typing) {
        float m = dt * 1.0;
        game->ortho = glfwGetKey(game->window, CRAFT_KEY_ORTHO) ? 64 : 0;
        game->fov = glfwGetKey(game->window, CRAFT_KEY_ZOOM) ? 30 : 90;
        if (glfwGetKey(game->window, CRAFT_KEY_FORWARD)) sz--;
        if (glfwGetKey(game->window, CRAFT_KEY_BACKWARD)) sz++;
        if (glfwGetKey(game->window, CRAFT_KEY_LEFT)) sx--;
        if (glfwGetKey(game->window, CRAFT_KEY_RIGHT)) sx++;
        if (glfwGetKey(game->window, GLFW_KEY_LEFT)) s->rx -= m;
        if (glfwGetKey(game->window, GLFW_KEY_RIGHT)) s->rx += m;
        if (glfwGetKey(game->window, GLFW_KEY_UP)) s->ry += m;
        if (glfwGetKey(game->window, GLFW_KEY_DOWN)) s->ry -= m;
    }
    float vx, vy, vz;
    get_motion_vector(game->flying, sz, sx, s->rx, s->ry, &vx, &vy, &vz);
    if (!game->typing) {
        if (glfwGetKey(game->window, CRAFT_KEY_JUMP)) {
            if (game->flying) {
                vy = 1;
            }
            else if (dy == 0) {
                dy = 8;
            }
        }
    }
    float speed = game->flying ? 20 : 5;
    int estimate = roundf(sqrtf(
        pow(vx * speed, 2) +
        powf(vy * speed + ABS(dy) * 2, 2) +
        powf(vz * speed, 2)) * dt * 8);
    int step = MAX(8, estimate);
    float ut = dt / step;
    vx = vx * ut * speed;
    vy = vy * ut * speed;
    vz = vz * ut * speed;
    for (int i = 0; i < step; i++) {
        if (game->flying) {
            dy = 0;
        }
        else {
            dy -= ut * 25;
            dy = MAX(dy, -250);
        }
        s->x += vx;
        s->y += vy + dy * ut;
        s->z += vz;
        if (collide(2, &s->x, &s->y, &s->z)) {
            dy = 0;
        }
    }
    if (s->y < 0) {
        s->y = highest_block(s->x, s->z) + 2;
    }
}


void parse_command(const char *buffer, int forward) {
    char username[128] = {0};
    char token[128] = {0};
    char server_addr[MAX_ADDR_LENGTH];
    int server_port = DEFAULT_PORT;
    char filename[MAX_PATH_LENGTH];
    int radius, count, xc, yc, zc;
    if (sscanf(buffer, "/identity %128s %128s", username, token) == 2) {
        db_auth_set(username, token);
        add_message("Successfully imported identity token!");
        login();
    }
    else if (strcmp(buffer, "/logout") == 0) {
        db_auth_select_none();
        login();
    }
    else if (sscanf(buffer, "/login %128s", username) == 1) {
        if (db_auth_select(username)) {
            login();
        }
        else {
            add_message("Unknown username.");
        }
    }
    else if (sscanf(buffer,
        "/online %128s %d", server_addr, &server_port) >= 1)
    {
        game->mode_changed = 1;
        game->mode = MODE_ONLINE;
        strncpy(game->server_addr, server_addr, MAX_ADDR_LENGTH);
        game->server_port = server_port;
        snprintf(game->db_path, MAX_PATH_LENGTH,
            "cache.%s.%d.db", game->server_addr, game->server_port);
    }
    else if (sscanf(buffer, "/offline %128s", filename) == 1) {
        game->mode_changed = 1;
        game->mode = MODE_OFFLINE;
        snprintf(game->db_path, MAX_PATH_LENGTH, "%s.db", filename);
    }
    else if (strcmp(buffer, "/offline") == 0) {
        game->mode_changed = 1;
        game->mode = MODE_OFFLINE;
        snprintf(game->db_path, MAX_PATH_LENGTH, "%s", DB_PATH);
    }
    else if (sscanf(buffer, "/view %d", &radius) == 1) {
        if (radius >= 1 && radius <= 24) {
            game->create_radius = radius;
            game->render_radius = radius;
            game->delete_radius = radius + 4;
        }
        else {
            add_message("Viewing distance must be between 1 and 24.");
        }
    }
    else if (strcmp(buffer, "/copy") == 0) {
        copy();
    }
    else if (strcmp(buffer, "/paste") == 0) {
        paste();
    }
    else if (strcmp(buffer, "/tree") == 0) {
        tree(&game->block0);
    }
    else if (sscanf(buffer, "/array %d %d %d", &xc, &yc, &zc) == 3) {
        array(&game->block1, &game->block0, xc, yc, zc);
    }
    else if (sscanf(buffer, "/array %d", &count) == 1) {
        array(&game->block1, &game->block0, count, count, count);
    }
    else if (strcmp(buffer, "/fcube") == 0) {
        cube(&game->block0, &game->block1, 1);
    }
    else if (strcmp(buffer, "/cube") == 0) {
        cube(&game->block0, &game->block1, 0);
    }
    else if (sscanf(buffer, "/fsphere %d", &radius) == 1) {
        sphere(&game->block0, radius, 1, 0, 0, 0);
    }
    else if (sscanf(buffer, "/sphere %d", &radius) == 1) {
        sphere(&game->block0, radius, 0, 0, 0, 0);
    }
    else if (sscanf(buffer, "/fcirclex %d", &radius) == 1) {
        sphere(&game->block0, radius, 1, 1, 0, 0);
    }
    else if (sscanf(buffer, "/circlex %d", &radius) == 1) {
        sphere(&game->block0, radius, 0, 1, 0, 0);
    }
    else if (sscanf(buffer, "/fcircley %d", &radius) == 1) {
        sphere(&game->block0, radius, 1, 0, 1, 0);
    }
    else if (sscanf(buffer, "/circley %d", &radius) == 1) {
        sphere(&game->block0, radius, 0, 0, 1, 0);
    }
    else if (sscanf(buffer, "/fcirclez %d", &radius) == 1) {
        sphere(&game->block0, radius, 1, 0, 0, 1);
    }
    else if (sscanf(buffer, "/circlez %d", &radius) == 1) {
        sphere(&game->block0, radius, 0, 0, 0, 1);
    }
    else if (sscanf(buffer, "/fcylinder %d", &radius) == 1) {
        cylinder(&game->block0, &game->block1, radius, 1);
    }
    else if (sscanf(buffer, "/cylinder %d", &radius) == 1) {
        cylinder(&game->block0, &game->block1, radius, 0);
    }
    else if (forward) {
        client_talk(buffer);
    }
}
