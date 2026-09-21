# 渲染算法修订方案（Render Revision Plan）

> **实施状态（2026-09-20）**：M1（除 M1.3-clamp、M1.4 见决策说明）、M2、M3、M3.3、M4 与启动期 GPU API 切换已完成；D3D11/D3D12/OpenGL 通过可视化验证，Vulkan 可启动但实例渲染仍需专项修复。

> 依据 2026-09-20 渲染算法分析结论制定。目标：把当前“大坐标无限网格验证器”升级为可承载 CAD 引擎需求的渲染底座。
> 范围：main.cpp、lib/rendering/BgfxRenderer.*、lib/shaders/*、文档与验证脚本。不动 Rebase/RTE 精度内核。

## 总原则

1. 保留并冻结已验证正确的精度内核：chunk 对齐 WorldRebase、RTE 视图、CPU 双精度 slab、GL→D3D 深度转换、外向 float 扩展、网格 LOD 滞回。
2. 每个里程碑必须可回归验证：现有环境变量（GRID_STRESS_COUNT、GRID_CAMERA_TEST_PAN_X）与 verify_camera_target.py 保持可用。
3. 修改着色器后必须执行 lib\compile_shaders_all.bat 重新生成头文件；文档（README、技能参考）与代码同步修订，杜绝再次出现“文档硬规则在代码中不存在”的漂移。

## M1 正确性修复（P1/P2，1~2 天）

### M1.1 网格写深度，修复“远侧物体浮在网格上”（✅ 已完成）
- 现状：BgfxRenderer::drawGrid 状态无 BGFX_STATE_WRITE_Z，半透明对象后绘制时只对不透明体做深度测试，位于网格平面远侧的对象会混叠到网格线之上（XY/YZ/剖面最明显）。
- 修改：
  1) drawGrid 状态加 BGFX_STATE_WRITE_Z；frag.sc 中 alpha<=0.001 的片段已 discard，因此只有网格线/轴像素写深度，线间空隙仍透明。
  2) 绘制顺序保持：不透明中心立方体 → 网格 → 世界线 → 半透明对象（远到近）→ 目标点。
  3) 世界线保持不写深度（overlay 语义）。
- 验收：XY 平面 + 远侧半透明立方体场景，立方体不得覆盖网格线像素；线间空隙仍可见。

### M1.2 目标点深度偏移改为世界尺度（✅ 已完成）
- 现状：BgfxRenderer.cpp drawTargetPoint 用固定 normalizedLogDepth-0.0001f，far/near≈2e10 时等效约 0.24% 距离偏移，1e7 距离处约 2.4 万世界单位，目标点可穿透遮挡几何。
- 修改：改在视图深度域减偏移：biasWorld = max(0.5 * worldPerPixel(target), 0.01)，viewDepth' = max(logNear, viewDepth - biasWorld)，再 normalizedLogDepth(viewDepth')。需要把每帧 pixelSize（或 worldPerPixel）通过 TargetPointRenderData 传入。
- 验收：大坐标场景中目标点被几何遮挡时不再“浮现”；近景时仍可见（偏移 ≤ 半像素）。

### M1.3 正交 slab（⛔ clamp 方案取消，仅完成注释修正）

**实施决策**：原“用 immutableSceneBounds 钳制 slab”的方案在实施复核时被否决——正交相机沿视线是一个圆柱，X/Y 落在视口内、深度很远的对象是**真实可见**的，必须纳入 slab；场景总深度区间包含所有对象，钳制无收益，反而会裁掉无限网格在场景范围之外的可见部分。已完成的实际修改：
- main.cpp 注释漂移修正（128→1024；默认 1e5→1e7）。
- 正交 slab 的逐帧抖动问题由 M2 稳定器解决。
- 现状：正交按视口 X/Y 收集对象，深度方向不设界， slab 会被远处对象拉大（正交本质是沿视线圆柱，不能直接深度剔除对象，但可被“场景总深度区间”钳制）。
- 修改：每帧用 immutableSceneBounds 做一次 O(1) 相机空间 AABB 变换，得到 sceneDepthMin/Max；将 includeObjectDepth/includeSegmentCameraDepth/地面区间贡献后的 [targetDepth±slabRadius] 与 [sceneDepthMin-ε, sceneDepthMax+ε] 求交后再扩展 kMinDepthSpan。
- 顺带修正注释漂移：main.cpp:1584 “128 world units” → 实际 1024；main.cpp:199 “默认 1e5” → 实际 1e7。

### M1.4 透视世界轴常量（⏸ 暂缓，附量化依据）

实施复核发现该问题实际不可见：axis 常量误差 ≈ float ULP(originWorld·tangent)，仅当轴**在屏内**时才有意义，而轴在屏内意味着相机距世界原点在屏距量级，此时 ULP 误差远小于每像素世界尺寸（透视像素尺寸 ∝ 距离，误差 ∝ 距离的 1e-7 倍）。结论：误差始终亚像素，不需要着色器改动。保留 rebase 相对坐标方案作为未来 1e9+ 场景的加固项。
- 现状：axisOriginGridRelative = dot(-originWorld, tangent) 直接转 float，在近距离大坐标 + 自定义斜平面时量化误差可达数十世界单位。
- 修改：轴判定移到 relativePos（= uOriginRelative + p，rebase 相对、幅值有界）上：
  - GridRenderData 新增 axisOriginRebaseRelative = vec2(dot(-rebase,tangentU), dot(-rebase,tangentV))（双精度计算后转 float，幅值 ≤ chunk/2，精确）。
  - frag.sc 透视分支改用 planeCoordinates(relativePos) - uAxisOriginRebaseRelative 计算轴距离；删除 axisOriginGridRelative 大常量路径。
- 验收：相机推近到 1e7 级坐标，世界轴与立方体边缘对齐无抖动。

### M1.5 交互尺寸与健壮性小修（✅ 已完成）
- handleOrbitMouseMovement / 自动 pan 的 worldPerPixel 改用 SDL_GetWindowSizeInPixels 实时值（或缓存 render() 每帧结果），替换 SCREEN_HEIGHT 常量。
- BgfxRenderer::beginFrame 增加返回 bool（或设置 m_frameValid 标志）；FBO 重建失败时 render() 直接跳过本帧提交，避免向无效句柄提交。
- 开启 SDL_WINDOW_RESIZABLE（配合上一条），保证 resize/HiDPI 全链路一致。

## M2 深度 slab 稳定化（✅ 已完成）

### M2.1 恢复 stable/pending 迟滞（补齐文档硬规则）
新增小工具类（main.cpp 匿名命名空间）：

    struct SlabStabilizer {
        double stableNear=0, stableFar=0;   // 当前生效
        double pendingNear=0, pendingFar=0; // 待收缩候选
        int    stableFrames=0;
        void reset();
        void apply(double candNear, double candFar, double& outNear, double& outFar);
    };

规则（与技能文档一致）：
- 扩张立即生效：candidate 超出 stable → stable 扩到 candidate，pending 复位，计数清零。
- 收缩延迟：candidate 在 stable 内 → pending=candidate；pending 与 stable 差 < eps=max(1e-4, 1e-4*max(|near|,|far|)) 视为稳定并计数；连续 20 帧后 stable=pending。
- 稳定帧不得回写 stable 之外的任何抖动（防 1-ULP 远面乒乓）。

### M2.2 接入点
- 正交 [orthoNearDepth, orthoFarDepth]、透视对象 slab near/far、overlay slab near/far 全部经 SlabStabilizer 输出。
- logDepth 由稳定后的 activeNear/activeFar、overlayNear/overlayFar 派生，保证 log 映射分母不逐帧变化。
- reset 触发点：P 键切换投影、L 键切换场景、1-4 键切换网格平面。rebase 跳 chunk 不需要 reset（相机空间深度平移不变）。
- logCameraStateIfChanged 旁增加一个 GRID_DEBUG_SLAB 环境变量开关：静止 200 帧输出 distinct near/far 数量，验收标准 ≤ 2（初始扩张 + 一次收缩）。

## M3 线渲染与抗锯齿（✅ 已完成，含两处实施决策）

### M3.1 宽线 ribbon 化（✅ 已完成；AABB 线框暂缓）

- worldLine 程序改为 6 顶点 ribbon quad（(t, side) attribute），VS 在 NDC 空间按 `lineWidth` 像素垂直扩展，D3D11 宽线生效；log 深度输出保持不变（v_viewDepth 线性插值）。
- `WorldLineRenderData::lineWidth` 真正上传（新 uniform uLineWidth）；参考线、视锥 wireframe、grid 调试四边形全部走 ribbon。
- AABB 线框仍用 cube 程序 1px 线（依赖 MSAA 抗锯齿），迁移 ribbon 列入 M4 随实例化一并处理。
- lib/shaders/worldLine/{vertex.sc,varying.def.sc} 重写并重新生成头文件；blit 目录着色器不再被引用。
- 用 4 顶点 quad（attribute = (t, side)）替代 PT_LINES：
  - VS：worldLine 用 D3D 投影 * view 得 clip0/clip1；透视除 w 得 NDC，沿屏幕垂直方向 offset = widthPx*0.5 * (1/viewportWH)，输出插值 viewDepth（沿用现有 log 深度输出）。
  - FS：跨宽度 smoothstep 抗锯齿；宽度、颜色、虚线相位作为 uniform/attribute。
  - CPU 端裁剪（clipReferenceSegmentTo*）保留，端点仍有界。
- drawWorldLine 签名保留 lineWidth 并真正上传；AABB 线框（drawAabb）改用同一 ribbon 程序，删除 PT_LINES 依赖。
- 视锥 wireframe、grid 可见四边形调试线一并迁移。

### M3.2 MSAA 实际生效（✅ 已完成，采用方案 A）

- 移除场景 FBO + blit 管线（createSceneFrameBuffer/destroySceneFrameBuffer、BlitShaders、view 1 全部删除），view 0 直接渲染 backbuffer。
- bgfx::init 与 bgfx::reset 均使用 BGFX_RESET_VSYNC | BGFX_RESET_MSAA_X4。
- **深度格式权衡**：backbuffer 深度为 D24S8（原 FBO 为 D32F）。经评估可接受：log 深度下 24bit 对数分辨率约 2^(34.2/2^24)≈1.0000014；正交线性深度在 3.3e7 范围内分辨率约 0.002 世界单位，且 slab 已由 M2 稳定器消除逐帧抖动。若未来需要后处理，再按方案 B（MSAA 附件 + resolve）恢复离屏管线。
- 方案 A（推荐，改动小）：删除场景 FBO + blit 管线（当前 FBO 无后处理用途），view 0 直接渲染到 backbuffer；bgfx::reset 标志加 BGFX_RESET_MSAA_X8 | BGFX_RESET_VSYNC。MSAA/LINEAA 立即生效，endFrame 简化。
- 方案 B（保留 FBO 以备后处理）：createSceneFrameBuffer 换 MSAA 附件并在 blit 前做 resolve；仅当确需后处理时实施。
- 验收：截图放大 4x 对比线段边缘；无 Z-fighting 回归（D32F 深度保持）。

## M3.3 worldline 消失回归修复（✅ 已完成）

### 现象与结论
- 正交模式（以及检查后发现的透视模式）中，绿色 worldline 的 CPU frustum clipping 返回 visible=1，CPU NDC 端点也在视口内，但 GPU 无输出。
- 用全屏 quad、已知 NDC、关闭深度测试、关闭网格写深度分别定位后，确认有两个独立问题：
  1. ribbon VS 使用 mul(projection, vec3)。bgfx 跨 HLSL/GLSL 翻译时不能依赖隐式 w；改为标准 mul(projection, vec4(cameraPos, 1.0))。
  2. worldline 常与解析网格共面；网格开启 BGFX_STATE_WRITE_Z 后，log/linear 深度在浮点量化下发生遮挡。复用 uLineWidth.y 传递深度偏移在两个 shader stage 中不可靠。

### 修复
- worldLine VS 显式使用 vec4 投影。
- 新增专用 uDepthBias Vec4 uniform：
  - perspective：片元 viewDepth 减去 1% 相机距离；仅影响深度，不改 ribbon 几何；实体几何仍可正常遮挡。
  - orthographic：同一世界偏移换算为 normalized depth；解决共面 D24 比较 ULP 问题。
- 保持网格 BGFX_STATE_WRITE_Z 和 worldline 不写深度不变，避免回退 M1.1 的遮挡修复。

### 验证
- GRID_CAMERA_DEBUG=1：persp/ortho 均输出 [LINE] visible=1，clip/NDC 端点正确。
- 截图回归：透视与正交大坐标场景均能看到绿色 worldline；实体、网格遮挡关系不回退。

### 交互语义澄清
- P 不重建/不重置场景，只切换投影矩阵并 reset 深度 slab 稳定器，防止旧模式迟滞污染新模式首帧。
- L 也不销毁/重生成静态 stress objects；它传送相机到 1e7 测试场，并按演示设计移动中心 cube 到该场景。
## M4 规模化绘制架构（3~5 天）（✅ 已完成）

### 实施结果

1. **实例化 mesh 路径**
   - `RendererBackend` 新增 `MeshInstance` / `MeshInstancesRenderData`；mesh 实例数据为 32 字节：`i_data0 = rebase-relative position + scale`，`i_data1 = rgb + opacity`。
   - `BgfxRenderer::drawMeshInstances()` 按背景瞬时实例缓冲容量分 chunk 提交；Cube/Sphere/Cone/Torus 各成一组，静态场景最多 4 个 mesh draw call。
   - CPU 先按相机距离 far-to-near 排序，因此同一 mesh 组内保持准确的透明 painter 序；跨 mesh 组只保证组间按首对象排序，跨组互相穿插的精确混合仍是已知限制。

2. **实例化 point impostor 路径**
   - `TargetPointInstance` / `TargetPointInstancesRenderData` 使用 48 字节实例记录；CPU 预投影 center、深度、屏幕尺寸和颜色。
   - 原 tinyDraws 逐对象 point draw 改为一个批量实例路径，并按瞬时缓冲容量分 chunk。

3. **静态 BVH 宽相位剔除**
   - 为 `LargeCoordinateObject` 构建一次双精度世界 AABB BVH；中位数分割，叶子最多 4 个对象，右子节点显式保存。
   - 透视和正交每帧共用 `collect()` 做视锥宽相位剔除；叶内继续精确 AABB、screen-extent 与 slab/LOD 判定。
   - 不重建 BVH，不改静态场景数据；L 只传送相机，P 只切换投影。

### 关键修复：bgfx D3D11 实例语义

- 首轮实现把 `i_data0/1/2` 声明为 `TEXCOORD0/1/2`。CPU 分组和实例数据正确，但 D3D11 vertex input 没接到 bgfx instance stream，导致所有实例属性退化为无效数据，实体对象完全不显示。
- 对齐 bgfx instancing 示例后改为：`i_data0 : TEXCOORD7`、`i_data1 : TEXCOORD6`、`i_data2 : TEXCOORD5`；重新编译 shader 后 mesh 和 impostor 路径均恢复。
- 该语义限制已作为 D3D11/bgfx 集成硬规则记录在方案中。

### 验证

- `GRID_STRESS_COUNT=0`：8 个验证对象全部通过实例路径可见。
- `GRID_STRESS_COUNT=1000`：透视模式显示 1008 个可见对象，BVH 分组为 cube=258、sphere=250、cone=250、torus=250；透视和 ortho 均截图通过。
- `GRID_STRESS_COUNT=50000`：全场景 fit-all 视图保持约 164.6 FPS，绘制不崩溃，几何/网格/worldline 稳定。
- 重新构建命令：`lib\compile_shaders_all.bat` + `cmake --build build2022 --config Release`。

## M4.3 启动期 GPU API 切换（✅ D3D11/D3D12/OpenGL，⚠ Vulkan 实验性）

- 新增 `GraphicsApi { Auto, Direct3D11, Direct3D12, OpenGL, Vulkan }`。
- `WINDOW_RENDERER` 现支持：`bgfx`、`dx11`、`dx12`、`gl/opengl`、`vk/vulkan`。
- `BgfxRenderer` 将 API 映射到 `bgfx::Init.type`；默认 `Auto` 使用 D3D11。
- `createRenderResources()` 按 `bgfx::getRendererType()` 选择 `*_dx11`、`*_dx12`、`*_glsl` 或 `*_spv`。
- shader 批编译新增桌面 GLSL 120 数组，保证 bgfx 默认 OpenGL 2.1 context 可创建程序。
- D3D 专属 `[-1,1] -> [0,1]` 投影深度重映射只在 D3D11/D3D12 生效。
- 验证：`dx11`、`dx12`、`gl/opengl` 均显示 1000 stress objects 且 FPS 约 165；`vk` 初始化和部分几何显示正常，但实例/透明路径出现异常大三角形，标记为 Vulkan 专项修复项。

## M5 CAD 能力补齐（后续里程碑）

- 剖面模式：cube/line VS/FS 增加 uSectionPlane(normal, offset)；法向负侧 discard 或按 uSectionFade 渐隐，独立于深度序（网格作为剖切参考面）。
- 线样式：ribbon 基础上加 dash 相位、选中高亮色、深度偏移（polygon offset 语义）。
- 拾取：GPU ID buffer（离屏 R32U + instance id）或 CPU ray/AABB 双路径；与本方案 M4.2 的 BVH 共用。
- 文本/标注：bgfx debug 字体仅限调试，生产需 SDF 文本管线（另立项）。

## 文档同步

- 修订 README：渲染架构图增加“网格写深度、透明绘制顺序、MSAA”现状。
- 更新技能参考（infinite-grid-plane-camera）中已过时描述：透视掠射渐隐（现为 5° 硬裁剪）、grid 深度写入、ortho slab 最小跨度 1024、LARGE_COORDINATE_BASE_POINT=1e7、slab 迟滞恢复后的规则。
- 所有修复在 PR 描述中附“分析结论 → 修复项”映射表。

## 回归与验收清单

1. 构建：cmake --build build2022 --config Release；着色器：lib\compile_shaders_all.bat。
2. 功能：1/2/3/4 平面切换、P/L 投影与场景切换、Shift+中键旋转、中键平移、滚轮缩放，各模式目视检查。
3. 稳定性：GRID_DEBUG_SLAB=1 静止 200 帧 distinct(near,far) ≤ 2。
4. 剖面正确性：XY 平面远侧立方体不覆盖网格线；线间空隙可透视。
5. 精度：1e7 场景推近到 OrthoSize≈1e-4，世界轴与几何边缘无漂移；verify_camera_target.py 通过。
6. 性能：GRID_STRESS_COUNT=0/1000/50000 记录 FPS 与 draw call 数（bgfx 统计），M4 后 50k 场景目标 ≥60 FPS。
7. 截图回归：默认视角、大坐标视角、正交极小/极大 halfH 四组基线图。
