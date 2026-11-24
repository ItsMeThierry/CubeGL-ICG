#pragma once

#include "backend.h"
#include "camera.h"
#include "config.h"
#include "hud.h"
#include "messages.h"
#include "renderer.h"
#include "skybox.h"

struct GLFWwindow;

// Coordena janela, loop de renderizacao, entrada e comunicacao com o Backend.
class ClientApp {
public:
    ClientApp() = default;
    ~ClientApp();

    ClientApp(const ClientApp&) = delete;
    ClientApp& operator=(const ClientApp&) = delete;

    bool init(const char* config_path);
    void run();
    void shutdown();

private:
    InputState poll_input();

    Config         cfg_{};
    Backend        backend_;
    GLFWwindow*    win_ = nullptr;
    Camera         cam_{};
    Skybox         sky_{};
    ChunkRenderer  chunk_renderer_;
    Hud            hud_{};

    // Estado de borda para teclas/botoes.
    bool    f3_prev_        = false;
    bool    f11_prev_       = false;
    bool    prev_break_     = false;
    bool    prev_place_     = false;
    bool    prev_time_pause_= false;
    bool    prev_time_accel_= false;
    BlockID selected_block_ = 1;
};
