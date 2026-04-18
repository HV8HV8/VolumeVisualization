#version 330 core

in vec3 vTexCoord;

uniform sampler3D volumeTex;
uniform float sliceDepth;
uniform float alphaScale;
uniform float densityThreshold;
uniform int colorMode;  // 0=彩虹, 1=肉色, 2=灰度
uniform int hasGradient;

out vec4 fragColor;

// ===== 彩虹色映射 ===== 
vec3 rainbowColor(float t) {
    t = clamp(t, 0.0, 1.0);
    
    if (t < 0.2) {
        return mix(vec3(0.0, 0.8, 1.0), vec3(0.0, 0.5, 1.0), t * 5.0);
    } else if (t < 0.4) {
        return mix(vec3(0.0, 0.5, 1.0), vec3(0.0, 1.0, 0.3), (t - 0.2) * 5.0);
    } else if (t < 0.6) {
        return mix(vec3(0.0, 1.0, 0.3), vec3(1.0, 1.0, 0.0), (t - 0.4) * 5.0);
    } else if (t < 0.8) {
        return mix(vec3(1.0, 1.0, 0.0), vec3(1.0, 0.5, 0.0), (t - 0.6) * 5.0);
    } else {
        return mix(vec3(1.0, 0.5, 0.0), vec3(1.0, 0.0, 0.0), (t - 0.8) * 5.0);
    }
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
    vec3 texCoord = vTexCoord;
    
    // 采样体数据密度
    float density = texture(volumeTex, texCoord).r;
    
    if (density < densityThreshold) {
        discard;
    }
    
    vec3 color;
    
    if (colorMode == 0) {
        // 彩虹模式
        color = rainbowColor(density);
    } else if (colorMode == 1) {
        // 肉色模式
        color = fleshColor(density);
    } else {
        // 灰度模式
        color = vec3(density);
    }
    
    float alpha = density * alphaScale;
    alpha = clamp(alpha, 0.0, 1.0);
    
    fragColor = vec4(color, alpha);
}