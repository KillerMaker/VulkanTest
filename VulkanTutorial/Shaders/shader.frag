#version 450
layout(location = 0) out vec4 outColour; 
layout(location = 0) in vec3 vertexColor;

void main() {
	outColour = vec4(vertexColor, 1.0);
}