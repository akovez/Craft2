//
// Created by akovez on 4/7/25.
//

#ifndef INPUT_H
#define INPUT_H
#include "GLFW/glfw3.h"


void handle_movement(double dt);

void on_key(GLFWwindow *window, int key, int scancode, int action, int mods);

void on_left_click();

void on_middle_click();

void on_right_click();

void parse_command(const char *buffer, int forward);

#endif //INPUT_H
