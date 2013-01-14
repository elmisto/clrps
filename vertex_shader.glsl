#version 330

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 texture_uv;

uniform mat4 view;
uniform mat4 perspective;
uniform mat4 translate;
uniform mat4 scale;

out vec2 uv;

void main() {
    gl_Position = perspective * view * scale * translate * position;
    uv = texture_uv;
}