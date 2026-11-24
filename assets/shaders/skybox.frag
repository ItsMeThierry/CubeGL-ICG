#version 330 core

in vec3 v_dir;

uniform vec3 u_zenith;
uniform vec3 u_horizon;

out vec4 frag_color;

void main() {
    float t = clamp(v_dir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 c = mix(u_horizon, u_zenith, t);
    frag_color = vec4(c, 1.0);
}