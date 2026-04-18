#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in float density;

uniform mat4 mvp;
uniform float splatSize;
uniform vec2 viewportSize;

out float vDensity;

void main() {
    // 变换到裁剪空间
    gl_Position = mvp * vec4(position, 1.0);
    vDensity = density;
    
    // 透视正确的点大小计算
    // 基本思路：点在屏幕空间的大小 = 世界空间大小 / 深度 * 屏幕高度
    float depth = gl_Position.w;
    float pointSize = (splatSize * viewportSize.y) / depth;
    
    // 限制范围防止过大或过小的点
    gl_PointSize = clamp(pointSize, 1.0, 100.0);
}