#pragma once

#include <string>

struct GLFWwindow;

struct WindowConfig {
    int width = 1366;
    int height = 768;
    bool vsync = true;
    bool fullscreen = true;   // inicia em borderless fullscreen
    std::string title = "Projeto ICG";
};

// Cria janela + contexto OpenGL 3.3 Core. Retorna nullptr em caso de erro.
GLFWwindow* window_create(const WindowConfig& cfg);

// Destroi janela e encerra GLFW.
void window_destroy(GLFWwindow* win);

// Captura/libera o mouse.
void window_set_capture(GLFWwindow* win, bool capture);

// Delta do mouse desde a ultima chamada (em pixels). Zera internamente.
void window_get_mouse_delta(GLFWwindow* win, double& dx, double& dy);

// Alterna entre janela normal e borderless fullscreen no monitor atual.
// Usado pelo F11 no main.
void window_toggle_fullscreen(GLFWwindow* win);