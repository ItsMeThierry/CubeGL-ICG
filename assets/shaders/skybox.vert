#version 330 core

layout(location = 0) in vec3 a_pos;

uniform mat4 u_view_proj;

out vec3 v_dir;

void main() {
    v_dir = a_pos;
    // z=w para ficar no plano distante.
    vec4 clip = u_view_proj * vec4(a_pos, 1.0);
    gl_Position = clip.xyww;
}