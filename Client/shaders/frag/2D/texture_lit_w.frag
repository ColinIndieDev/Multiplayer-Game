#version 300 es
precision mediump float;

out vec4 frag_color;

in vec2 frag_pos;
in vec2 tex_coord;

uniform sampler2D tex;
uniform vec4 input_color;
uniform float ambient;

struct point_light {
    vec2 pos;
    float r;
    float intensity;
    vec4 color;
};

struct global_light {
    float intensity;
    vec4 color;
};

#define MAX_POINT_LIGHTS 32

uniform int point_lights_cnt;
uniform point_light point_lights[MAX_POINT_LIGHTS];
uniform global_light g_light;

vec3 calc_point_light(point_light l, vec2 f_pos, float a) {
    float dist = length(l.pos - f_pos);
    float falloff = clamp(1.0 - dist / l.r, 0.0, 1.0);

    vec3 light_color = l.color.rgb * l.intensity;

    falloff = falloff * falloff;

    return (a + falloff) * light_color;
}

vec3 calc_global_light(global_light l) {
    vec3 light_color = l.color.rgb * l.intensity;
    return light_color;
}

void main() {
    vec3 result = vec3(ambient);

    for (int i = 0; i < point_lights_cnt; i++) {
        result += calc_point_light(point_lights[i], frag_pos, ambient);
    }
    result += calc_global_light(g_light);

    vec4 texture_color = texture(tex, tex_coord);
    if (texture_color.a < 0.1) {
        discard;
    }
    vec3 obj_color = input_color.rgb * texture_color.rgb;
    vec3 out_color = obj_color * result;

    frag_color = vec4(out_color, input_color.a * texture_color.a);
}
