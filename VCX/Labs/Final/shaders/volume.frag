#version 330 core

in vec2 fragUV;
out vec4 fragColor;

uniform sampler3D volumeTex;      // 密度纹理
uniform sampler3D gradientTex;    // 梯度纹理(如果有)
uniform vec3 volumeSize;
uniform int hasGradientChannel;

// 相机参数
uniform float azimuth;
uniform float elevation;
uniform float roll;
uniform float distance;

// 渲染参数
uniform float stepSize;
uniform int maxSteps;
uniform float alphaScale;

// 光照参数
uniform vec3 lightDir;
uniform float ambient;
uniform float diffuse;

// 高级控制参数
uniform float softTissueAlpha;
uniform float boneAlpha;
uniform float gradientThreshold;
uniform int colorMode;  // 0=Rainbow, 1=Flesh, 2=Grayscale

// ==================== 2D Transfer Function ====================
// 基于 Kniss et al. 2002 + VIEWER-5.2

vec4 Transfer2D(float density, float gradient) {
    float d = clamp(density, 0.0, 1.0);
    float g = clamp(gradient, 0.0, 1.0);
    
    // 梯度增强
    g = g * 12.0;
    g = clamp(g, 0.0, 1.0);
    
    // 1. 背景过滤
    if (d < 0.01) {
        return vec4(0.0);
    }
    
    // 2. Emission - 颜色映射（彩虹/肉色/灰度三模式）
    vec3 emission;
    
    if (colorMode == 0) {
        // ===== 彩虹色映射 =====
        if (d < 0.25) {
            //深紫(提亮)
            float t = d / 0.25;
            emission = mix(
                vec3(0.1, 0.0, 0.2),  // 深紫
                vec3(0.2, 0.1, 0.4),  // 紫色
                t
            );
        }
        else if (d < 0.28) {
            //紫→蓝紫
            float t = (d - 0.25) / 0.03;
            emission = mix(
                vec3(0.2, 0.1, 0.4),  // 紫色
                vec3(0.28, 0.28, 0.88),  // 蓝紫
                t
            );
        }
        else if (d < 0.31) {
            //蓝紫→深蓝
            float t = (d - 0.28) / 0.03;
            emission = mix(
                vec3(0.28, 0.28, 0.88),  // 蓝紫(提亮)
                vec3(0.18, 0.48, 0.96),  // 深蓝(提亮)
                t
            );
        }
        else if (d < 0.34) {
            //深蓝→亮蓝
            float t = (d - 0.31) / 0.03;
            emission = mix(
                vec3(0.18, 0.48, 0.96),  // 深蓝(提亮)
                vec3(0.22, 0.65, 0.98),  // 亮蓝(稍微提亮)
                t
            );
        }
        else if (d < 0.38) {
            //亮蓝→青色
            float t = (d - 0.34) / 0.04;
            emission = mix(
                vec3(0.15, 0.6, 0.95),   // 亮蓝
                vec3(0.1, 0.75, 0.85),   // 青色
                t
            );
        }
        else if (d < 0.42) {
            //青色→青绿
            float t = (d - 0.38) / 0.04;
            emission = mix(
                vec3(0.1, 0.75, 0.85),   // 青色
                vec3(0.2, 0.85, 0.7),    // 青绿
                t
            );
        }
        else if (d < 0.46) {
            //青绿→绿色
            float t = (d - 0.42) / 0.04;
            emission = mix(
                vec3(0.2, 0.85, 0.7),    // 青绿
                vec3(0.25, 0.9, 0.4),    // 绿色
                t
            );
        }
        else if (d < 0.50) {
            //绿色→亮绿
            float t = (d - 0.46) / 0.04;
            emission = mix(
                vec3(0.25, 0.9, 0.4),    // 绿色
                vec3(0.5, 0.95, 0.3),    // 亮绿
                t
            );
        }
        else if (d < 0.54) {
            //亮绿→黄绿
            float t = (d - 0.50) / 0.04;
            emission = mix(
                vec3(0.5, 0.95, 0.3),    // 亮绿
                vec3(0.75, 0.95, 0.2),   // 黄绿
                t
            );
        }
        else if (d < 0.58) {
            //黄绿→柠檬黄
            float t = (d - 0.54) / 0.04;
            emission = mix(
                vec3(0.75, 0.95, 0.2),   // 黄绿
                vec3(0.95, 0.95, 0.1),   // 柠檬黄
                t
            );
        }
        else if (d < 0.62) {
            //柠檬黄→金黄
            float t = (d - 0.58) / 0.04;
            emission = mix(
                vec3(0.95, 0.95, 0.1),   // 柠檬黄
                vec3(1.0, 0.85, 0.05),   // 金黄
                t
            );
        }
        else if (d < 0.66) {
            //金黄→橙黄
            float t = (d - 0.62) / 0.04;
            emission = mix(
                vec3(1.0, 0.85, 0.05),   // 金黄
                vec3(1.0, 0.7, 0.05),    // 橙黄
                t
            );
        }
        else if (d < 0.70) {
            //橙黄→橙色
            float t = (d - 0.66) / 0.04;
            emission = mix(
                vec3(1.0, 0.7, 0.05),    // 橙黄
                vec3(1.0, 0.5, 0.05),    // 橙色
                t
            );
        }
        else if (d < 0.74) {
            //橙色→橙红
            float t = (d - 0.70) / 0.04;
            emission = mix(
                vec3(1.0, 0.5, 0.05),    // 橙色
                vec3(1.0, 0.35, 0.1),    // 橙红
                t
            );
        }
        else if (d < 0.78) {
            //橙红→红色
            float t = (d - 0.74) / 0.04;
            emission = mix(
                vec3(1.0, 0.35, 0.1),    // 橙红
                vec3(0.95, 0.2, 0.15),   // 红色
                t
            );
        }
        else if (d < 0.85) {
            //红色→粉红
            float t = (d - 0.78) / 0.07;
            emission = mix(
                vec3(0.95, 0.2, 0.15),   // 红色
                vec3(1.0, 0.5, 0.5),     // 粉红
                t
            );
        }
        else {
            //粉红→白
            float t = (d - 0.85) / 0.15;
            emission = mix(
                vec3(1.0, 0.5, 0.5),     // 粉红
                vec3(1.0, 1.0, 1.0),     // 纯白
                t
            );
        }
    }
    else if (colorMode == 1) {
        // ===== 肉色映射 =====
        if (d < 0.20) {
            //深棕黑到深棕
            float t = d / 0.20;
            emission = mix(
                vec3(0.12, 0.08, 0.05),   // 深棕黑
                vec3(0.30, 0.20, 0.12),   // 深棕
                t
            );
        }
        else if (d < 0.22) {
            //深棕→棕红
            float t = (d - 0.20) / 0.02;
            emission = mix(
                vec3(0.30, 0.20, 0.12),   // 深棕
                vec3(0.62, 0.24, 0.28),   // 棕红
                t
            );
        }
        else if (d < 0.24) {
            //暗红→深红
            float t = (d - 0.25) / 0.05;
            emission = mix(
                vec3(0.62, 0.24, 0.28),   // 暗红
                vec3(0.76, 0.32, 0.32),   // 深红
                t
            );
        }
        else if (d < 0.26) {
            //深红→鲜红
            float t = (d - 0.30) / 0.01;
            emission = mix(
                vec3(0.76, 0.32, 0.32),   // 深红
                vec3(0.86, 0.42, 0.36),   // 鲜红
                t
            );
        }
        else if (d < 0.28) {
            //鲜红→橙红
            float t = (d - 0.31) / 0.02;
            emission = mix(
                vec3(0.86, 0.42, 0.36),   // 鲜红
                vec3(0.92, 0.58, 0.42),   // 橙红
                t
            );
        }
        else if (d < 0.30) {
            //橙红→亮橙
            float t = (d - 0.33) / 0.02;
            emission = mix(
                vec3(0.92, 0.58, 0.42),   // 橙红
                vec3(0.94, 0.70, 0.48),   // 亮橙
                t
            );
        }
        else if (d < 0.32) {
            //亮橙→金橙
            float t = (d - 0.35) / 0.02;
            emission = mix(
                vec3(0.94, 0.70, 0.48),   // 亮橙
                vec3(0.90, 0.76, 0.56),   // 金橙
                t
            );
        }
        else if (d < 0.34) {
            //金橙到棕褐
            float t = (d - 0.37) / 0.13;
            emission = mix(
                vec3(0.90, 0.76, 0.56),   // 金橙
                vec3(0.85, 0.74, 0.60),   // 棕褐
                t
            );
        }
        else if (d < 0.55) {
            //棕褐到米棕
            float t = (d - 0.50) / 0.10;
            emission = mix(
                vec3(0.85, 0.74, 0.60),   // 棕褐
                vec3(0.91, 0.82, 0.68),   // 米棕
                t
            );
        }
        else if (d < 0.58) {
            // 急变: 米棕→浅棕黄
            float t = (d - 0.55) / 0.03;
            emission = mix(
                vec3(0.91, 0.82, 0.68),   // 米棕
                vec3(0.94, 0.86, 0.70),   // 浅棕黄
                t
            );
        }
        else if (d < 0.61) {
            //浅棕黄→金黄
            float t = (d - 0.58) / 0.03;
            emission = mix(
                vec3(0.94, 0.86, 0.70),   // 浅棕黄
                vec3(0.96, 0.90, 0.72),   // 金黄
                t
            );
        }
        else if (d < 0.64) {
            //金黄→浅金
            float t = (d - 0.61) / 0.03;
            emission = mix(
                vec3(0.96, 0.90, 0.72),   // 金黄
                vec3(0.97, 0.92, 0.76),   // 浅金
                t
            );
        }
        else if (d < 0.68) {
            //浅金→橙黄
            float t = (d - 0.64) / 0.04;
            emission = mix(
                vec3(0.97, 0.92, 0.76),   // 浅金
                vec3(0.98, 0.94, 0.78),   // 橙黄
                t
            );
        }
        else if (d < 0.72) {
            //橙黄→黄白
            float t = (d - 0.68) / 0.04;
            emission = mix(
                vec3(0.98, 0.94, 0.78),   // 橙黄
                vec3(0.98, 0.95, 0.82),   // 黄白
                t
            );
        }
        else if (d < 0.76) {
            //黄白→米白
            float t = (d - 0.72) / 0.04;
            emission = mix(
                vec3(0.98, 0.95, 0.82),   // 黄白
                vec3(0.98, 0.96, 0.86),   // 米白
                t
            );
        }
        else if (d < 0.80) {
            //米白→浅米白
            float t = (d - 0.76) / 0.04;
            emission = mix(
                vec3(0.98, 0.96, 0.86),   // 米白
                vec3(0.99, 0.97, 0.90),   // 浅米白
                t
            );
        }
        else if (d < 0.90) {
            //浅米白到象牙白
            float t = (d - 0.80) / 0.10;
            emission = mix(
                vec3(0.99, 0.97, 0.90),   // 浅米白
                vec3(0.995, 0.985, 0.96), // 象牙白
                t
            );
        }
    }
    else {
        // ===== 灰度映射 =====
        emission = vec3(d);
    }
    
    // 3. Absorption - 分段策略 + UI控制
    float base_absorption = d;
    float gradient_factor;
    float absorption;
    
    if (d < 0.3) {
        // 极低密度
        gradient_factor = smoothstep(0.5 * gradientThreshold, 0.95, g);
        absorption = base_absorption * (0.01 * softTissueAlpha + gradient_factor * 0.15 * softTissueAlpha);
    }
    else if (d < 0.5) {
        // 低密度软组织
        gradient_factor = smoothstep(0.35 * gradientThreshold, 0.85, g);
        absorption = base_absorption * (0.05 * softTissueAlpha + gradient_factor * 0.35 * softTissueAlpha);
    }
    else if (d < 0.7) {
        // 中等密度
        gradient_factor = smoothstep(0.2 * gradientThreshold, 0.7, g);
        float blendFactor = (d - 0.5) / 0.2;  // 0.5-0.7之间的插值
        float softPart = (0.25 + gradient_factor * 0.45) * softTissueAlpha;
        float bonePart = (0.5 + gradient_factor * 0.5) * boneAlpha;
        absorption = base_absorption * mix(softPart, bonePart, blendFactor);
    }
    else {
        // 高密度骨骼
        gradient_factor = smoothstep(0.1 * gradientThreshold, 0.5, g);
        absorption = base_absorption * (0.5 * boneAlpha + gradient_factor * 0.5 * boneAlpha);
    }
    
    // 4. 转换为RGBA
    vec3 color = emission;
    float alpha = pow(absorption, 1.0) * alphaScale;
    alpha = clamp(alpha, 0.0, 1.0);
    
    return vec4(color, alpha);
}

// ==================== 辅助函数 ====================

float SampleDensity(vec3 pos) {
    return texture(volumeTex, pos).r;
}

float SampleGradient(vec3 pos) {
    return texture(gradientTex, pos).r;
}

vec3 EstimateGradient(vec3 pos, vec3 invSize) {
    float h = 1.0 / max(max(volumeSize.x, volumeSize.y), volumeSize.z);
    
    vec3 grad;
    grad.x = SampleDensity(pos + vec3(h, 0, 0)) - SampleDensity(pos - vec3(h, 0, 0));
    grad.y = SampleDensity(pos + vec3(0, h, 0)) - SampleDensity(pos - vec3(0, h, 0));
    grad.z = SampleDensity(pos + vec3(0, 0, h)) - SampleDensity(pos - vec3(0, 0, h));
    
    return grad / (2.0 * h);
}

bool RayBoxIntersect(vec3 orig, vec3 dir, out float tEnter, out float tExit) {
    vec3 invDir = 1.0 / dir;
    vec3 t0 = (vec3(0) - orig) * invDir;
    vec3 t1 = (vec3(1) - orig) * invDir;
    
    vec3 tmin = min(t0, t1);
    vec3 tmax = max(t0, t1);
    
    tEnter = max(max(tmin.x, tmin.y), tmin.z);
    tExit = min(min(tmax.x, tmax.y), tmax.z);
    
    return tExit >= tEnter && tExit >= 0.0;
}

vec3 ApplyLighting(vec3 color, vec3 normal, vec3 viewDir) {
    vec3 N = normalize(normal);
    vec3 L = normalize(lightDir);
    vec3 V = normalize(viewDir);
    
    vec3 ambientColor = color * ambient;
    
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuseColor = color * diffuse * NdotL;
    
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), 32.0);
    vec3 specularColor = vec3(0.3) * spec;
    
    return ambientColor + diffuseColor + specularColor;
}

// ==================== 主函数 ====================

void main() {
    vec2 ndc = fragUV * 2.0 - 1.0;
    
    vec3 center = vec3(0.5);
    
    vec3 cameraOffset = vec3(
        cos(elevation) * sin(azimuth),
        sin(elevation),
        cos(elevation) * cos(azimuth)
    ) * distance;
    
    vec3 cameraPos = center + cameraOffset;
    
    vec3 forward = normalize(center - cameraPos);
    vec3 worldUp = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(forward, worldUp));
    vec3 up = cross(right, forward);
    
    if (abs(roll) > 0.001) {
        float cosR = cos(roll);
        float sinR = sin(roll);
        vec3 newRight = right * cosR + up * sinR;
        vec3 newUp = -right * sinR + up * cosR;
        right = newRight;
        up = newUp;
    }
    
    float fov = 1.2;
    vec3 rayDir = normalize(forward + ndc.x * right * fov + ndc.y * up * fov);
    vec3 rayOrigin = cameraPos;
    
    float tEnter, tExit;
    if (!RayBoxIntersect(rayOrigin, rayDir, tEnter, tExit)) {
        discard;
    }
    
    float t = max(tEnter, 0.0);
    vec3 invSize = 1.0 / volumeSize;
    float normStep = stepSize * max(max(invSize.x, invSize.y), invSize.z);
    
    vec4 accumColor = vec4(0.0);
    int steps = 0;
    
    while (t < tExit && steps < maxSteps && accumColor.a < 0.99) {
        vec3 pos = rayOrigin + t * rayDir;
        
        float density = SampleDensity(pos);
        
        float gradient;
        vec3 gradVec;
        
        if (hasGradientChannel != 0) {
            gradient = SampleGradient(pos);
            gradVec = EstimateGradient(pos, invSize);
        } else {
            gradVec = EstimateGradient(pos, invSize);
            gradient = length(gradVec);
        }
        
        vec4 sample = Transfer2D(density, gradient);
        
        if (sample.a > 0.01 && length(gradVec) > 0.01) {
            vec3 normal = -normalize(gradVec);
            vec3 viewDir = -rayDir;
            sample.rgb = ApplyLighting(sample.rgb, normal, viewDir);
        }
        
        sample.rgb *= sample.a;
        accumColor.rgb += (1.0 - accumColor.a) * sample.rgb;
        accumColor.a += (1.0 - accumColor.a) * sample.a;
        
        t += normStep;
        steps++;
    }
    
    fragColor = vec4(accumColor.rgb, 1.0);
}