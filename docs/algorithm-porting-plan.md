# 算法移植计划：external/ 参考库 → infinite-grid

> 生成日期：2026-10-12
> 方法：主代理 + 4 个并行只读子代理，对**宿主两个方向**各做函数级盘点，再做交叉比对。
> 宿主侧基线：`lib/ge/`（15 头文件，**零 .cpp**，全 inline）+ `lib/brep/`（11 头文件 + 唯一 TU `BRep.cpp`，仅实现 `signedVolume`）
> 参考侧：external/ 24 个项目，本次深挖 5 个（NURBS/曲线 3 个 + 拓扑/网格 5 个）

---

## 0. 结论摘要

**宿主不是"缺很多算法"，而是"已有一批算法在数学上不完整、且工程上缺少统一契约"。** 这个区别决定了移植策略。

三类缺口，按性质完全不同：

| 类型 | 特征 | 处理方式 |
|---|---|---|
| **A. 契约缺口** | 算法本身在，但没有统一抽象，导致各处硬编码 epsilon、结果无法被验证 | 移植"概念"，工作量以小时计，收益最大 |
| **B. 一般性缺口** | 算法只覆盖特例（凸体 / 90° 夹角 / 平面），代码自己声明了作用域 | 移植"算法内核"，是内核能力的真正升级 |
| **C. 能力缺口** | 完全没有的算法族（最近点、测地距离、kd-tree、平行曲线） | 移植"整块算法" |

**Tier 0 的六项全部是 A 类，总工作量 M 以内，但能同时消掉 B 类的三个作用域声明。** 建议从这里开始，而不是直接攻通用布尔。

另外，本次盘点**推翻了三条此前的判断**，见 §5。

---

## 1. 宿主已有算法基线（"Have" 列）

去重后约 120 项。以下按子系统列出**能力边界**，供比对：

### 1.1 `lib/ge/` — 值类型与算法层
- **值类型**：`AcGePoint2d/3d`、`AcGeVector2d/3d`、`AcGeMatrix3d`（glm::dmat4 存储）、`AcGePlane`、`AcGeBoundBlock3d`、`AcGeTol`
- **变换**：AutoCAD 任意轴（OCS）算法、OCS→WCS 基矩阵
- **鲁棒谓词**：`orient2d/3d`、`incircle/insphere`（Shewchuk，已接线，`/fp:precise` 锁定）
- **2D 平面图元**：line/circle/arc 值类型 + 7 个两两求交（精确符号判定走谓词，量级判定走容差）
- **NURBS 核心**：A2.1 span 搜索、A2.2 BasisFuns、A2.3 DersBasisFuns、A9.1 平均节点向量、chord/centripetal 参数化
- **曲线曲面**：de Casteljau、Bézier 导数 Bézier、A3.1/A3.2/A3.5/A3.6、A4.1-A4.3 有理求值与导数、曲率 κ=|C′×C″|/|C′|³、A9.1 全局插值（配点矩阵 + 稠密高斯消元 + 部分主元）
- **拟合样条**：Thomas 三对角、Sherman-Morrison 循环三对角、Gauss-Legendre 节点（Newton 迭代生成，非转录表）、自然三次样条、周期闭环样条、24 点求积弧长
- **求交与投影**：曲线-曲线（AABB 拒 + 64×64 播种 + Newton + 二阶修正）、曲线-曲面（48³ 扫描 + 3 未知 Newton）、点-曲线（33 种子标量 Newton）、点-曲面（9×9 播种 2×2 Newton）
- **空间加速**：松散八叉树（box/radius 查询）、binned-SAH BVH（三轴 32 桶 + 中位回退 + 近优先有序栈遍历 + refit）、量化 BVH
- **注意**：`ge.h` 只 re-export 8 个头，`ge2d.h`/`gepredicates.h`/`gebvh.h`/`geqbvh.h` 必须直接 include

### 1.2 `lib/brep/` — 拓扑与特征
- **拓扑**：Body→Shell→Face→Loop→CoEdge→Edge→Vertex 半边模型，arena bump 分配，每边严格两个径向 CoEdge，双向链表 Loop（CCW 绕外向法向）
- **构建**：微米栅格顶点去重（`llround(coord·1e6)`）、无向边去重、凸环绕质心角排序、Newell 法向、圆弧边规范化到 CCW ≤ π、makeBox、平面多边形拉伸
- **布尔**（3-b）：截交线两平面解析求交 → 3D 双向裁剪 → 面片切分 → 质心分类 → 规则表筛选 → 共享 map 焊接
- **混合**：等退距 chamfer（Sutherland-Hodgman 半空间裁剪）、滚球 fillet（仅 90° 二面角）、加厚
- **扫掠**：旋转（轴向边→柱面片 / 径向边→平面扇环）、放样（平行对应边强制检查）
- **历史**：参数化 journal + 重放，撤销 = 截断重放
- **拾取**：面细分（平面环扇 / 柱面两条弧线间 strip / 弧边 16 弦）+ BVH 剪枝 + Möller-Trumbore 精确求交 + 柱面解析径向法向
- **IO**：AP203/AP214 Part-21 单 `MANIFOLD_SOLID_BREP` 输出，严格拓扑序，实体去重
- **校验**：5 项结构检查（双边、双径向对、环闭合、顶点一致、**里外翻检测**）
- **自测**：`KernelSelfTest.h`，解析真值断言（体积/面积/拓扑计数/Pappus/STEP 实体数），写 `kernel_selftest.log`

### 1.3 其余子系统
- **acdb**：单调句柄分配、事务（前像/后像）、256 深度撤销栈、SQLite 持久化 + ops journal、JSON 值树、NOD 字典、嵌套块实例 DFS 展开（32 层上限）
- **acgi**：traits 语义绘制协议、6 种 LTYPE 图案展开、SHX/bigfont 双字节、字形 SDF 布局 + 视口裁剪
- **acgs**：screen-density 自适应弦高容差 `acos(1−tol/r)` 夹在 [1°,15°]、DrawListCache（键 = entityRevision × traitsVersion × 容差桶 × rebase 原点）、深度 slab 滞回（立即扩、20 帧稳定缩）、GPU ID 拾取 + FNV-1a 变更检测、metafile 重放 + 逐实体区间
- **rendering**：DoubleSingle 编码、6 种视觉样式 → 通道标志、mesh instance（RGB 塞进 `.w`）、GPU 曲线求值协议（16 控制点 / 4 节点 / 弧参数）
- **coordinate**：`WorldRebase` chunk 锚定（`chunkSize = 1e4`）

---

## 2. 宿主自述缺口（最有价值的输入）

这些是**代码注释里自己写明的**作用域限制，直接就是需求列表。

### 2.1 `lib/brep/` 的作用域声明（原文引用）

| 模块 | 声明 |
|---|---|
| `BRep.h` | `Face::outerLoop` — "single loop in M2" → **无内环/孔**<br>"M4: planar + cylindrical faces, line + arc edges" → **无 NURBS/B 样条面与边** |
| `BRepBoolean.h` | "**Scope (first milestone): convex solids, no coplanar face pairs (On-classification and solid-angle In/Out for untouched faces arrive with the general phase.)**"<br>`opKeeps`: "**(On-classification arrives with coplanar-face support.)**" |
| `BRepBlend.h` | "**Scope (first milestone): convex shells, straight edges, planar face pairs; filletEdge further requires the 90-degree dihedral of box-like solids**"<br>`if (hasArcs) return nullptr; // curved input beyond the M4 scope` |
| `BRepSweep.h` | "slanted edges (conical faces) **return nullptr until the NURBS phase**"<br>无整圈旋转的 seam 焊接 → **闭合旋转体不可能** |
| `BRepRayCast.h` | 弧边固定 16 弦 → **柱面拾取带弦误差**（自测承认 ~0.015）<br>**BVH 在每次 rayCast 内部重建** |
| `StepWriter.h` | "Line and circle edge geometry are supported" → 无修剪曲线、无 B_SPLINE_SURFACE<br>每面单 `FACE_OUTER_BOUND` → **孔无法表达**<br>`ADVANCED_FACE` 硬编码 `same_sense = .T.`<br>**无 STEP reader** |
| `BRepHistory.h` | 只有 4 种 FeatureKind；撤销 = **全量重算**；每次 blend/boolean 泄漏一个 arena |
| `Validate.h` | 无几何自交检查、无 Euler 特征/流形边计数检查 |
| `KernelSelfTest.h` | **无 NURBS/B 样条、BVH/八叉树/射线遍历、点投影、求交的覆盖** |

### 2.2 `lib/ge/` 的算法层缺口

- **`genurbscore.h`**：次数 ≥32 抛异常（`left[32]`/`ndu[33][33]` 固定栈）；`isValidKnotVector` **只查单调性与尺寸，不查多重度/连续性/是否 clamped**
- **`geintersect.h`**：头文件声称 "control-polygon subdivision §7.1"，**代码实为固定 64×64 / 48³ 均匀采样** → 落两个采样点之间的交集会漏；仅接受 `AcGeBsplineCurve3d`，**不支持有理 NURBS 曲线求交**；无曲面-曲面、无解析特例（圆/柱/平面）
- **`geproject.h`**：33 / 81 种子 Newton，**无全局最小保证**；返回 `optional`，"未收敛"与"很远"无法区分
- **`geoctree.h`**：`nearestByCenter` 是**穷举 O(n)**（注释写"先访问包含点的子节点"但代码按下标顺序遍历）；**无 remove/update API**
- **`gebvh.h`**：固定 128 层栈；"栈在混合下降后**不按距离排序**"；refit "质量优雅退化"；无合法 SAH 分割时走 `bestAxis < 0` → **无条件成叶**，可突破 maxLeafSize
- **`geqbvh.h`**：**注释与实现不符**（详见 §5.1）
- **`geutil.h`**：**`AcGeTol` 是死类型** —— 定义了但全 `lib/` 无引用，各算法各自硬编码 `1e-9`…`1e-14`

### 2.3 其余子系统

- **acdb**：ObjectARX open/close 协议超出值类型层作用域；**符号表与块结构编辑"毒化事务"故不在事务范围内**；非图形 NOD 变更不通知；BlockReference 细分产出空
- **acgs**：块实例的**旋转与非均匀缩放对曲面几何退化为纯平移**；VPORT twist 忽略；每视口 FBO 调度未落地；选区尚未迁到 ECS `Selected` tag；变更后的 payload 只在 re-ensure 时刷新
- **rendering**：`WebGpuRenderer::drawLineInstances` **空重写**、`loadMeshTexture` 返回 0
- **text**：`text_render.h` 是与 `text_font.h` **并行的旧 API**，256 项定长表不支持完整 Unicode

---

## 3. 移植候选矩阵

难度 S≈半天 / M≈1-3 天 / L≈1 周+。价值按"对内核能力的关键性 × 解除的作用域声明数量"评。

### Tier 0 — 契约类（A 类缺口，M 以内总工作量，建议立刻做）

| # | 算法 | 来源 | 核心 | 价值 | 难度 |
|---|---|---|---|---|---|
| 1 | **容差阶梯 + 启动期 check** | cadcore `crates/cadcore-union/src/tolerance.rs:8-80` | 两轴分离：精度阶梯 `refine(1e-12) ≤ model(1e-9) ≤ knit(1e-3)`；搜索半径 `weld(2e-2)`、`sliver(5e-2)` **可远大于 knit**；`check()` 验证有序否则 `Err` | **极高**。宿主目前是"一个隐式 epsilon 走天下"，`AcGeTol` 死类型。cadcore 的注释直指动机：*"mixing rungs implicitly is how the legacy implementation accumulated µm-level drift"*。宽搜+精化的分离让"搜索容差大"不再等于"输出精度松" | **S** |
| 2 | **流形性校验闸（Gate 1）** | cadcore `validate/manifold.rs:27-90` | 建 `edge → Vec<(face,sense)>`；`len != 2` 是硬违规；**同向只算 warning**（STEP writer 的 shared-edge cache 会几何地反推 sense，故内存层同向不构成失败）；遍历带 `guard > 100_000` 熔断 | **极高**。宿主布尔 weld 产出的 shell **从未被检查流形性**。hard/warning 二分是成熟度标志 | **S** |
| 3 | **边-面距离校验（Gate 4）** | cadcore `validate/distance.rs` | 每条输出边必须在其界定的每个面 `knit` 距离内 | **极高**。宿主写 AP203/214，**接收方缝合容差正是这个数**（SolidWorks ≈1µm 量级） | **S** |
| 4 | **容差顶点合并 VertexPool** | cadcore `arrangement.rs:124-158` | 空间哈希网格 cell = `tol*4`，插 3×3 邻域，命中后用 `dist2 <= tol²` 精确复核 | **极高**。让"两条交线各自算出的同一点"必然坍缩成同一 vertex id，不需要全局排序或容差传播。宿主 weld 只有共享 map，缺通用几何吸附器 | **S** |
| 5 | **Van Oosterom–Strackee 归一化立体角** | mcut `source/preproc.cpp:1777-1850` | 逐三角 `atan2(-bary, 1+Σdot)`，求和后 `/(2π)`；**numerator==0 按零立体角处理**（避免 `atan2(0,负)` 跳变 ±π） | **高**。宿主"固体角 In/Out 待补"的**更优实现**：纯顶点向量、无需参数域、对任意流形网格成立。CZY3DKernel 的参数域高斯积分只在多面体路径有效 | **S–M** |
| 6 | **输出溯源映射（vertex_map / face_map）** | mcut `internal/kernel.h:175-186` | 输出元素 → 输入元素 descriptor 数组，null 表示交点 | **中高**。宿主布尔 weld 后**丢失"这个新面来自哪两个输入面"**，这是后续历史/参数化/PMI 的前提 | **S** |

> Tier 0 的共同特征：**几乎不增加代码量，但把"看起来对的结果"变成"可审计的结果"**。宿主现在最危险的不是算错，而是算出拓扑不合法的模型却没人拦。

### Tier 1 — 一般性缺口（B 类缺口，解除作用域声明）

| # | 算法 | 来源 | 核心 | 解除什么 | 难度 |
|---|---|---|---|---|---|
| 7 | **per-face uv DCEL arrangement** | cadcore `arrangement.rs:171-367` | O(n²) 段求交（**参数域归一化容差** `tt=tol/len_r`）+ VertexPool + 半边 `next(h) = ring[pred(twin(h))]` + 鞋带面积分内外环 | **凸布尔→一般 case 的必要前置**。不做参数域划分，交线穿过面内部就会整面误判。同时解决圆柱/环面并集 | **M** |
| 8 | **组级三态分类（局部角 + 立体角瀑布）** | CZY3DKernel `Boolean.cxx:1826-1904, 2167-2223` | 连通组（只跨非交线边 DFS）→ 共享边比较 `InnerSide = N×T` 的有向角 → 失败**回退**固体角。`UnKnown` 是三态一等公民，"宁可说不知道也不猜" | 正面命中 `BRepBoolean.h` 的 "general phase" 声明 | **M** |
| 9 | **共面(On)判定 IsGroupOn** | CZY3DKernel `Boolean.cxx:2102-2165` | 前置：组只有 1 个面 + 每边 ≥4 coedge；边集合恒等；**用两层同向标志的异或等价判法向同向** | `opKeeps` 的 "On-classification arrives with coplanar-face support" | **M** |
| 10 | **Pave / PaveBlock 切分账本** | CZY3DKernel `Boolean.hxx:46-138` | `Pave{vertex, param, edgeIndex}` = 落在原始边参数轴上的交点；`PaveBlock` = 两相邻 Pave 间的区间；`SplitPaveBlocks` | 一般布尔的切分参数管理载体，**宿主完全没有** | **M** |
| 11 | **有向角 AngleWithRefRoughly** | CZY3DKernel `Boolean.cxx:2266-2294` | 用 `cross(d1,d2)` 的模作 sin、点积作 cos 得 β，再用 `cross ⊙ ref` 符号定方向，β<0 加 2π | 避开 `atan2` 的分支与库差异；小函数即插即用（第 8 项的依赖） | **S** |
| 12 | **kd-tree / 有界 KNN 结果集** | KrisLibrary `geometry/KDTree.h`（BSD-3）<br>LNLib `OctreeCollector.h:31-52`（`KnnResultSet(capacity)` / `RadiusNnResultSet(radius)` / `WorstDist()`） | 空间划分 + 优先队列有界结果集 | 宿主**完全没有 kd-tree**，而 `technical-plan.md` 4.2 规划了它负责"点云/控制点最近邻、标注偏移"，P3 捕捉强依赖 | **S–M** |
| 13 | **三角形-三角形最近距离 + OBB SAT + best-first 遍历** | KrisLibrary `geometry/PQP/src/{TriDist,OBB_Disjoint,BVTQ}.h` | 闭式最小距离解 + 15 轴分离轴快速排除 + 优先队列近点遍历（**非 DFS**） | 宿主只有 Möller-Trumbore（**相交**），不覆盖最近点/最小距离/穿透。`PenetrationDepth.h` 的传播法可同批移植 | **M** |
| 14 | **测地距离（MMP + 虚拟边）** | KrisLibrary `meshing/Geodesic.cpp` | 钝角检测（纯点积 `ba·ac>0`）→ 支撑半平面 → 从 `a2` 绕 `a1` 旋转辐条找虚拟顶点 → Dijkstra/FMM on 真实边+虚拟边。O(n log n)。退化处理极完整（四类三角不等式 + 相切等号 + 数值错误 fallback） | 宿主有 BVH 但**没有任何曲面上的距离度量**。用于加工路径、拔模/倒角路径、点到边集距离场、表面参数化 | **M** |
| 15 | **偏差驱动自适应细分** | gbs `bscanalysis.h:581-712` / `bssanalysis.h:75,142` | 递归二分直到局部弦偏差 ≤ `dev_max`，以 `approx_max_sample_points` 封顶 | 宿主现在是**屏幕密度驱动**的 `acos(1−tol/r)`（视觉导向，随缩放变化）。这是**几何导向**的"细分到 0.01mm"，导出/碰撞网格需要 | **S–M** |
| 16 | **特征保留采样（拐点/极值插入）** | qaws `qaws_sampling.h:62-74` | 均匀基准集 **+ 拐点 + 极值点**（曲率与一阶导的根） | 近免费的保真度提升，比均匀细分严格更好 | **S** |
| 17 | **BVH refit / rotate + 两级索引** | embree `kernels/bvh/{bvh_refit,bvh_rotate,bvh_builder_twolevel}.cpp`（**文件确实存在**，已核实） | refit 只重算包围盒并旋转节点使体积更紧；two-level 空间索引支持"改几个动几千个" | P1 的频繁小事务正是这个形态。宿主 `gebvh.h` 已有 refit 但"质量优雅退化"，缺 rotate 与两级 | **M** |
| 18 | **Hi-Z 遮挡剔除 + 两帧时间滞回** | gl_cadscene `cullingsystem.{hpp,cpp}` + `cull-downsample.frag.glsl` | 深度金字塔逐层保守测试；可见性位三模式（当前 / 与上帧复用 / 与上帧不同）；transform-feedback **与 SSBO 双路径**；前缀和压实 | 对齐 P4"把 Octree/BVH 接入剔除"。宿主目前只有视锥，缺遮挡一路 | **M** |

### Tier 2 — 曲面与建模能力（C 类缺口）

| # | 算法 | 来源 | 核心 | 难度 |
|---|---|---|---|---|
| 19 | **解析平行曲线框架（含一二阶导）** | gbs `curveoffset.h:83-230` | 2D 用单位法向 + 完整导数代数；3D `normal = normalize(cross(direction, tangent))`。**offset 查询无需 refit** | **M** |
| 20 | **2D 平行曲线的 cusp / 自交修剪** | qaws `qaws_operations.h:39` / `.c:1983+` | 前因子 `f(t) = 1 − κ(t)·d`（offset 反转的教科书判据）；三级修剪，第 2 级把一个 offset 拆成**多个闭合环**并丢弃尾部 | **L**（三个库里只有它做修剪这半；LNLib 有 NURBS refit 那半但明确不处理自交） |
| 21 | **认证容差的节点移除 / 全局逼近** | LNLib `NurbsCurve.h:261-275` | `ComputerRemoveKnotErrorBound` → 只移除误差界内节点 → **逐节点误差报告** | **M** |
| 22 | **rolling-ball 2D blend** | gbs `bscanalysis.h:876-927` | 嵌套最近点：解 `u2` 使两条法线的最近点距离相等；含共线退化 | **M** |
| 23 | **Gordon + 自动交点发现** | gbs `bssbuild.h:483-562` | `build_intersections` **自己算** N×M 交点网格，不要求调用方提供 | **M** |
| 24 | **loft 的 V 参数对应 + spine 切向传递** | gbs `loft/loftBase.h:182`、`bssbuild.h:124-262` | 未指定时**平均各截面参数**（退化行回退均匀）；`spine_tangent_to_pole` 做旋转最小化框架的变体 | **M** |
| 25 | **B-rep 面构建器：角点合并 + seam 复用 + 退化分类** | gbs `gbs-brep/builders.h:481-573`、`closure.h:48-107` | 四角中 `dist<tol` 的合并为共享顶点；`closed_u/closed_v` 时**复用对边而非造 seam 边**；`degenerate` 边永不报为 seam；`uv_signed_area` 判外环/孔 | **M–H**。周期曲面（环面/球/旋转）的可用与不可用之别，**教科书不覆盖** |
| 26 | **shape deformation（warp/flatten/bend）** | LNLib `NurbsCurve.h:324-334` | 沿平面法向按形状函数推移参数区间的 pole 网，保端点 | **M** |
| 27 | **约束式曲线修改** | LNLib `NurbsCurve.h:340` | 任意参数/导数约束 + 钉住的 pole 下标 + 每约束的施加次数 | **M–H** |

### Tier 3 — 中长期

| # | 算法 | 来源 | 难度 |
|---|---|---|---|
| 28 | **约束 Delaunay + 曲率驱动加密** | gbs `inc/topology/tessellations.h`（Boyer–Watson → 约束 CDT → `crit_max` 插点） | **H**（边界仅支持星形环） |
| 29 | **转置有限元插值（TFI）+ 尺寸律** | gbs `gbs-mesh/{tfi,mshedge}.h`（`degree-2 B-spline law` 过 `{(0,0),(h1/L),(1-h2/L),(1)}`） | **H** |
| 30 | **半边描述符即索引 + free-list** | mcut `internal/hmesh.h:49-260`（描述符=uint32，`twin = h^1`，**索引使 vector realloc 对外透明**） | **M** |
| 31 | **LM 秩亏正则化 SSI 预测-校正** | cadcore `geom/ssi.h:1-31`（`JᵀJ` 4×4 但**秩≤3**，`λI` 正则化自由切向使投影**落在**曲线上；配 marching squares 回退） | **M–L** |
| 32 | **面配对分类学（退化前置）** | cadcore `geom/classify.rs`（`AxisRelation` × `CrossingDepth`） | **S** |
| 33 | **1D 区间 arrangement（pcurve 参数区间重叠）** | KrisLibrary `geometry/Arrangement1D.h/.cpp` | **S–M** |
| 34 | **SoA 批量求值 ABI + 尺寸门控并行** | qaws `qaws_eval.h:44-69` + `batch/*.c`（SoA in/out、两次调用先取容量）<br>gbs `bscurve.h:65` `values(u_list,d)` + `execution.h:69-144`（**bit-identical 确定性 + 实测小输入回退 25× 门槛**） | **S–M** |
| 35 | **状态化遍历 `advance(dt)`** | qaws `qaws_traversal.h` | **M** |
| 36 | **QEM 简化 + 法锥判据** | meshoptimizer（法锥权重 `cone_weight`） | **S–M**（只能用于细分后的显示网格） |
| 37 | **Yuksel conic 重构** | qaws `qaws_yuksel.c:119-425`（三点二次 Bézier 二分 + 密切平面标架 + 垂直平分线拟合圆/椭圆） | **M**（CAM 探测几何平滑） |

---

## 4. 建议落地顺序

分三批，每批独立可交付、可回归。

### 第 1 批：契约与闸门（对应 Tier 0）
**动机**：先把"能验证"建起来，再动算法。否则后续每一个 Boolean/fillet 改动都没有判据。

1. `lib/ge/geutil.h`：激活 `AcGeTol`，落地 Tier 0-1 的容差阶梯（两轴分离 + `check()`）——**替换全库硬编码 epsilon 的第一步**
2. 新增 `lib/brep/ValidateGates.h`：Gate 1（边恰好 2 用）+ Gate 4（边-面距离 ≤ knit），区分 hard / warning
3. `lib/brep/BRepBuild.h`：引入 VertexPool 做顶点吸附，替换硬编码 `llround(coord·1e6)` 栅格
4. `lib/brep/BRepBoolean.h`：`performBoolean` 出口接 Gate 1

**回归门槛**：现有 `KernelSelfTest` 的 20+ 项解析真值断言（box 1000 / union 1720 / intersect 280 / subtract 720 / chamfer 995 / fillet 991.4159 / revolve Pappus 3π/4 / loft 1750 / STEP 实体计数）**一条都不能破**。

### 第 2 批：一般化布尔（对应 Tier 1 的 7-11）
顺序上是强依赖的：
```
VertexPool(4) → Pave/PaveBlock(10) → 有向角(11) → 立体角(5) → 组级三态分类(8) → 共面判定(9)
                                    ↑ 平行于
                          per-face uv DCEL arrangement(7)
```
建议先做**立体角(5)** + **有向角(11)** + **组级分类(8)**，用**现有凸路径的输入**验证——应当与旧结果逐面一致；再上 DCEL arrangement(7) 与 Pave(10)，此时才引入非凸与共面用例。

### 第 3 批：查询与加速（对应 Tier 1 的 12-18）
kd-tree(12) → 最近点(13) → Hi-Z(18) → BVH refit/twolevel(17) → 测地距离(14) → 细分策略(15,16)
这一批与几何内核正交，可并行推进，不阻塞前两批。

---

## 5. 对此前结论的三条修正

### 5.1 `geqbvh.h` 的注释描述了从未实现的算法
注释声称"每轴一字节量化、per-node start+scale、子边界存为 unsigned char、**约 6× 压缩**"。
实际 `QuantizedNode` 存的是 `float lower[2][3]` / `float upper[2][3]`（24 字节/子节点），**无 start/scale、无字节量化**。真正实现的保证只有 `nextafterf` 单 ulp 向外加宽。

⇒ 要么补实现，要么改注释。**这是全库唯一一处文档比代码乐观的地方**，且恰好会误导"内存占用"的容量规划。

### 5.2 KrisLibrary 没有 DCEL 插入算法
`geometry/DCEL.h` **只有 53 行**，且只是三个裸 struct 的字段声明（且连 namespace 都没有），**无插入、无事件队列、无面分裂**。全库检索 `DCEL` 零命中。
真正的平面算法是 `geometry/SegmentOverlay.h`（sweep-line overlay），而**作者自己在 `:24-25` 写明** *"I'm not sure if this works completely accurately or not — uses floating point numerical precision."*
⇒ DCEL arrangement 应以 **cadcore `arrangement.rs`** 为准（完整、自洽、有容差分离设计）。

### 5.3 qaws 的 "GLSL/HLSL/Halide 后端" 是空壳
`qaws_glsl.h` / `qaws_hlsl.h` / `qaws_halide.h` 各自**只有一个 backend define + 两个 include**，无 kernel 源码、无 codegen、无资源绑定。
`simd/qaws_simd.h` 也只有 `add/sub/mul` + load/store/broadcast——**无 FMA、无除法、无 sqrt、无比较/混合**；而 `batch/qaws_batch_bspline.c:33-57` 是**标量 for 循环**，根本没调用 SIMD 层。
⇒ 可移植的是 **SoA 批量 ABI 签名、两次调用容量协议、不可变曲线 + 可变遍历的线程安全契约**，而不是"GPU 后端"。

**附带修正**：`external/embree-master` 的"被裁剪"风险等级应下调——`kernels/geom` 与 `kernels/traverse` 确实缺失，但 `kernels/CMakeLists.txt` **零处引用**它们，所以这是**自洽的 BVH-only 构建**，缺的是图元求交能力而非构建完整性。而 `kernels/bvh/`（含 `bvh_refit` / `bvh_rotate` / `bvh_builder_twolevel` / `bvh_node_qaabb`）**完整存在**。

---

## 6. 明确不移植

| 项 | 理由 |
|---|---|
| **LNLib 全部 NURBS Book 例程**（A5.1/A5.8/A5.9/A5.10/A5.7/A9.1/A9.2/A9.5/A9.7） | 宿主 `genurbscore.h`/`gebspline.h`/`genspline.h` 已有。这是 LNLib 的绝大部分行数，**一条都不是缺口** |
| LNLib `Polynomials.h` / `Matrix4d.h` / `FFT.h` / `ValidationUtils.h` | Horner、Bernstein、刚体变换构造、Ooura FFT —— 平凡或与曲线几何无关 |
| **qaws GLSL/HLSL/Halide 后端、SIMD 层** | 见 §5.3 |
| **qaws clothoid** | Fresnel 积分正确，但是道路/铁路走向的窄场景；且 36 行可自行实现 |
| **gbs-occt 的 `intersections.h` / `measure.h`** | 各 4 行 `Standard_EXPORT` 包装，无算法 |
| **gbs-occt `discetize.h`** | **7 行空 namespace stub** —— 文件名最误导，头文件里什么都没有 |
| **Marching Cubes / TSDF 重建 / 共形参数化** | 宿主无标量场、无深度相机、已有精确曲面参数 |
| **HACD 凸分解** | 除非做网格快速预览/代理碰撞，否则场景不匹配（宿主是精确 B-Rep 内核） |
| KrisLibrary `GLdraw` 显示层 | OpenGL 1.x-2.0 固定管线时代代码；只取 `camera/` 包的投影数学 |
| `external/stb` | **整个目录可删**：宿主 `lib/text/text_font.cpp` 的 `#include "stb/stb_truetype.h"` 实际解析到 `lib/bgfx/3rdparty/stb/`，图像解码走 `lib/bimg/3rdparty/stb/`。这份是重复副本 |
| OpenCADStudio / Quake3e 的任何代码 | 仅作设计参考（GPL） |

---

## 7. 验证策略

1. **回归基线是现成的**：`KernelSelfTest` 的解析真值断言可直接当门禁，扩展它而不是另起炉灶
2. **自测当前有盲区**——完全没有 NURBS / BVH / 八叉树 / 求交 / 投影的覆盖（`KernelSelfTest.h` 自述只覆盖 geom2d + brep）。**第 1 批开工前应先补这两块**，否则第 3 批无从验证
3. **Boolean general phase 的对拍基准**：CZY3DKernel 自述对 OCCT 的最大几何相对差约 `1e-15`，可作为宿主重构后的数值门槛（该 README 的方法论值得照抄：几何量比对 + 拓扑计数 + 面积体积 + 合法性检查**四项都查**，因为它明确指出"几何一致 ≠ 拓扑组织一致"）
4. **环境约束**：本机 `bash` 工具失效（`Fatal error. Your Windows doesn't fully support CET`，exit `2148734214`），无法编译或运行自测。**上述所有"建议落地顺序"都需要在 bash 恢复后验证**，本次只完成了静态分析。

---

## 8. 配套建议

- 本计划的"Have"列与 §2 的"缺口"列，建议回填进 [`external-project-map.md`](./external-project-map.md) 的"设计血缘/已吸收"一节，以及 [`external-project-graph.json`](./external-project-graph.json)（新增 `algorithmic-gap` 关系类型）
- `lib/ge/` 全 inline 且**零 .cpp**，而 `lib/brep/` 只有 1 个 TU。移植时若引入编译期常量（如容差阶梯），header-only 结构会放大编译期成本，值得提前评估是否需要给 `lib/ge` 引入至少一个 TU 承载常量