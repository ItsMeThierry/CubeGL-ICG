#version 330 core

layout(location = 0) in ivec3 a_pos;        // 0..32 (local no chunk)
layout(location = 1) in int   a_face_uv;    // face + u + v empacotados
layout(location = 2) in int   a_layer;      // camada do texture array
layout(location = 3) in int   a_light;      // RGB + ceu empacotados

uniform mat4 u_model;   // translate(chunk_origem - cam_pos)
uniform mat4 u_view;    // view_rot (sem translacao)
uniform mat4 u_proj;

out vec2  v_uv;
out float v_layer;
out vec3  v_light_rgb;
out float v_light_sky;
out vec3  v_world_rel;

// Indice da face (0..5) para o sombreamento por direcao no fragment.
// flat: nao interpolar (todos os vertices da face tem o mesmo valor).
flat out int v_face;

void main() {
    int face = (a_face_uv >> 4) & 0x7;
    int u    =  a_face_uv       & 0x3;
    int v    = (a_face_uv >> 2) & 0x3;

    v_uv    = vec2(float(u), float(v));
    v_layer = float(a_layer);
    v_face  = face;

    float r   = float( a_light        & 0xF) / 15.0;
    float g   = float((a_light >> 4)  & 0xF) / 15.0;
    float b   = float((a_light >> 8)  & 0xF) / 15.0;
    float sky = float((a_light >> 12) & 0xF) / 15.0;
    v_light_rgb = vec3(r, g, b);
    v_light_sky = sky;

    vec4 local = vec4(vec3(a_pos), 1.0);
    vec4 rel   = u_model * local;
    v_world_rel = rel.xyz;

    gl_Position = u_proj * u_view * rel;
}