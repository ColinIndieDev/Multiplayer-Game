#version 300 es
precision mediump float;

out vec4 frag_color;

in vec2 frag_pos;
in vec2 tex_coord;

uniform sampler2D tex;
uniform vec4 input_color;

void main() {
    vec4 texture_color = texture(tex, tex_coord);
    if (texture_color.a < 0.1) {
        discard;
    }
    frag_color = input_color * texture_color;
}
