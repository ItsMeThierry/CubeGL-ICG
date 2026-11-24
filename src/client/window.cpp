#include "window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdio>

static double g_last_x = 0.0;
static double g_last_y = 0.0;
static bool   g_first_mouse = true;

// Estado para restaurar a janela ao sair do fullscreen.
static bool g_is_fullscreen = false;
static int  g_saved_x = 0;
static int  g_saved_y = 0;
static int  g_saved_w = 0;
static int  g_saved_h = 0;

static void framebuffer_cb(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
}

// Coloca a janela em borderless fullscreen no monitor atual, guardando a
// geometria anterior para poder restaurar depois.
static void enter_fullscreen(GLFWwindow* win) {
    if (g_is_fullscreen) return;

    glfwGetWindowPos(win, &g_saved_x, &g_saved_y);
    glfwGetWindowSize(win, &g_saved_w, &g_saved_h);

    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    if (!mon) {
        std::fprintf(stderr,
            "[window] nenhum monitor primario; mantendo em janela.\n");
        return;
    }
    const GLFWvidmode* mode = glfwGetVideoMode(mon);
    if (!mode) {
        std::fprintf(stderr,
            "[window] nao foi possivel obter o modo de video; mantendo em janela.\n");
        return;
    }

    // Borderless: monitor=nullptr + posicao/tamanho do monitor.
    glfwSetWindowMonitor(win, nullptr, 0, 0, mode->width, mode->height, mode->refreshRate);
    g_is_fullscreen = true;
}

// Restaura a geometria guardada ao entrar em fullscreen.
static void leave_fullscreen(GLFWwindow* win) {
    if (!g_is_fullscreen) return;

    if (g_saved_w > 0 && g_saved_h > 0) {
        glfwSetWindowMonitor(win, nullptr, g_saved_x, g_saved_y,
                             g_saved_w, g_saved_h, GLFW_DONT_CARE);
    } else {
        // Fallback: volta para o tamanho padrao.
        glfwSetWindowMonitor(win, nullptr, 100, 100, 1366, 768, GLFW_DONT_CARE);
    }
    g_is_fullscreen = false;
}

GLFWwindow* window_create(const WindowConfig& cfg) {
    if (!glfwInit()) {
        std::fprintf(stderr, "[window] falha ao inicializar GLFW\n");
        return nullptr;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Cria sempre como janela primeiro; se fullscreen, promove em seguida.
    GLFWwindow* win = glfwCreateWindow(cfg.width, cfg.height, cfg.title.c_str(),
                                       nullptr, nullptr);
    if (!win) {
        std::fprintf(stderr, "[window] falha ao criar janela OpenGL 3.3 Core\n");
        glfwTerminate();
        return nullptr;
    }

    glfwMakeContextCurrent(win);
    glfwSetFramebufferSizeCallback(win, framebuffer_cb);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::fprintf(stderr, "[window] falha ao carregar GLAD\n");
        glfwDestroyWindow(win);
        glfwTerminate();
        return nullptr;
    }

    glfwSwapInterval(cfg.vsync ? 1 : 0);
    glEnable(GL_DEPTH_TEST);

    if (cfg.fullscreen) {
        enter_fullscreen(win);
    }

    std::printf("[window] OpenGL %s\n", glGetString(GL_VERSION));
    return win;
}

void window_destroy(GLFWwindow* win) {
    if (win) glfwDestroyWindow(win);
    glfwTerminate();
}

void window_set_capture(GLFWwindow* win, bool capture) {
    glfwSetInputMode(win, GLFW_CURSOR, capture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (capture) g_first_mouse = true;
}

void window_get_mouse_delta(GLFWwindow* win, double& dx, double& dy) {
    dx = 0.0;
    dy = 0.0;
    if (glfwGetInputMode(win, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
        // Atualiza a referencia para nao acumular um salto ao recapturar.
        glfwGetCursorPos(win, &g_last_x, &g_last_y);
        return;
    }
    double cx = 0.0, cy = 0.0;
    glfwGetCursorPos(win, &cx, &cy);
    if (g_first_mouse) {
        g_last_x = cx;
        g_last_y = cy;
        g_first_mouse = false;
        return;
    }
    dx = cx - g_last_x;
    dy = cy - g_last_y;
    g_last_x = cx;
    g_last_y = cy;
}

void window_toggle_fullscreen(GLFWwindow* win) {
    if (!win) return;
    if (g_is_fullscreen) leave_fullscreen(win);
    else                 enter_fullscreen(win);
}