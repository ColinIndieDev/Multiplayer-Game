#version 300 es
precision mediump float;

out vec4 frag_color;

in vec2 tex_coord;

uniform sampler2D hdr_buffer;
uniform bool gamma_correct;
uniform float exposure;

void main() {             
    const float gamma = 2.2;
    vec3 hdr_color = texture(hdr_buffer, tex_coord).rgb;
    if (gamma_correct) {
        vec3 result = vec3(1.0) - exp(-hdr_color * exposure);   
        result = pow(result, vec3(1.0 / gamma));
        frag_color = vec4(result, 1.0);
    }
    else {
        vec3 result = vec3(1.0) - exp(-hdr_color * exposure);
        frag_color = vec4(result, 1.0);
    }
}
