#version 330 core

in float vDensity;

uniform float alphaScale;
uniform int colorMode;
uniform float densityThreshold;

out vec4 fragColor;

// ==== 彩虹色映射 ====
vec3 rainbow(float d) {
    d = clamp(d, 0.0, 1.0);
    
    if (d < 0.25) {
        float t = d / 0.25;
        return mix(vec3(0.1, 0.0, 0.2), vec3(0.2, 0.1, 0.4), t);
    }
    else if (d < 0.28) {
        float t = (d - 0.25) / 0.03;
        return mix(vec3(0.2, 0.1, 0.4), vec3(0.28, 0.28, 0.88), t);
    }
    else if (d < 0.31) {
        float t = (d - 0.28) / 0.03;
        return mix(vec3(0.28, 0.28, 0.88), vec3(0.18, 0.48, 0.96), t);
    }
    else if (d < 0.34) {
        float t = (d - 0.31) / 0.03;
        return mix(vec3(0.18, 0.48, 0.96), vec3(0.22, 0.65, 0.98), t);
    }
    else if (d < 0.38) {
        float t = (d - 0.34) / 0.04;
        return mix(vec3(0.15, 0.6, 0.95), vec3(0.1, 0.75, 0.85), t);
    }
    else if (d < 0.42) {
        float t = (d - 0.38) / 0.04;
        return mix(vec3(0.1, 0.75, 0.85), vec3(0.2, 0.85, 0.7), t);
    }
    else if (d < 0.46) {
        float t = (d - 0.42) / 0.04;
        return mix(vec3(0.2, 0.85, 0.7), vec3(0.25, 0.9, 0.4), t);
    }
    else if (d < 0.48) {
        float t = (d - 0.46) / 0.02;
        return mix(vec3(0.25, 0.9, 0.4), vec3(0.95, 0.95, 0.1), t);  // 直接跳到黄色
    }
    else if (d < 0.52) {
        float t = (d - 0.48) / 0.04;
        return mix(vec3(0.95, 0.95, 0.1), vec3(1.0, 0.85, 0.05), t);
    }
    else if (d < 0.56) {
        float t = (d - 0.52) / 0.04;
        return mix(vec3(1.0, 0.85, 0.05), vec3(1.0, 0.7, 0.05), t);
    }
    else if (d < 0.60) {
        float t = (d - 0.56) / 0.04;
        return mix(vec3(1.0, 0.7, 0.05), vec3(1.0, 0.5, 0.05), t);
    }
    else if (d < 0.64) {
        float t = (d - 0.60) / 0.04;
        return mix(vec3(1.0, 0.5, 0.05), vec3(1.0, 0.35, 0.1), t);
    }
    else if (d < 0.68) {
        float t = (d - 0.64) / 0.04;
        return mix(vec3(1.0, 0.35, 0.1), vec3(0.95, 0.2, 0.15), t);
    }
    else if (d < 0.75) {
        float t = (d - 0.68) / 0.07;
        return mix(vec3(0.95, 0.2, 0.15), vec3(1.0, 0.5, 0.5), t);
    }
    else {
        float t = (d - 0.75) / 0.25;
        return mix(vec3(1.0, 0.5, 0.5), vec3(1.0, 1.0, 1.0), t);
    }
}

// ==== 肉色映射 ====
vec3 fleshColor(float d) {
    d = clamp(d, 0.0, 1.0);
    
    if (d < 0.20) {
        float t = d / 0.20;
        return mix(vec3(0.14, 0.10, 0.06), vec3(0.32, 0.22, 0.14), t);  // 稍微增加绿色
    }
    else if (d < 0.22) {
        float t = (d - 0.20) / 0.02;
        return mix(vec3(0.32, 0.22, 0.14), vec3(0.60, 0.28, 0.26), t);  // 减少红色，增加绿色
    }
    else if (d < 0.24) {
        float t = (d - 0.22) / 0.02;
        return mix(vec3(0.60, 0.28, 0.26), vec3(0.74, 0.36, 0.30), t);
    }
    else if (d < 0.26) {
        float t = (d - 0.24) / 0.02;
        return mix(vec3(0.74, 0.36, 0.30), vec3(0.84, 0.45, 0.35), t);
    }
    else if (d < 0.30) {
        float t = (d - 0.26) / 0.04;
        return mix(vec3(0.84, 0.45, 0.35), vec3(0.90, 0.54, 0.42), t);
    }
    else if (d < 0.35) {
        float t = (d - 0.30) / 0.05;
        return mix(vec3(0.90, 0.54, 0.42), vec3(0.94, 0.64, 0.52), t);
    }
    else if (d < 0.45) {
        float t = (d - 0.35) / 0.10;
        return mix(vec3(0.94, 0.64, 0.52), vec3(0.96, 0.76, 0.66), t);
    }
    else if (d < 0.60) {
        float t = (d - 0.45) / 0.15;
        return mix(vec3(0.96, 0.76, 0.66), vec3(0.97, 0.88, 0.80), t);
    }
    else if (d < 0.80) {
        float t = (d - 0.60) / 0.20;
        return mix(vec3(0.97, 0.88, 0.80), vec3(0.98, 0.96, 0.90), t);
    }
    else {
        float t = (d - 0.80) / 0.20;
        return mix(vec3(0.98, 0.96, 0.90), vec3(0.99, 0.98, 0.95), t);
    }
}

void main() {
    // ========== 密度阈值过滤 ==========
    if (vDensity < densityThreshold) {
        discard;
    }
    
    // 计算点坐标到中心的距离
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    
    // 高斯核函数：exp(-r^2 / (2*sigma^2))
    float gaussian = exp(-dist * dist * 10.0);
    
    // 提前剔除边缘透明像素
    if (gaussian < 0.01) {
        discard;
    }
    
    // 根据颜色模式选择颜色映射
    vec3 color;
    if (colorMode == 0) {
        color = rainbow(vDensity);
    } else if (colorMode == 1) {
        color = fleshColor(vDensity);
    } else {
        color = vec3(vDensity);
    }
    
    // 计算最终Alpha
    float alpha = vDensity * alphaScale * gaussian;
    
    // 输出最终颜色
    fragColor = vec4(color, alpha);
}