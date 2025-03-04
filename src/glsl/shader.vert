#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 0) out vec4 frag_in;

layout(set = 0, binding = 0) uniform transformation {
	mat4 model;
	mat4 view;
	mat4 projection;
} m;

void main() {
	gl_Position = m.projection * m.view * m.model * vec4(position, 1.0);
	frag_in = color;
}
