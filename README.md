Volume Visualization 体积可视化实现

## 项目概述

本项目是可视计算与交互概论课程的大作业，根据课上讲解内容，在课程提供的交互界面基础上，实现多种相关算法。

## 实现思路

### 1. 准备工作

- 数据集：采用 [The Volume Library](http://volume.open-terrain.org/) 提供的体数据

- 数据处理：使用 [V^3 package](https://sourceforge.net/projects/volren/) 的 `pvm2raw.exe`，将 `.pvm` 格式转为 `.raw` 格式，生成 `.raw.meta` 文件记录通道信息（单/双通道、梯度信息）

- 开发基础：基于课程 lab 代码框架开发

### 2. Ray-Casting 算法实现

- 流程：严格遵循 **Interpolation → 梯度估计 → 分类 → 着色 → 合成** 步骤

- 优化：GPU 端执行核心计算，CPU 端负责数据准备与参数传递；使用帧缓冲（仅交互时刷新），实现早停机制、空白区域跳过、自适应步长

- 效果优化：3种颜色选择，非线性映射增强“肉”与“骨”层次变化；传递函数颜色由密度决定，透明度由梯度和密度共同决定

### 3. Splatting 算法实现

- 先实现 SliceView，支持体数据沿指定方向和深度切片预览

- 核心：采样点云 → 视角排序 → GPU 渲染混合

### 4. Slice Compositing 算法实现

垂直于视线方向对体数据切片，采样后从后往前混合渲染

## 实现结果

1. Ray-Casting 渲染效果
(部分效果图如下)
![Ray-Casting 渲染效果](<img width="244" height="158" alt="image" src="https://github.com/user-attachments/assets/649a3f7d-0719-4053-b640-5faca5f6a57b" />
)
![Ray-Casting 渲染效果](<img width="110" height="111" alt="image" src="https://github.com/user-attachments/assets/3bd56db0-3267-477d-a55a-80a0c50840e5" />
)
![Ray-Casting 渲染效果](<img width="132" height="112" alt="image" src="https://github.com/user-attachments/assets/11231ea6-3ac3-4ac8-8bf1-0f8dec995768" />
)

3. Slice Viewer 切片预览效果
(部分效果图如下)
![Ray-Casting 渲染效果](<img width="238" height="154" alt="image" src="https://github.com/user-attachments/assets/73f9f746-7d63-495b-8f8d-2b07ca6da94c" />
)
![Ray-Casting 渲染效果](<img width="142" height="113" alt="image" src="https://github.com/user-attachments/assets/2466df32-4df5-4b58-98ed-5c05dde3b9c0" />
)
![Ray-Casting 渲染效果](<img width="145" height="111" alt="image" src="https://github.com/user-attachments/assets/2a0e34df-91ba-43bd-bd3a-7d7a0acc90c7" />
)

5. Splatting 渲染效果
(部分效果图如下)
![Ray-Casting 渲染效果](<img width="247" height="160" alt="image" src="https://github.com/user-attachments/assets/f72fc0ea-be4d-43c9-b904-f17f06121111" />
)
![Ray-Casting 渲染效果](<img width="144" height="117" alt="image" src="https://github.com/user-attachments/assets/57d41d17-7bfd-4eaa-834e-1225f93d4fb8" />
)
![Ray-Casting 渲染效果](<img width="102" height="118" alt="image" src="https://github.com/user-attachments/assets/7f816c9e-25d5-477d-ba38-1f55ec1aca64" />
)

7. Slice Compositing 渲染效果
(部分效果图如下)
![Ray-Casting 渲染效果](<img width="196" height="128" alt="image" src="https://github.com/user-attachments/assets/010574c6-fe9c-4637-9a07-069ae269d8c1" />
)
![Ray-Casting 渲染效果](<img width="108" height="111" alt="image" src="https://github.com/user-attachments/assets/9c041740-f36d-4846-8ad6-9bbcc231af4d" />
)
![Ray-Casting 渲染效果](<img width="103" height="111" alt="image" src="https://github.com/user-attachments/assets/2f816444-41ac-4fdb-a598-2bc3f5691f54" />
)

> 注：仅展示了特定参数下部分数据集效果
