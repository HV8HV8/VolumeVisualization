#version 330 core

in vec2 fragUV;
out vec4 fragColor;

uniform sampler2D cachedTex;

void main() {
    fragColor = texture(cachedTex, fragUV);
}