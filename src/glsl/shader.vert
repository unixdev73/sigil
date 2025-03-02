#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 0) out vec4 frag_in;

void main() {
	gl_Position = vec4(position, 1.0);
	frag_in = color;
}
