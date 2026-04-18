#version 330 core

uniform sampler3D volumeTex;
uniform vec3 sliceNormal;
uniform float sliceDistance;
uniform float alphaScale;
uniform int colorMode;  // 0=彩虹, 1=肉色, 2=灰度

in vec2 vUV;
out vec4 fragColor;

// ===== 彩虹色映射 ===== 
vec3 rainbow(float t) {
    t = clamp(t, 0.0, 1.0);
    // 蓝->青->绿->黄->橙->红
    vec3 c = vec3(0.0);
    if (t < 0.25) {
        c = mix(vec3(0.2, 0.4, 1.0), vec3(0.0, 0.8, 1.0), t * 4.0);
    } else if (t < 0.5) {
        c = mix(vec3(0.0, 0.8, 1.0), vec3(0.0, 1.0, 0.5), (t - 0.25) * 4.0);
    } else if (t < 0.75) {
        c = mix(vec3(0.0, 1.0, 0.5), vec3(1.0, 1.0, 0.0), (t - 0.5) * 4.0);
    } else {
        c = mix(vec3(1.0, 1.0, 0.0), vec3(1.0, 0.3, 0.0), (t - 0.75) * 4.0);
    }
    return c;
}

// ===== 肉色映射 ===== 
vec3 fleshColor(float t) {
    t = clamp(t, 0.0, 1.0);
    
    if (t < 0.2) {
        // 红褐 -> 深红
        return mix(vec3(0.6, 0.2, 0.15), vec3(0.8, 0.3, 0.2), t / 0.2);
    } else if (t < 0.3) {
        // 深红 -> 橙红
        return mix(vec3(0.8, 0.3, 0.2), vec3(0.9, 0.5, 0.3), (t - 0.2) / 0.1);
    } else if (t < 0.4) {
        // 橙红 -> 橙黄
        return mix(vec3(0.9, 0.5, 0.3), vec3(0.95, 0.75, 0.45), (t - 0.3) / 0.1);
    } else {
        // 橙黄 -> 米黄白(骨骼)
        return mix(vec3(0.95, 0.75, 0.45), vec3(1.0, 0.95, 0.85), (t - 0.4) / 0.6);
    }
}

void main() {
    vec3 n = normalize(sliceNormal);
    
    // 使用固定的世界坐标轴构建切线空间,避免翻转
    vec3 worldUp = vec3(0, 1, 0);
    vec3 worldRight = vec3(1, 0, 0);
    
    vec3 tangent1, tangent2;
    
    // 如果法向量接近Y轴
    if (abs(dot(n, worldUp)) > 0.99) {
        tangent1 = worldRight;
        tangent2 = normalize(cross(n, tangent1));
    } else {
        // 使用Y轴作为参考
        tangent1 = normalize(cross(worldUp, n));
        tangent2 = normalize(cross(n, tangent1));
    }
    
    vec2 localUV = vUV - vec2(0.5);
    vec3 planeCenter = vec3(0.5) + n * sliceDistance;
    vec3 samplePos = planeCenter + tangent1 * localUV.x + tangent2 * localUV.y;
    
    // 边界检查
    if (any(lessThan(samplePos, vec3(0.0))) || any(greaterThan(samplePos, vec3(1.0)))) {
        discard;
    }
    
    // 采样体数据密度
    float density = texture(volumeTex, samplePos).r;
    
    vec3 color;
    if (colorMode == 0) {
        // 彩虹模式
        color = rainbow(density);
    } else if (colorMode == 1) {
        // 肉色模式
        color = fleshColor(density);
    } else {
        // 灰度模式
        color = vec3(density);
    }
    
    fragColor = vec4(color, density * alphaScale);
}