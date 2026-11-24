#version 330 core

layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec4 a_color;

uniform vec2 u_screen;

out vec4 v_color;

void main() {
    // stb_easy_font produz coordenadas em pixels com Y para baixo.
    vec2 ndc = vec2((a_pos.x / u_screen.x) * 2.0 - 1.0,
                    1.0 - (a_pos.y / u_screen.y) * 2.0);
    v_color = a_color;
    gl_Position = vec4(ndc, 0.0, 1.0);
}