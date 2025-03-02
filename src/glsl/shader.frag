#version 460

layout(location = 0) in vec4 color;
layout(location = 0) out vec4 frag_out;

void main() { frag_out = color; }
