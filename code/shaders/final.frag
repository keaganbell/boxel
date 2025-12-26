#version 450

//layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 frag_color;
//layout(location = 1) in vec2 tex_coord;

layout(location = 0) out vec4 final_color;

void main() {
    //vec4 sampled_color = texture(texSampler, tex_coord);
    final_color = vec4(frag_color, 1.0);
}
