#version 330 core

in vec2  v_uv;
in float v_layer;
flat in int v_face;
in vec3  v_light_rgb;
in float v_light_sky;
in vec3  v_world_rel;

uniform sampler2DArray u_atlas;

uniform float u_time_of_day;
uniform vec3  u_sky_color;

uniform float u_fog_start;
uniform float u_fog_end;
uniform vec3  u_fog_color;

// Ordem de face_index: 0=topo, 1=base, 2=leste, 3=oeste, 4=sul, 5=norte
uniform float u_face_shading[6];

out vec4 frag_color;

void main() {
    vec4 tex = texture(u_atlas, vec3(v_uv, v_layer));

    vec3 light_blocks = v_light_rgb;
    vec3 light_sky    = v_light_sky * u_sky_color;
    vec3 light_final  = light_blocks + light_sky;
    light_final = max(light_final, vec3(0.25));

    float face_factor = u_face_shading[v_face];

    vec3 color = tex.rgb * light_final * face_factor;

    float dist = length(v_world_rel);
    float fog = clamp((dist - u_fog_start) / (u_fog_end - u_fog_start),
                      0.0, 1.0);
    color = mix(color, u_fog_color, fog);

    frag_color = vec4(color, tex.a);
}