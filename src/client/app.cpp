#include "app.h"
#include "window.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>

ClientApp::~ClientApp() {
    shutdown();
}

bool ClientApp::init(const char* config_path) {
    config_load(config_path, cfg_); // tolerante a falha; usa defaults

    if (!backend_.init(config_path)) {
        std::fprintf(stderr, "[app] falha ao iniciar backend\n");
        return false;
    }

    WindowConfig wcfg;
    wcfg.width      = cfg_.win_w;
    wcfg.height     = cfg_.win_h;
    wcfg.vsync      = cfg_.vsync;
    wcfg.fullscreen = cfg_.fullscreen;

    win_ = window_create(wcfg);
    if (!win_) return false;

    cam_.fov = cfg_.fov;

    if (!sky_.init()) {
        shutdown();
        return false;
    }

    if (!chunk_renderer_.init("assets/shaders/chunk.vert",
                              "assets/shaders/chunk.frag",
                              "assets/texturas/atlas.png",
                              cfg_)) {
        std::fprintf(stderr, "[app] falha ao iniciar ChunkRenderer\n");
        shutdown();
        return false;
    }

    if (!hud_.init()) {
        shutdown();
        return false;
    }

    window_set_capture(win_, true);
    backend_.start();
    return true;
}

InputState ClientApp::poll_input() {
    if (glfwGetKey(win_, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(win_, GLFW_TRUE);

    // HUD toggle (F3).
    bool f3_now = glfwGetKey(win_, GLFW_KEY_F3) == GLFW_PRESS;
    if (f3_now && !f3_prev_) hud_.visible = !hud_.visible;
    f3_prev_ = f3_now;

    // Fullscreen toggle (F11).
    bool f11_now = glfwGetKey(win_, GLFW_KEY_F11) == GLFW_PRESS;
    if (f11_now && !f11_prev_) window_toggle_fullscreen(win_);
    f11_prev_ = f11_now;

    // Recaptura do mouse.
    if (glfwGetMouseButton(win_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS &&
        glfwGetInputMode(win_, GLFW_CURSOR) != GLFW_CURSOR_DISABLED) {
        window_set_capture(win_, true);
    }

    InputState in{};
    if (glfwGetKey(win_, GLFW_KEY_W) == GLFW_PRESS) in.move_forward += 1.0f;
    if (glfwGetKey(win_, GLFW_KEY_S) == GLFW_PRESS) in.move_forward -= 1.0f;
    if (glfwGetKey(win_, GLFW_KEY_D) == GLFW_PRESS) in.move_right   += 1.0f;
    if (glfwGetKey(win_, GLFW_KEY_A) == GLFW_PRESS) in.move_right   -= 1.0f;
    if (glfwGetKey(win_, GLFW_KEY_SPACE) == GLFW_PRESS)        in.move_up += 1.0f;
    if (glfwGetKey(win_, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) in.move_up -= 1.0f;
    in.shift_held = glfwGetKey(win_, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

    double mdx = 0.0, mdy = 0.0;
    window_get_mouse_delta(win_, mdx, mdy);
    in.mouse_dx = (float)mdx;
    in.mouse_dy = (float)mdy;

    bool b_break = glfwGetMouseButton(win_, GLFW_MOUSE_BUTTON_LEFT)  == GLFW_PRESS;
    bool b_place = glfwGetMouseButton(win_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    in.break_pressed = b_break && !prev_break_;
    in.place_pressed = b_place && !prev_place_;
    prev_break_ = b_break;
    prev_place_ = b_place;

    if (glfwGetKey(win_, GLFW_KEY_1) == GLFW_PRESS) selected_block_ = 1;
    if (glfwGetKey(win_, GLFW_KEY_2) == GLFW_PRESS) selected_block_ = 2;
    if (glfwGetKey(win_, GLFW_KEY_3) == GLFW_PRESS) selected_block_ = 3;
    if (glfwGetKey(win_, GLFW_KEY_4) == GLFW_PRESS) selected_block_ = 4;
    if (glfwGetKey(win_, GLFW_KEY_5) == GLFW_PRESS) selected_block_ = 5;
    if (glfwGetKey(win_, GLFW_KEY_6) == GLFW_PRESS) selected_block_ = 6;
    if (glfwGetKey(win_, GLFW_KEY_7) == GLFW_PRESS) selected_block_ = 7;
    if (glfwGetKey(win_, GLFW_KEY_8) == GLFW_PRESS) selected_block_ = 8;
    if (glfwGetKey(win_, GLFW_KEY_9) == GLFW_PRESS) selected_block_ = 9;
    if (glfwGetKey(win_, GLFW_KEY_0) == GLFW_PRESS) selected_block_ = 10;
    in.selected_block = selected_block_;

    bool tp = glfwGetKey(win_, GLFW_KEY_LEFT_BRACKET)  == GLFW_PRESS;
    bool ta = glfwGetKey(win_, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
    if (tp && !prev_time_pause_) in.time_pause_toggle = true;
    prev_time_pause_ = tp;
    if (ta && !prev_time_accel_) in.time_speed_delta += 1.0f;
    prev_time_accel_ = ta;

    return in;
}

void ClientApp::run() {
    // Cores provisorias do ceu (semana 4 interpola do tempo do dia).
    const glm::vec3 zenith(0.239f, 0.482f, 0.851f);
    const glm::vec3 horizon(0.749f, 0.890f, 1.000f);

    // Orcamento de uploads por frame (documento: "por exemplo, 4 chunks").
    const int UPLOAD_BUDGET_PER_FRAME = 4;

    double last = glfwGetTime();
    double fps_timer = 0.0;
    int fps_frames = 0;
    int fps_value = 0;

    while (!glfwWindowShouldClose(win_)) {
        double now = glfwGetTime();
        float dt = (float)(now - last);
        last = now;
        if (dt > 0.1f) dt = 0.1f;

        glfwPollEvents();
        backend_.input_queue().push(poll_input());

        // --- Snapshot ---
        PlayerSnapshot snap = backend_.latest_snapshot();
        cam_.position = glm::vec3(snap.px, snap.py, snap.pz);
        cam_.yaw      = snap.yaw;
        cam_.pitch    = snap.pitch;

        // --- Consome meshes (com orcamento) ---
        {
            int uploaded = 0;
            ChunkMeshUpdate up;
            while (uploaded < UPLOAD_BUDGET_PER_FRAME &&
                   backend_.mesh_queue().try_pop(up)) {
                chunk_renderer_.upload_mesh(up);
                ++uploaded;
            }
            ChunkUnload un;
            while (backend_.unload_queue().try_pop(un)) {
                chunk_renderer_.unload_chunk(un.pos);
            }
        }

        // --- Render ---
        int fb_w = 0, fb_h = 0;
        glfwGetFramebufferSize(win_, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);

        glClearColor(horizon.x, horizon.y, horizon.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = (fb_h > 0) ? (float)fb_w / (float)fb_h : 1.0f;
        glm::mat4 proj = cam_.projection(aspect);
        glm::mat4 view = cam_.view();

        glm::mat4 view_no_translate = glm::mat4(glm::mat3(view));
        sky_.draw(proj * view_no_translate, zenith, horizon);

        // Semana 4: fog_start/fog_end e sky_color vem do ciclo dia/noite.
        const glm::vec3 sky_intensity_color(1.0f);
        const float fog_start = 1e9f;
        const float fog_end   = 1e9f + 1.0f;

        chunk_renderer_.draw(view, proj, cam_.position,
                             snap.time_of_day,
                             sky_intensity_color,
                             fog_start, fog_end,
                             horizon,
                             cfg_.face_shading);

        if (hud_.visible) {
            char text[512];
            std::snprintf(text, sizeof(text),
                "FPS: %d\nPos: %.1f %.1f %.1f\nYaw: %.1f  Pitch: %.1f\n"
                "Chunk: %d %d %d  Loaded: %d\n"
                "Drawn: %d  (transp: %d)\n"
                "Time: %.2f  Bloco: %u\n"
                "[F3] HUD  [ESC] Sair",
                fps_value, snap.px, snap.py, snap.pz, snap.yaw, snap.pitch,
                snap.chunk_x, snap.chunk_y, snap.chunk_z, snap.chunks_loaded,
                chunk_renderer_.last_frame_drawn(),
                chunk_renderer_.last_frame_drawn_transparent(),
                snap.time_of_day, (unsigned)selected_block_);
            hud_.draw(fb_w, fb_h, text);
        }

        glfwSwapBuffers(win_);

        fps_frames++;
        fps_timer += dt;
        if (fps_timer >= 0.5) {
            fps_value = (int)(fps_frames / fps_timer);
            fps_frames = 0;
            fps_timer = 0.0;
        }
    }
}

void ClientApp::shutdown() {
    backend_.stop();
    hud_.destroy();
    chunk_renderer_.destroy();
    sky_.destroy();
    if (win_) {
        window_destroy(win_);
        win_ = nullptr;
    }
}
