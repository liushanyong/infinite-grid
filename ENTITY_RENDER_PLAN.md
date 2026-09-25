# 实体渲染双轨计划（Entity Render Plan）

> 制定日期：2026-09-24。目标：把现有"大坐标无限网格验证器"的实体渲染明确分为
> CAD 档与真实感档两轨。Mesh 类实体支持双模式（默认 CAD，按需真实感）；其余实体
> 一律走 CAD 档。CAD 档支持简单贴图（无高光/反射要求）。两档共享同一深度、精度
> 与排序体系，可任意混排。
>
> 本计划与 RENDER_REVISION_PLAN.md（已完成 M1~M4）衔接，不改动 Rebase/RTE 精度
> 内核与已冻结的深度/排序架构。

## 总原则

1. 只有 Mesh 类实体有双渲染模式；默认 CAD 档，真实感档仅在特殊要求时启用
   （全局开关 + 逐实体覆盖）。
2. CAD 档 = 无光照平面色（保留固定头灯明暗，可开关）+ 简单 albedo 贴图。
   真实感档 = PBR（metallic/roughness）+ 光源 + 透明度。
3. 两档共用：bgfx 视图通道（DepthPrepass/SolidFill/Edges/Wire/Overlay）、
   log 深度、rebase/RTE 双精度路径、BVH 剔除、layer 合成、far-to-near 透明排序。
4. 每个里程碑必须可通过现有环境变量与截图回归验证。
5. 修改着色器后必须执行 lib\compile_shaders_all.bat 重新生成头文件。

## 命名规则（对齐 OpenCADStudio）

实体类型名与文件名参照 external/OpenCADStudio-main/src/entities（其类型来自
acadrust crate，渲染输出为 RenderObject::*）。本项目为 C++，规则如下：

| OpenCADStudio 实体 | 本项目类型名 | 文件名 | 渲染输出映射 |
| --- | --- | --- | --- |
| line.rs / Line | Line | line.h | PolylineRenderData |
| arc.rs / Arc | Arc | arc.h | PolylineRenderData |
| circle.rs / Circle | Circle | circle.h | PolylineRenderData |
| ellipse.rs / Ellipse | Ellipse | ellipse.h | PolylineRenderData |
| polyline.rs / Polyline | Polyline | polyline.h | PolylineRenderData |
| lwpolyline.rs / LwPolyline | LwPolyline | lwpolyline.h | PolylineRenderData |
| spline.rs / Spline | Spline | spline.h | PolylineRenderData |
| hatch.rs / Hatch | Hatch | hatch.h | FilledTrianglesRenderData |
| solid.rs / Solid | Solid | solid.h | FilledTrianglesRenderData |
| solid3d.rs / Solid3d | Solid3d | solid3d.h | MeshInstance（CAD 档） |
| mesh.rs / Mesh | Mesh | mesh.h | MeshInstance（双档） |
| point.rs / Point | Point | point.h | TargetPointInstance |
| ray.rs / Ray | Ray | ray.h | PolylineRenderData |
| mline.rs / MLine | MLine | mline.h | PolylineRenderData + FilledTriangles |
| text.rs / mtext.rs | Text / MText | text.h / mtext.h | 后续 SDF 文字管线（M6） |
| dimension/leader/tolerance/table/insert | 同名 | 同名小写 | 后续里程碑（M6） |
| light.rs / Light | Light | light.h | 真实感档光源定义 |

- 命名空间统一为 `entities`；公共字段（图层/颜色/线宽/可见性）放
  `entities::EntityCommon`，对应 OpenCADStudio 的 `common` 字段。
- 渲染类别枚举：`entities::RenderClass { Cad, Realistic }`；材质结构体
  `entities::PbrMaterial` 字段对齐 VulkanSceneGraph
  include/vsg/state/material.h 的 PbrMaterial（baseColorFactor、emissiveFactor、
  metallicFactor、roughnessFactor、alphaMask、alphaMaskCutoff）。
- 贴图槽位预留 VSG TexCoordIndices 分类（diffuse/normal/ao/emissive/mr），
  首期只实现 diffuse。
- 本命名规则为"暂时对齐"：后续如引入 STEP/glTF 导入，字段语义逐步向
  acadrust 的字段补齐，但类型名不回退改名。

## 执行状态（2026-09-25）

M1~M5 已完成，M6 仍保持为独立后续工作。当前落地结果：

1. M1：`lib/entities` 已建立 OpenCADStudio 对齐的实体命名与公共模型。
2. M2：Mesh CAD 档已支持 UV、diffuse 贴图、实例色 tint 和可开关头灯。
3. M3：`GRID_REALISTIC=1` 可启用 PBR / 光源 / 透明度路径，并与 CAD 档共存。
4. M4：`lib/entities/tessellate.h` 提供 CAD 细分桥，`GRID_CAD_DEMO=1` 可验证。
5. M5：透明 Mesh 已改为全局深度桶排序，CAD 与 PBR 混排在统一 far-to-near
   序列内提交。
6. 硬编码向量图元 Demo 已迁移为正式 `entities::Line/Arc/Circle/Ellipse/
   Polyline/LwPolyline/Spline/Hatch/Solid/Ray/MLine/Point/Mesh` 数据。
   `drawVectorPrimitivesDemo()` 与 CPU 拾取共享同一份不可变细分缓存；点实例
   支持实体级 point size / line weight。
7. 剩余硬编码 mesh 已收口为 `entities::Mesh` 记录：大坐标验证体、stress
   mesh、CenterCube、RedDebugCube 和四个 Demo mesh 的名称/颜色/透明度/
   CAD-Realistic 样式均以实体数据为准。渲染批次按 shape + style + material +
   opacity 分组，`EntityCommon::visible` 同时约束绘制与 CPU 拾取。
   相机焦点红点（`drawTargetPoint()`）是相机/视口覆盖层，保持非实体路径；
   大坐标验证体与 stress mesh 的实体 alpha 保留原实例路径的 `0.45`。

本轮回归：Release 构建通过；D3D11 CAD 档与真实感混合档（1000 实体）运行无
`Invalid program`；`GRID_DEBUG_SLAB=1` 静止相机只记录 1 次深度区间初始化；
新增 `verify_camera_target.py` 固化相机平移回归且通过。Vulkan 仍按实验项
处理，主线验证以 D3D11 为准。

向量图元迁移回归：Debug 与 Release 构建通过；D3D11 下
`verify_camera_target.py Debug/Release 1` 均通过；D3D11 启动运行确认 CAD
细分缓存为 `31 strokes / 1210 fills / 30 points`，`GRID_PICK_DEBUG=1` 可进入
CPU 拾取路径且无 Spline/vector 越界断言。

硬编码 mesh 迁移收口（2026-09-26）：Debug 与 Release 构建通过；
D3D11 下 `verify_camera_target.py Debug/Release 1` 与
`verify_ortho_convergence.py Debug/Release` 均通过。

统一可见性机制（2026-09-26）：正交与透视现在共享
`UnifiedVisibilityQuery` 的 CPU 三态判定（`Offscreen/Tiny/Visible`）。
候选覆盖大坐标验证体、stress mesh、CenterCube、CAD mesh、CAD stroke、
CAD fill 和 CAD point；渲染剔除与 CPU 拾取使用同一份候选和同一分类结果。
正交档保持 CAD 图元按原比例显示，不替换 tiny impostor；透视档保留 tiny
点代理。因此 CAD 图元、mesh、CenterCube 在两种投影下的可见性与可拾取性
不再分别维护。

清理后最终回归：Debug 与 Release 构建通过；D3D11 下
`verify_camera_target.py Debug/Release 1` 与
`verify_ortho_convergence.py Debug/Release` 均通过。正交收敛结果保持在
`near=-2033.000366, far=2063.000732`。

## M1 实体模型地基（0.5~1 天）

- 新建 lib/entities/ 目录，按上表建立实体头文件与 `EntityCommon`；
  现有 main.cpp 的 LargeCoordinateObject 保持不动，作为演示数据源。
- 定义 `RenderClass`、`PbrMaterial`、`MeshStyle { Cad, Realistic }`。
  MeshInstance 暂不改布局，仅在渲染数据层预留。
- 验收：编译通过，渲染行为零变化（截图与当前基线一致）。

## M2 Mesh CAD 档贴图（2~3 天）

1. 四个程序化 mesh 顶点加 UV：cube 面平面投影、sphere 球面、cone 圆柱、
   torus 参数曲面；顶点格式 pos+normal+uv。导入网格兜底用 triplanar
   （shader 内映射，不依赖顶点 UV）。
2. 贴图加载：用 lib/bimg 解析 PNG/JPG，bgfx::createTexture2D 上传；
   RendererBackend 新增 loadMeshTexture(path)，draw call 级绑定
   （同一 mesh 形状共享一张 diffuse 贴图，实例颜色做 tint）。
3. CAD 实例着色器（centerAnchorInstance/fs）增加 s_albedo 采样：
   最终色 = 贴图 × 实例色 × 头灯（头灯强度可关，0 = 纯平涂）。
   alpha 通道参与现有不透明度；RenderMode 六种视觉样式对贴图无感知。
4. 环境变量 GRID_MESH_TEXTURE=<path> 演示加载；默认 1x1 白纹理。
- 验收：GRID_STRESS_COUNT=0/1000 下贴图 cube/sphere/cone/torus 在
  Wireframe2D/Shaded/ShadedWithEdges 下截图正确；透明实例排序不回退。

## M3 真实感档按需启用（2~3 天）

1. 把 lib/shaders/cadAlgorithm/fs_cad_styles.sc 的 PBR 分支
   （GGX+Smith+Schlick）拆分为独立 pbr 实例片元着色器，复用现有
   实例顶点着色器与 4 组 mesh 顶点缓冲。
2. 光源 uniform 块（对齐 VSG 灯型，首期 Ambient + Directional + Point；
   Spot 与阴影后续）：u_ambient、u_dirLight(dir,color)、u_pointLights
   (pos,color,radius)×N（N 由 uniform 上限定，初期 4）。
3. MeshInstance 扩展 i_data2 = (metallic, roughness, emissive, alphaCutoff)，
   i_data1.a 保持 opacity；`MeshStyle::Realistic` 实体进 pbr program，
   其余进 CAD program。
4. 开关：全局 GRID_REALISTIC=1；逐实体 `MeshStyle` 覆盖。
- 验收：同场景 CAD 贴图 mesh 与 PBR mesh 混排，遮挡/透明排序正确；
  光源方向改变时仅 PBR 实体变化；50k 实例不低于 60 FPS（Release）。

## M4 CAD 实体细分桥（3~5 天）

按 OpenCADStudio 各实体 to_render 的细分算法移植（弧长/角度细分规则对齐）：

1. 线类：Line/Arc/Circle/Ellipse/Polyline/LwPolyline/Spline/Ray/MLine →
   PrimVertex 宽线 ribbon（圆角连接、抗锯齿沿用 cadPrimitives/polyline）。
2. 面类：Hatch/Solid → FillVertex 填充三角（对齐边界与填充规则）；
   MLine 双线 + 填充。
3. Point → TargetPointInstance（PDMODE 简化版：圆点）。
4. 与 RenderMode 六视觉样式联动：face3dFill、show3dEdges、hiddenLine
   等开关由现有 RenderModeFlags 驱动。
5. 演示场景：新增键位或 env 加载一组示例 CAD 实体，与大坐标 mesh 共存。
- 验收：示例实体在六种视觉样式下截图；与网格/mesh 遮挡正确；
  大坐标（1e7）下无精度抖动。

## M5 共存排序收尾（1~2 天）

1. 统一透明排序键：CAD 透明填充、透明 mesh、PBR 透明实体按相机深度
   合并为同一 painter 序，消除跨 mesh 组排序限制（文档已知问题）。
2. 混合基线：CAD 线框 + 贴图 mesh + PBR mesh + 网格四层同屏截图基线。
3. 回归：GRID_DEBUG_SLAB=1 静止 200 帧 distinct(near,far) ≤ 2；
   verify_camera_target.py 通过；Vulkan 标记仍为实验性。

## M6 后续（独立立项，不阻塞主线）

- Text/MText：SDF 文字管线（对齐 RENDER_REVISION_PLAN M5 结论）。
- Dimension/Leader/MultiLeader/Tolerance/Table/Insert（块引用）。
- 阴影（VSG HardShadows/PCSS 思路）、IBL 环境光、glTF 材质导入。
- 拾取（GPU ID buffer 或 CPU ray/AABB），与现有 BVH 共用。

## 验证与构建命令

1. 着色器：lib\compile_shaders_all.bat。
2. 构建：cmake --build build2022 --config Release。
3. 运行基线：GRID_STRESS_COUNT=0/1000/50000 + GRID_START_STYLE=0..5 截图；
   WINDOW_RENDERER=dx11/dx12/gl 各验证一轮（vk 实验性）。
4. 每个 PR 附"分析结论 → 修复项"映射与截图对比。

## 风险与约束

- Vulkan 实例路径为已记录的实验项，双档验证以 D3D11 为准。
- i_data2 扩展使实例记录从 32B 增至 48B，需复测瞬时缓冲 chunk 行为。
- acadrust 细分参数（弧长容差、Spline 次数等）以 OpenCADStudio 实现为准
  移植，差异需在 PR 中列出。
