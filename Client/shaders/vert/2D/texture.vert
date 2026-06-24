#version 330 core
layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec2 a_tex_coord;

out vec2 tex_coord;
out vec2 frag_pos;

uniform mat4 projection;
uniform mat4 transform;

void main() {
    vec4 world_pos = transform * vec4(a_pos, 1.0);
    frag_pos = world_pos.xy;
    tex_coord = vec2(a_tex_coord.x, a_tex_coord.y);
    gl_Position = projection * world_pos;
}
