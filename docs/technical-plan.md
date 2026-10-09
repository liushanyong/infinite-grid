# infinite-grid 技术方案

> 记录本项目从"demo 渲染"逐步演化为"完整 CAD 客户端"所做的技术决策与实现。
> 已完成部分附提交哈希作为锚点，方便回查。

## 1. 项目定位

**当前阶段**：基于 bgfx + GLM + libredwg 的 CAD 渲染原型。已有 AcGe/AcGi 绘制协议、6 种视觉样式、GPU 拾取、大坐标支持、SQLite 文档存储、实体事务/有限撤销栈、entt 场景镜像和多视图基础；DWG 转换仍是有限实体子集，尚非完整 CAD 编辑器或格式互操作实现。  
**下一阶段**：优先闭合“文档变更 → 场景脏标记/属性刷新 → 绘制与拾取缓存更新”，再扩大真实图纸的几何和文件覆盖并建立回归基线。  
**不做的事**：实时协同、跨进程共享、网络同步、字形/字体完整引擎（占位）。

参考实现：OpenCADStudio-main（实体风格）、ObjectARX 2026 SDK（协议对齐目标）、libredwg（DWG 解码）、simpline（拟合点插值验证基准）、LNLib（NURBS 算法思路）。

## 2. 整体架构（目标态）

```
┌──────────────── 持久化层（数据）──────────────────┐
│  SQLite + WAL（entities / undo_log / metadata）   │
│  文档事务 / 撤销栈 / 单进程多视口                │
└────────────────────────┬─────────────────────────┘
                         │ 事务提交 → 脏事件
┌──────────────── 运行时索引（内存）───────────────┐
│  Octree（几何邻接、视锥剔除、拾取包围盒）         │
│  kd-tree（点云/控制点最近邻、标注偏移）          │
│  map<RID, Metadata>（实体档案）                  │
└────────────────────────┬─────────────────────────┘
                         │ RID + AcGe 表示
┌──────────────── ECS 渲染层（表现）────────────────┐
│  Component: AcGeRef / AcGiStyle / Bounds /        │
│             DSDecomp / Selection / DirtyMask     │
│  System: VisibilityCull / Tessellate / Submit    │
│              Outline / Pick                       │
└────────────────────────┬─────────────────────────┘
                         │ bgfx 调用
┌──────────────── bgfx 表现层────────────────────────┐
│  视图调度 / 深度测试 / 线框/填充/着色管线        │
│  DoubleSingle 重定基 / log-depth 切片           │
└────────────────────────────────────────────────┘
```

**约束**：渲染层只读运行时索引；写只走 SQLite 事务；ECS 组件是运行时缓存，权威源在数据库。

## 3. 已完成工作

### 3.1 AcGe 值类型层（`lib/ge/`）

签名对齐 ObjectARX AcGe 内核；存储用裸 double（`.x/.y/.z` 直接访问），与 glm 双向转换做细分桥接。

- **`gepoint.h`**：`AcGePoint2d/3d`、`AcGeVector2d/3d`——运算符、`dotProduct/crossProduct/length/lengthSq/normalize`；
- **`gematrix.h`**：`AcGeMatrix3d`（glm::dmat4 存储）+ `setToTranslation/Scaling/Rotation`、变换点向量、OCS→WCS 矩阵（`setToPlaneToWorld`）、**AutoCAD 任意轴算法** `arbitraryAxis`（AAA：`|Nx|<1/64 && |Ny|<1/64` 决定种子轴），对应 DWG 挤出法向→OCS 基轴；
- **`gecurve3d.h`**：`AcGeLineSeg3d`、`AcGeCircArc3d`（圆/弧统一表达、`pointAt(angle)`、`isFullCircle()`）、`AcGeEllip3d`；
- **`geutil.h`**：`AcGeBoundBlock3d`、`AcGeTol`；**`ge.h`** 统一入口。

提交：`e29d688`（值类型层）

### 3.2 AcGi 协议层（`lib/acgi/`）

AcGi 风格的回调协议（不先细分再提交）：实体通过回调流发送图元，按当前 traits 着色，支持矩阵栈 `pushModelTransform/popModelTransform`。

- **`AcGiWorldDraw.h`** 回调：`worldLine`、`worldPolyline`、`worldTriangle`、`worldCircle`、`worldEllipse`、`worldPoint`、`worldInfiniteLine`、`text`；配套特性 `addStroke/appendTriangle/appendPoint` 在 `GeometrySink`；
- **`AcGiTextStyle.h`** AcGi 文本样式接口（textSize/xScale/obliqueAngle/setters），text 回调以当前空间绘制布局框；字形引擎就位时只改实现；
- **`AcGiLineType.h`** AutoCAD LTYPE 标准表（DASHED/HIDDEN/CENTER/DASHDOT/DOT/PHANTOM），元素按 .lin 格式正=dash 负=gap 零=dot，`acgiLineTypeMarks` 展平为 mark/stroke。

12 个实体走协议路径：Line/Circle/Arc/Ellipse/Ray/XLine/Solid/Hatch/Polyline/LwPolyline/Spline/Text/MText。共享几何帮助函数（`polylineOutline/polylineWallTriangles/hatchPatternSegments`）让细分路径与回调路径用同一份数学。

提交：`3404b56`（协议层）、`93c5e24`（闭合变体）、`655533c`（XLine 实体）

### 3.3 DWG 模型对齐与 libredwg 桥接

- **AAA 任意轴算法替换**：原自创规则改为严格 AAA，**直接读 DWG 弧/椭圆会正确**；  
- **Arc/Line/Circle thickness 墙**：补 DWG 公共字段；`tessellate(Arc)` 弧段细分后逐段挤成墙；  
- **OCS→WCS 变换**通过 `setToPlaneToWorld(normal)` 应用到 Line 端点；  
- **`lib/entities/dwg_bridge.h`**：DWG 实体→entities 转换；`main.cpp` 加 `appendDwgFile`，`GRID_DWG=<path>` 环境变量触发——`dwg_read_file` 解码 → `toEntity()` → `appendVectorPrimitive` 进入与 demo 实体同管线；  
- **CMake**：libredwg 以静态库 `LIBREDWG_LIBONLY=ON` 接入，禁用写和 JSON。  
提交：`48fad69`（AAA 修正+libredwg 桥）、`5e17d10`（运行时加载）

### 3.4 实体重构与几何修正

- **Solid 重写为特征边 + 反色边线**：细分时按共享边分析（只用过 1 次 = 边界、对角线两面共面 = 隐藏、折痕 = 显示），生成 `Stroke` 边线 → 反色输出；选中轮廓只显示外轮廓并加粗（`silhouetteWidth = 2×outlineWidth`），按朝向选择正面/背面取分；  
- **Solid `worldDraw` 修复**为 DXF 锯齿序 `(1,2,3)+(2,4,3)`，与细分路径完全一致（含绕向）；  
- **闭合 stroke 渲染修复**：ribbon 绘制主循环缺少末段回绕，统一改为 `segCount = closed ? count : count−1` + 模取点。  
提交：`5b085b0`、`5b082b1`（Solid 重写）、`6a32b77`（winding 修复）、`a9cc4b9`（闭合渲染）

### 3.5 拟合点插值与曲线高斯-勒让德积分（`lib/ge/genspline.h`）

DWG SPLINE 拟合数据 → C2 自然三次样条（Thomas 三对角解算 + 周期变体 Sherman-Morrison）；`AcGeFitSpline3d` 支持开放/周期，支持给定参数值或弦长参数化。弧长按 24 点 Gauss-Legendre 节点/权重求积——**用 Newton 迭代在 Legendre 多项式上数值生成**，避免转录错误。

**独立离线测试**（MSVC 编译直跑）16 项检查全过：开放/闭合过点精度 0、闭合缝 C1 连续误差 1.3e-9、四分之一圆弧长误差 5e-6。

`tessellate(Spline)` 拟合点路径从 C1 Catmull-Rom 替换为新实现。demo 加 SpaceSpline（5 个 z 值夸张的非平面拟合点）演示 3D 插值能力。

提交：`5b7ff26`（genspline）、`3e984df`（3D 样条 demo）、`790e0d8`（越界 bug 修复）、`a9cc4b9`（ClosedFitRing 闭环）

### 3.6 视觉样式与拾取

- **V 键循环 6 种样式**：Wireframe2D/Wireframe3D/HiddenLine/Shaded/Shaded+Edges/DepthBuffer；循环顺序与 AutoCAD VISUALSTYLES 命令一致；  
- **Wireframe3D 边界通道**：填充被抑制时为填充实体（Solid/Hatch/Rectangle/CircleFill）绘制**共享边**——只画用过一次的边，按实体颜色 `contrastAgainstBackground` 着色、与场景线框同深度参与。提交：`ddd5700`  
- **kind 分叉**：`queueGpuMeshPick` 根据 `meshFill` 标志决定走 `Kind::Edge`（线框模式，棱边线）或 `Kind::Mesh`（其余模式，表面实例）；  
- **`makeGpuPickMeshVertices` 交错修复**：faceIndex 改为逐顶点交错（之前追加在面块末尾导致顶点按 9 浮点步长错位 1-2 个浮点，引起 ID 通道"爆炸")；提交：`61bba2c`  
- **DepthBuffer 样式补全**：meshInstance 片元着色器添加 `uPrimParams.x ≈ 7` 灰度分支（之前遗漏，网格仍渲彩色材质）；新增 `GRID_DEPTH_RAW=1` 切换原始硬件深度/对数归一化；提交：`626450f`  
- **`[PICK_DUMP]/[PICK_QUEUE]` 诊断**（`GRID_DEBUG_PICK=1`）；提交：`e3d213a`、`7ea957a`

### 3.7 交互、文档与生态

- 鼠标：中键平移、Shift+中键环绕（VSG 式持久支点）、滚轮缩放、左键双击拾取；  
- 键盘：V/P/1-4/R/C/L/F/K/I/O/U/ESC；启动横幅打印最常用按键；  
- 30+ 个 `GRID_*` 环境变量分组（场景内容/渲染/自动化/诊断），完整手册在 `docs/demo-operation-guide.md`；  
- demo 30 个 CAD 实体（Line/Arc/Circle/Ellipse/EllipseArc/Polyline/LwPolyline/Spline 控制点+拟合点形式/SpaceSpline/ClosedFitRing/Ray/XLine/MLine/Text/MText/Hatch 实心+ANSI31 图案+内环挖孔+45° 加粗/Solid/Rectangle/Solid3d/ArrowHead/DashedArrow/Hexagon/Light/ParamSurface）。提交：`1552ead`（操作手册）

## 4. 算法层面对话结论（未落地，方案存档）

### 4.1 LNLib 集成

**不集成**——会从"无几何求值的小内核"跨界成"NURBS 库集成方"；当前渲染管线不依赖 NURBS 求交/投影，没有消费者。  
**逐项自写值得做**（不引用 LNLib 代码，思路照搬《The NURBS Book》）：
- 节点向量工具（`AcGeKnotVector::uniform/chordLength/centripetal`）；
- 全局曲线插值（B-spline 拟合 fit 点）—— DWG SPLINE 的核心转换路径；
- B-spline/NURBS 求值（de Boor 算法）—— DWG 3D 曲线/曲面/区域边界；
- 曲线-曲线求交、点-曲线投影（编辑器核心）—— 编辑命令依赖。

### 4.2 Octree 与 SQLite

**不混合**：Octree 用 SQL 替代会失去查询常数因子；SQLite 替代 Octree 会失去邻接指针遍历。  
**分层合作**：SQLite 存元数据/事务/撤销（持久化），Octree 在内存（运行时几何邻接），kd-tree 在内存（最近邻）；文档事务提交 → dirty sets（`{RID, old_aabb, new_aabb}`）→ Octree 增量更新 → 渲染脏标记。

### 4.3 SpatiaLite 配合 kd-tree

**不配合**：R-tree（range query 强）与 kd-tree（k-NN 强）职责正交；用 SpatiaLite "配合" kd-tree 等于两层空间索引叠加同一查询，常数因子双重惩罚。若需要混合，要么纯内存（Octree+kd-tree），要么 SQLite 持久化+kd-tree 邻接（不带 SpatiaLite）。

### 4.4 ECS 系统

**可行且自然**——渲染侧的"批量迭代几何对象"是 ECS 典型场景。  
**推荐 entt 或 flecs**：头文件、runtime archetype、cache 友好视图查询。  
**不适用 ECS 的层**：SQLite 持久化、撤销栈（用 command pattern）、Octree/kd-tree（邻接遍历与 chunked SoA 互斥）。  
**实现调整**：在 P1（数据层）后加 P1.5（引入 ECS：定义 Component + 第一个 System）——把渲染复杂度从手写压到注册 30 个 Component + 5 个 System。

### 4.5 真要做完整 CAD 软件的优先级

| 阶段 | 现状 | 仍需完成 |
|---|---|---|
| 文档/撤销 | SQLite 存储、实体事务和有限撤销栈已存在 | 符号表、块结构及其他文档状态的事务覆盖 |
| 场景投影 | entt `SceneStore`、文档桥和实体修改/撤销脏通知已存在 | 完整缓存消费者、图层/样式表变更通知及加载后的场景对账 |
| 空间索引 | Octree/BVH 实现已存在 | 接入实际视锥剔除、窗口查询和拾取，并验证增量更新 |
| 绘制管线 | AcGi-lite、`AcGsView` 与多视图基础已存在 | 将文档编辑、缓存重建、渲染提交和拾取统一为可测闭环 |
| 几何内核 | 曲线、B-spline/NURBS 与 B-rep 基础已存在 | 容差、退化输入、曲面网格和真实图纸正确性验证 |
| 文件互操作 | SQLite 文档保存/加载、部分 DWG 导入已存在 | 扩大 DWG 实体覆盖，补 DXF/DWG 导出、往返与结构化诊断 |

## 5. 设计不变量（必须持续守住）

1. **渲染纯函数**：渲染只读运行时索引副本，同一帧两次绘制结果相同（可重入）；  
2. **事务先于重绘**：编辑事务提交前，渲染看到旧状态；提交后才 push 脏事件；  
3. **脏更新从实体 ID 起步**：当前事务提交按实体通知场景镜像；接入空间索引后扩展为旧/新 AABB 增量更新，避免全表刷新；
4. **双精度不变量**：相机/几何全 double 精度，主缓冲区始终是 DoubleSingle 拆分（高/低）；  
5. **背景噪声可承受**：拖拽频繁小事务用 SQLite WAL 应付；  
6. **AcGe 与 AcGi 协议层稳定**：签名对齐 ObjectARX，移植代码仅需改 include 即可编译。

## 6. 后续推进顺序建议

| 优先级 | 任务 | 预期交付 |
|---|---|---|
| **P1** | 文档变更闭环：修改、撤销/重做、块引用变换与场景投影统一失效；扩展到符号表/图层状态 | 实体编辑后几何、属性、渲染与拾取不陈旧，单体修改不触发全图重建 |
| **P2** | DWG/DXF 能力矩阵与诊断；优先补常用实体导入、未知对象保留策略及往返测试 | 样本图纸无静默丢失，降级/跳过有可读诊断 |
| **P3** | 正交 CAD 工作平面、视口状态同步、捕捉与子图元拾取 | 同一文档多视图操作一致 |
| **P4** | 把 Octree/BVH 接入剔除和查询；建立大坐标/大图纸/多视口自动化基准与后端降级测试 | 增量更新和关键交互有可重复性能及正确性门槛 |