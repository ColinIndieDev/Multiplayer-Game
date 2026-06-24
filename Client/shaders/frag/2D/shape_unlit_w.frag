#version 300 es
precision mediump float;

out vec4 frag_color;

in vec2 frag_pos;

uniform vec4 input_color;

void main() {
    frag_color = input_color;
}
