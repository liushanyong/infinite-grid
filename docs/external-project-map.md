# external/ 开源依赖图谱

> 扫描根：`E:/infinite-grid/external` · 生成日期：2026-10-11 · 共 **24** 个项目
> 机器可查询版：[`external-project-graph.json`](./external-project-graph.json) · 交互检索页：[`external-project-map.html`](./external-project-map.html)

---

## 0. 先说结论

**这 24 个项目里，只有 1 个真的被编译进主工程。**

`CMakeLists.txt:201-215` 只接了 `external/predicates`（Shewchuk 精确谓词），而且是刻意锁了 MSVC `/fp:precise` 的。其余 **23 个处于 vendored-but-unwired 状态**——既不进构建，也没有任何注释记录引入原因和取舍。

这本身是本次扫描发现的最大问题，比"选哪个库"更值得处理：`external/` 已经膨胀成一个 24 项的参考堆，其中：

| 事实 | 影响 |
|---|---|
| **4 个 NURBS / 曲线库**（LNLib、gbs、qaws、CAD-main 的 Bernstein）功能高度重叠 | 真正能用的内核只有 cadcore（Rust）和 OCCT 路线，其余是同质化竞争 |
| **5 个项目没有许可证文件** | qaws、simpline、CAD-main、CZY3DKernel-main、DB-CAD-master —— 法律上**只能读不能 vendor** |
| **4 份拷贝被裁剪或改版** | tinygltf（4 文件）、stb（无 README）、meshoptimizer（无构建文件）、**embree（缺 kernels/traverse 与 kernels/geom，尚未证明能构建）** |
| **3 条互斥的渲染栈** | Vulkan-only / wgpu / 真 OpenGL，只有最后一条能落到宿主 |

---

## 1. 全量速查表

图例：`🟢` 宽松许可可借鉴 · `🟡` 弱 copyleft 需动态链接或合规 · `🔴` 强 copyleft 禁抄 · `⚫` 无许可证不可用

| # | 项目 | 语言 | 类别 | 许可 | 宿主状态 | 相关度 |
|---|---|---|---|---|---|---|
| 1 | **predicates** | C | 鲁棒性 | 🟢 public-domain | **已编译接入** | 高 |
| 2 | **gl_cadscene_rendertechniques** | C++/GLSL | 渲染器（真 OpenGL） | 🟢 Apache-2.0 | 参考 | **最高** |
| 3 | **large-coordinate-rendering** | JS/GLSL | 大坐标精度 | ⚫ 未验证 | 参考（强耦合） | 高 |
| 4 | **KrisLibrary** | C++17 | 数学/几何/相机 | 🟢 BSD-3 | 参考 | 中高 |
| 5 | **meshoptimizer** | C++ | 网格优化 | 🟢 MIT | 参考 | 高 |
| 6 | **embree** | C++ | 光追 / BVH | 🟢 Apache-2.0 | 参考 | 高 |
| 7 | **cadcore** | Rust | 精确 B-rep 内核 | 🟢 MIT | 参考 | 高（只读） |
| 8 | **CZY3DKernel** | C++17 | 特征识别 / Boolean | ⚫ 声称 LGPL 无文件 | 参考 | 高（只读） |
| 9 | **LNLib** | C++17 | NURBS | 🟡 LGPL-2.1 | **已决策不集成** | 高（只读） |
| 10 | **gbs** | C++23 | NURBS + B-rep | 🟡 LGPL-3 | 参考 | 高（只读） |
| 11 | **mcut** | C++11/C | 网格 CSG / 切片 | 🟡 LGPL-3+ | 参考 | 高 |
| 12 | **OpenCADStudio** | Rust | CAD 应用 | 🔴 GPL-3 | **首要设计参考** | 高（禁抄） |
| 13 | **VulkanSceneGraph** | C++17 | 场景图 | 🟢 MIT | 参考 | 中高（架构） |
| 14 | **qaws** | C | NURBS + GLSL 后端 | ⚫ **无许可证** | 参考 | 高（只读） |
| 15 | **stb** | C/C++ | 工具 / 图像 / 字体 | 🟢 双许可 | 参考 | 中 |
| 16 | **tinygltf** | C++11 | glTF IO | 🟢 MIT（仅头文件内） | 参考 | 中 |
| 17 | **simpline** | C++11 | 样条插值 | ⚫ **无许可证** | 验证基准 | 中 |
| 18 | **Quake3e** | C | 游戏引擎 | 🔴 GPL-2 | 参考 | 中（仅算法） |
| 19 | **Spatialite** | C | 地理空间 SQL | 🟡 MPL 三许可 | **已决策不配合** | 低（已决） |
| 20 | **OpenGeode** | C++17 | B-rep 框架 | 🟢 MIT | 参考 | 中低 |
| 21 | **arcs** | Rust | 2D ECS 架构 | 🟢 MIT/Apache-2.0 | 参考 | 低 |
| 22 | **CAD-main** | C++20 | 观察器 / ECS | ⚫ **无许可证** | 参考 | 低中 |
| 23 | **openctm** | C | 网格压缩 IO | 🟢 zlib 式 | 参考 | 低 |
| 24 | **DB-CAD** | C++/Qt | CAD 数据层 | ⚫ 无 | 参考 | 低 |

---

## 2. 图谱的五个集群

### 2.1 几何内核 / NURBS（6 个）
`LNLib` · `gbs` · `qaws` · `cadcore` · `CZY3DKernel` · `OpenGeode`

四个 NURBS 库里，真正有区别的只有两个：
- **cadcore**：唯一 OCCT-free 的精确解析内核，slotmap arena + AP214 STEP 写出器，`CLAUDE.md` 是少见的严谨写作，直接适用于任何要输出水密 STEP 的引擎。
- **CZY3DKernel**：绑定 OCCT ≥7.2 的算法层，AAG 特征识别 + 自研多面体 Boolean。
- **LNLib / gbs / qaws**：同质化竞争。宿主已对 LNLib 明确决策"不集成"；gbs 算法文档最扎实且是 header-only，但 LGPL-3 + C++23；qaws 有**唯一的 GLSL/HLSL/Halide 后端**（同一份求值代码能编进细分/顶点着色器）——但它没有许可证。

### 2.2 渲染 / 场景图（5 个）
`gl_cadscene` · `VulkanSceneGraph` · `OpenCADStudio` · `CAD-main` · `Quake3e`

**三条互斥的渲染栈**：VSG 是 Vulkan-only，OpenCADStudio 是 wgpu（它的 `gl` 是 GLES 转译，**不是桌面 OpenGL**），`gl_cadscene` 是**唯一真桌面 OpenGL**。宿主是 OpenGL，所以**只有 gl_cadscene 的代码能落下来**。

gl_cadscene 的 README 直接讨论了双精度 CAD 矩阵，建议把 double 的 view-projection × double 的 world 矩阵拆开、只吐 float（README.md:197）——正好对上宿主的重定基管线。

VSG 与 gl_cadscene 解决同一个问题（多零件装配的 draw call / 状态削减）但路子相反：VSG 保留可移植的 Object/Visitor/Bin/LOD 层级并用 RenderGraph 编译遍历，gl_cadscene 则刻意用扁平 NodeTree + 并行数组把这个层级换掉换速度。**两边都值得读，正好是宿主 acgs 层架构选型的两面。**

### 2.3 网格 / 资产 IO（6 个）
`meshoptimizer` · `embree` · `tinygltf` · `stb` · `openctm` · `mcut`

`meshopt_buildMeshlets`（法锥加权聚簇）产出簇，正是 Embree 用 BVH4/BVH8 节点打包消费的东西——"先抽稀+聚簇，再打进 Embree 场景"是一条成立的链路。openctm 是 2009 年停更的老格式，在现代管线里已被 glTF + Draco + meshopt 取代。

### 2.4 鲁棒性与空间索引（3 个）
`predicates` · `KrisLibrary` · `Spatialite`

宿主只真正接入了 `predicates`。空间索引的取舍决策已写在 `docs/technical-plan.md` 4.2 / 4.3：Octree 与 SQLite 不混合、SpatiaLite 的 R-tree 与 kd-tree 不叠加。

⚠️ **符号冲突**：`mcut/source/shewchuk.c` 内置了和 `predicates` 完全相同的公开域谓词。同时链接会在 `orient2d`/`insphere` 上撞名。

### 2.5 大坐标精度（1 个）
`large-coordinate-rendering` — 单点但与宿主主线强耦合。

---

## 3. 宿主接线现状（图谱的"边"）

| 关系 | 对象 | 状态 | 依据 |
|---|---|---|---|
| **已编译** | `predicates` | active | `CMakeLists.txt:201-215` |
| **首要设计参考** | `OpenCADStudio` | active | `technical-plan.md:12`；另有两份专门移植文档 |
| **技术同源** | `large-coordinate-rendering` | active | 与宿主 Rebase+RTE 是"同一种思想的两个层面" |
| **验证基准** | `simpline` | active | 宿主已自行实现（Thomas 三对角 + Sherman-Morrison + 24 点 Gauss-Legendre） |
| **决策不集成** | `LNLib` | closed | `technical-plan.md:112-119` |
| **决策不配合** | `Spatialite` | closed | `technical-plan.md:126-128` |
| **候选** | `gl_cadscene` / `embree` / `meshoptimizer` | candidate | 文档未点名，但问题域高度重合 |

### 关于大坐标这条边，值得单独说

`large-coordinate-rendering` 提出 **Split-Translation RTE**：把 modelMatrix 拆成 R（旋转/缩放，进 GPU）和 T（平移，留在 CPU 用 double 算），shader 里

```glsl
modelNoTranslation[3].xyz = vec3(0.0);
worldNoTranslation = modelNoTranslation * vec4(localPosition, 1.0);
relativePosition      = worldNoTranslation + uDemoRelativeWorldPosition;   // = objectWorld - cameraWorld
mvPosition            = uDemoViewRotationMatrix * vec4(relativePosition, 1.0);
```

宿主的 `lib/coordinate/WorldRebase.h` 做的是**同一条路的强化版**：chunk 锚定的重定基原点（`kDefaultChunkSize = 1.0e4`，相机跨过 chunk 边界时原点正好跳一个 chunk），所以无论相机飞离世界原点多远，每个物体 uniform 都被限制在 `chunkSize/2` 以内。

**读这张图谱时要看差值而不是重合度**：该 demo 只做到 rebase + RTE，明确没有实现 Cesium 的高低精度拆分和矩阵拆分；而 chunk 锚定是宿主独有的、更强的一层。

---

## 4. 风险登记册

| ID | 级别 | 风险 | 涉及 |
|---|---|---|---|
| R1 | 高 | **五个项目无许可证文件**，来源确认前只能读不能 vendor | qaws · simpline · CAD-main · CZY3DKernel · DB-CAD |
| R2 | 高 | **四份拷贝被裁剪/改版**，无法验证来源与可构建性；embree 尤其可疑 | tinygltf · stb · meshoptimizer · **embree** |
| R3 | 中 | 同质化重复 + 符号冲突 | 4 个 NURBS 库 · 3 份 DCEL/半边 · 3 套空间索引 · 3 份 Shewchuk |
| R4 | 中 | 许可证分级误用 | 见 §1 图例 |
| R5 | 中 | **OCCT 路线分叉未决**：引 OCCT 还是自研内核？ | CZY3DKernel vs cadcore |
| R6 | 低 | stb_image 不支持 DDS，资产管线若出 DirectX 表面格式缺路径 | stb · tinygltf |
| R7 | 低 | 24 进 1 出，23 个未接线且无引入理由注释 | 整个 `external/` |

### R2 里 embree 的核查细节

`.ispc` 文件**只存在于 `tutorials/`**；没有 `kernels/traverse/`、没有 `kernels/geom/`；`kernels/CMakeLists.txt` 里 **零** 个 "ispc" 引用；`INSTALL.md` 缺失。它引用的 `.cpp`（如 `bvh_intersector_hybrid4_bvh4.cpp`）确实存在，所以是"看起来能编"但**没有证据证明这份拷贝能构建**。

---

## 5. 建议的下一步

1. **给 `external/` 补一张引入台账**（每个目录一行：为什么引入、判定用/不用、看的是哪个文件）。R7 是根因——没有台账，下一个人只能重新扫一遍 24 个目录。
2. **五个无许可证项目二选一**：确认来源后补 LICENSE，或直接删除。悬着最糟——它看起来像可用资产。
3. **embree 先验证可构建性再决定去留**。它是 CPU 精确拾取的真替代方案（对宿主当前的 GPU ID-buffer 拾取），价值最高，但这份拷贝的完整性存疑。
4. **OCCT 分叉做一次决策**（R5）。把 CZY3DKernel 和 cadcore 并排读一遍是成本最低的判断方式——一个绑定 OCCT，一个明确自称 OCCT 替代品。
5. **gl_cadscene 优先读**（不只读）。它是唯一的真 OpenGL 目标，且 Apache-2.0 可安全借鉴；draw call 削减 + Hi-Z 遮挡剔除 + 双精度 CAD 矩阵拆分三条都直接对上宿主的问题。

---

## 6. 方法与局限（诚实交代）

**没有用 graphify 生成。** graphify 技能流水线需要 bash + Python，本环境的 bash 工具每次调用都直接失败：

```
Fatal error. Your Windows doesn't fully support CET.
Command exited with code 2148734214
```

命令根本没执行，所以 Python 从未启动。另外 `.graphifyignore` 第 2 行就排除了 `/external/`，现有的 `graphify-out/graph.json` 也不覆盖这棵树。这份图谱是用 `read` / `glob` / `grep` 手工建的等价物：主代理 + 4 个并行只读子代理，外加主代理直接分析 6 个项目。

**枚举方法**：按深度 2 的标志文件扫（`README*` / `LICENSE*` / `COPYING*` / `CMakeLists.txt` / `*.md` / `*.c` / `*.cpp` / `*.h` / 构建清单 / dotfile 等）。**一个不含任何这类标志文件的目录可能被漏掉**——24 个已发现项目都有已核实的证据。

**其他局限**：
- `large-coordinate-rendering` 的 LICENSE 内容本次未核实，只确认文件存在。
- `gl_cadscene` 的 C++ 标准未声明（CMakeLists 只写了 `cmake_minimum_required(VERSION 3.5)`）。
- embree 的 C++ 标准未在顶层 CMakeLists 中找到。
- 子代理对各项目的证据路径记录在 JSON 的 `evidence` 字段里，可回溯核对。