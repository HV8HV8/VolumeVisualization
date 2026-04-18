#version 330 core

layout(location = 0) in vec3 position;

uniform mat4 mvp;
uniform mat4 model;  // 模型矩阵,用于计算世界坐标

out vec3 vTexCoord;

void main() {
    // 将顶点变换到世界空间
    vec4 worldPos = model * vec4(position, 1.0);
    
    // 世界坐标 [-0.5, 0.5] 转换为纹理坐标 [0, 1]
    vTexCoord = worldPos.xyz + vec3(0.5);
    
    gl_Position = mvp * vec4(position, 1.0);
}