# external/ 之外的开源库选型：按算法缺口对位

> 生成日期：2026-10-12
> 配套文档：[algorithm-porting-plan.md](./algorithm-porting-plan.md)（缺口矩阵）、[external-project-map.md](./external-project-map.md)（现有 24 库）
> 方法：以 `algorithm-porting-plan.md` 的 37 项缺口为索引，逐条检索候选库，**核实许可证与作者自述的质量警告**后再入选。
> 本机 `bash` 失效（`Fatal error. Your Windows doesn't fully support CET`，exit `2148734214`），**本次全部为静态检索，未编译验证**。

---

## 0. 结论摘要

现有 `external/` 24 库的共同短板：**它们全部是"几何算法参考实现"，没有一个是"可直接链接的工程库"**。LNLib/gbs/qaws/CGAL 类库是 header-only 教学/研究实现，CAD-main/KrisLibrary 是完整应用而非可复用内核。因此移植计划的正确姿势是"读算法、按宿主架构重写"，而不是"加依赖"。

真正值得**作为依赖引入**的，是下面这 8 个——它们填补的缺口，现有 24 库一个都没覆盖，且是工程级成熟度：

| 优先级 | 库 | 填补的缺口（对应移植计划条目） | 许可证 | 建议 |
|---|---|---|---|---|
| **P0** | **nanoflann** | kd-tree / 有界 KNN（Tier 1 #12） | BSD-2 | 直接链接 |
| **P0** | **Clipper2** | 2D 多边形布尔 + 偏移 + 自交修复（Tier 2 #20/#22） | **BSL-1.0** | 直接链接（**但 CDT 模块别用**） |
| **P0** | **btDbvt**（Bullet 单文件） | 动态 AABB 树：insert/remove/update（`geoctree.h` 缺 remove/update） | zlib | 抄算法，不整库引入 |
| **P1** | **STEPcode**（NIST SCL） | **STEP reader**（宿主 StepWriter 只有 writer） | BSD-3 | 直接链接（只当 Part 21 语法层） |
| **P1** | **libdxfrw** | DXF/DWG 读写（technical-plan P2 已在规划） | GPL-2+ | ⚠️ 商用需评估，见 §3.3 |
| **P2** | **HarfBuzz + FreeType** | `acgi` 完整 Unicode / OpenType（256 项定长表不够） | MIT / FTL-GPL | 直接链接 |
| **P2** | **GTE**（Geometric Tools） | 精确最近点 / Voronoi-region 分类（Tier 1 #13） | Boost-1.0（免费部分） | 抄算法 |
| **P3** | **libigl（winding number 子集）** | 非水密网格的内外判定 | MPL-2 | 只抄 `fast_winding_number` |

**明确不建议引入**：CGAL、OCCT、Triangle、manifold、geogram（整库）。理由逐条在 §3，每条都和宿主的具体缺口有关，不是泛泛的"太大"。

---

## 1. P0 —— 建议直接引入

### 1.1 nanoflann：kd-tree（对应 Tier 1 #12）

**为什么现有库不够**：KrisLibrary 有 `geometry/KDTree.h`，LNLib 有 `OctreeCollector.h:31-52`（`KnnResultSet(capacity)` / `RadiusNnResultSet(radius)` / `WorstDist()`）——移植计划本来打算从这两处取。但 **nanoflann 是更好的工程选择**：

- **单头文件**（`nanoflann.hpp`），可直接 drop 进 `lib/ge/`，与 `lib/ge/` 全 inline 的风格一致（移植计划 §8 刚提过要给 `lib/ge` 引入 TU 承载常量，nanoflann 免掉这个问题）
- CRTP + 内联替换了 FLANN 的虚函数派发，比 FLANN 快；`radiusSearch()` 一次调用直接出 STL 容器，不需要先问容量
- **索引可存盘**（`saveload_example.cpp`）——acdb 已有 SQLite 持久化 + ops journal，kd-tree 索引落盘是自然的延伸
- `NANOFLANN_FIRST_MATCH` 宏给出确定的 tie-break（同距按下标最小），对可复现构建很重要

**宿主要做的适配**：nanoflann 要求数据**在查询期间不可变**（这与宿主 `acdb` 的事务模型直接冲突——`AcDbTransaction` 正在改实体时查询会失效）。解法是照它 README 的官方建议：**数据在事务期间变化时复制一份**，或按 entityRevision 做 double-buffer 索引。这不是移植能省的，是架构约束。

**来源**：https://github.com/jlblancoc/nanoflann （官方；搜索结果里大量 `*/fork` 镜像站不可信，以官方为准）

### 1.2 Clipper2：2D 多边形布尔（对应 Tier 2 #20/#22，Tier 3 #28）

**为什么现有库不够**：external 里**没有任何一个库做 2D 多边形区域布尔**。LNLib/gbs 只做曲线求值；mcut 是 3D BSP 切割；CZY3DKernel 的布尔是 3D 硬编码相位。而宿主 `lib/ge/ge2d.h` 只有 line/circle/arc 的**两两求交**（7 个），**没有区域级运算**。

**Clipper2 补的是这一整块**，而且是宿主最缺、又最容易见效的一块：

- `Intersect / Union / Difference / Xor`，支持**自交多边形、重叠边、共线顶点、嵌套孔**——这些正是 2D 草图/剖面/图案填充每天遇到的
- **整数坐标（int64）内核**：`Paths64` 在 ±(2^62−1) 范围内做精确整数运算，**完全绕开浮点容差**。这与宿主刚要建的容差阶梯（Tier 0 #1）是互补的——整数域根本不需要 eps
- `InflatePaths` 偏移（miter/round/square join + end type）直接对应 Tier 2 #20「2D 平行曲线的 cusp / 自交修剪」，而且 **`ClipperBase::FixSelfIntersects` 已经实现了修剪**——这是 qaws 要到 #20 才拿到的能力
- `PolyTree` 输出自带**环嵌套关系**（外环/孔），正好能喂给宿主 StepWriter 缺失的 `FACE_OUTER_BOUND` 多环表达

**⚠️ 必须知道的两个警告**（我核实了官方 README，不是道听途说）：

1. **CDT 模块有 bug，作者自己这么说**：README 原文 *"CAUTION: The triangulation code is buggy and I regret releasing it when I did. I'm working on fixes but these may still be weeks away."* —— 所以 **Tier 3 #28「约束 Delaunay + 曲率驱动加密」不要用 Clipper2 的 CDT**，仍走 gbs `inc/topology/tessellations.h`。
2. **C++17 依赖**：官方要求 C++17。宿主 `lib/ge/` 是全 inline 头文件，C++17 不成问题，但要确认宿主主工程标准版本。

**许可证**：**BSL-1.0（Boost Software License 1.0）** —— 可商用、可闭源、可改，唯一要求是分发源码时保留版权声明。这是本清单里最宽松的几何库之一。`CSharp/Clipper2Lib/HashCode.cs` 另有 MIT/BSD-2 的 xxHash32，与宿主 C++ 无关。

**来源**：https://github.com/AngusJohnson/Clipper2 ；许可证原文 https://angusj.com/clipper2/Docs/License.htm

### 1.3 btDbvt：动态 AABB 树（补 `geoctree.h` / `gebvh.h` 的 remove/update 缺口）

宿主 `lib/ge/gebvh.h` 已有 refit 但注释自述"质量优雅退化"，`lib/ge/geoctree.h` **完全没有 remove/update API**——移植计划 §2.2 记的。而 `acdb` 的事务模型 + `AcGsSelectionManager` 意味着**几何频繁增删**，静态 BVH 每次全重建是不可接受的（`BRepRayCast.h` 现在就在每次 rayCast 内部重建 BVH）。

**btDbvt 是这个问题最成熟的答案**（Bullet3 的 `btDbvt.cpp`，单文件、零依赖、zlib）：

- `insert / remove / update(node)` / `update(node, volume, velocity, margin)` 四件套齐全
- **velocity 预测更新**：`update(node, volume, velocity)` 会按运动方向单向扩 AABB（`SignedExpand`）——这正好对接宿主的 `AcGsOrbitCamera` 拖拽时的连续拾取
- **lookahead 参数控制重插深度**：`lookahead=0` 只重插到父节点（改动最小）；`lookahead=-1` 重插到根（最大化重平衡）。宿主可以在"刚编辑完一个实体"时用 0，在"空闲帧"用 `optimizeIncremental(passes)` 分摊重平衡成本
- **`remove` 是真正删除叶子并提升兄弟节点**，向上传播 AABB 时**遇到包围盒未变就停**——这个早退传播是性能关键，宿主自己写容易漏
- `rayTestInternal` 用**常驻栈 + 预计算符号/倒数方向**，避免 `rayTest` 的动态分配

**建议抄算法而非整库引入**：Bullet 的 `btDbvt` 依赖 `btVector3` / `btAlignedObjectArray` / `SIMD_FORCE_INLINE` 宏，剥掉这些要改不少。移植计划里 embree 的 refit/rotate 路线（Tier 1 #17）也可以，但 btDbvt 的 insert/remove/velocity-update 三件套是 embree 静态构建里没有的——**两者互补，不是替代**。

**来源**：https://github.com/bulletphysics/bullet3/blob/master/src/BulletCollision/BroadphaseCollision/btDbvt.cpp ；API 全表见 ROS 文档 `structbtDbvt.html`

---

## 2. P1 —— 功能性的，取决于路线选择

### 2.1 STEPcode：STEP reader（宿主只有 writer）

`lib/brep/StepWriter.h` 是**只出不进**的：单 `FACE_OUTER_BOUND`、硬编码 `same_sense=.T.`、无孔、无 B_SPLINE_SURFACE、**无 reader**。这是移植计划 §2.1 明确记的缺口。

**STEPcode（= NIST STEP Class Library SCL 的后继）** 是 Part 21 的**语法/数据模型层**，不是几何内核：

- 读 ISO 10303-11 EXPRESS schema → 生成 C++ 类 → 读写 Part 21 文件
- 覆盖 AP203 / AP210 / AP214 / AP238 数据模型
- 被 BRL-CAD、OpenVSP、SCView 等生产项目实际使用

**关键定位**：它**只解决"把文件读成结构化实体树"**，`ADVANCED_FACE` 里的几何解释、`FACE_OUTER_BOUND` 怎么变成宿主的 Loop/CoEdge/Edge，仍要靠宿主自己的 `BRepBuild.h`。所以它是**补 IO 前半段**，不是替代宿主的 B-Rep 内核。

**⚠️ 坑**：`fedex` EXPRESS→C++ 代码生成链是它的老本行，构建需要跑 `fedex`；且**上游活跃度低**（近年提交以 CMake/文档修补为主）。评估工作量时要把生成链的坑算进去。

**替代方案**：要"能用的 STEP reader"而不想要 EXPRESS 工具链，**直接上 OCCT**（`STEPControl_Reader`，工业级，被 FreeCAD/CadQuery/Gmsh 广泛使用）代价更大但更稳——见 §3.1 的取舍。

**来源**：https://github.com/stepcode/stepcode ；文档 https://stepcode.github.io/docs/home/

### 2.2 libdxfrw：DXF/DWG 读写

`docs/technical-plan.md` 的 P2 已经把「DWG-DXF 诊断」排进优先级了，但宿主目前**没有任何 DXF/DWG 代码**。libdxfrw 是这个领域的事实标准（SourceForge 起家，跨平台，CMake + autotools + VS 工程齐全）：

- DXF 读写（ASCII + 二进制），R12 → V2015+
- **DWG 读取** R14 → V2015
- 30+ CAD 实体类型 + 图层/线型/样式完整对象体系
- **内置字符编码转换**（`drw_textcodec.cpp`），内部统一转 UTF-8；亚洲图纸 CP936/CP950 是常见坑，这个库替你踩过了
- 自带 `dwg2dxf` CLI，可直接当批处理工具

**接口模式值得注意**：它**不替你存数据**，是 `DRW_Interface` 回调（解析到一个实体喊一声"来了一条 LINE!"）。这与宿主 `acgi` 的 traits 语义绘制协议是同一种哲学，接入成本低。

**⚠️ 两个必须评估的点**：

1. **GPL-2+**，不是 BSD/MIT。链接进宿主是否触发 copyleft 需法务判断。
2. **能力上限到 V2015**（AutoCAD 2015，2013 年）。更现代的 DWG 版本需要另找方案。

**一个实用的高频坑**（来源里反复强调）：`read(&iface, ext)` 的第二个参数决定是否按 extrusion 方向做 2D 投影。传 `false` 拿原始三维坐标；传 `true` 会把斜面/立面上的圆弧按所在平面投影成 2D，**用错会导致圆心半径"漂移"**。宿主接 acgi 时这个参数选错会出很难查的 bug。

**来源**：https://github.com/LibreDWG/libdxfrw （原 mirror 在 SourceForge `projects/libdxfrw`）

### 2.3 HarfBuzz + FreeType：完整 Unicode（补 `acgi` 的 256 项定长表）

宿主 `lib/text/text_render.h` 注释自述"256 项定长表不支持完整 Unicode"，且它是与 `text_font.h` **并行的旧 API**。而 `lib/imgui/misc/freetype/` 只有 ImGui 的**绑定胶水**（`imgui_freetype.cpp`），**FreeType 本体没有引入**。

- **FreeType**：字体栅格化与字形轮廓提取的事实标准，支持 TrueType/OpenType/Type1/CFF
- **HarfBuzz**（MIT）：OpenType 整形引擎，把 Unicode 码点序列变成正确的 glyph id + 位置。支持 Indic/Arabic/Thai/Khmer/Myanmar/Tibetan/Hangul/Hebrew/USE 通用整形模型 + Emoji 组合序列 + Graphite 2 + AAT + 可变字体 + **字体子集化**

宿主 `acgi/AcGiTextStyle.h` + `AcGiTextEngine` 走的是 AutoCAD SHX/bigfont 双字节路径，那是**为 CAD 字体优化的窄路径**，与 Unicode 世界没有交集。如果宿主要支持 CJK/emoji/RTL，HarfBuzz 是唯一现实选择——**自己实现整形不现实**。

**注意**：HarfBuzz 设计上跑在 FreeType 之上（`hb-ft`），两者是配套的。两者都不是 BSD 式的"随便拿"，HarfBuzz 是 MIT（最宽松），FreeType 是 FTL（带 GPL 双授权，嵌入动态库可按 FTL 用）。

**来源**：https://harfbuzz.github.io/what-does-harfbuzz-do.html

---

## 3. 明确不建议引入（附理由）

### 3.1 CGAL / OCCT —— 是"换内核"不是"补缺口"

这两个是**完整 CAD 内核**，引入意味着宿主 `lib/ge` + `lib/brep` 全部重写。这与"自研 CAD 引擎"的项目目标直接冲突。

**如果只是想要某个算法，正确做法是读算法**（这正是用户要求的角度）。典型例子：

- **CGAL `Arrangement_2`**：比 cadcore `arrangement.rs` 更完整的平面 DCEL，支持任意拓扑组合。这正是移植计划 Tier 1 #7（per-face uv DCEL arrangement）需要的——**去读它的插入算法与 traits 设计，不要链接**。
- **CGAL `Nef_polyhedron_2`**：selects 平面映射（每个面对象带布尔包含/排除标记），布尔运算 **O(n log n)** 且"complete，不需要一般位置假设"。这在数学上比任何近似方案都强——但它基于 extended kernel（需 gcd 或精确有理数），对宿主这种"浮点 + 容差阶梯"的架构是降维打击。**值得读的是它的 select plane map 数据结构**。
- **OCCT `BRepCheck_Analyzer`**：工业级 B-Rep 校验器，正是宿主 Tier 0 #2/#3（Gate 1/4）的成熟版。**读它的检查项清单**比读 cadcore 的 Rust 实现更容易照抄。
- **OCCT `XSControl_Reader` / `ShapeFix`**：STEP reader + shape healing 的工业方案，比 STEPcode 完整。

**⚠️ CGAL 许可证是硬伤**：**双授权**——基础层（Kernel/Support）LGPL-3+，**大部分几何算法包是 GPL-3+**。GPL 包一旦链接进宿主，宿主需整体开源。`Arrangement_2` / `Nef_polyhedron` / 多数 Meshers **都是 GPL**。商业授权需向 GeometryFactory 购买。所以对闭源产品，CGAL 的正确用法就是 §3.1 说的"只读算法"。

**⚠️ OCCT 许可证也比看上去窄**：LGPL-2.1 **+ 一个九行的例外文件**（`OCCT_LGPL_EXCEPTION.txt`）。该例外只授予"目标码可以包含库头文件里的材料"，解决的是 inline 头文件问题，**不豁免 LGPL 第 6 条的整体义务**（必须提供 OCCT 源码、告知用户有权修改 LGPL 库、静态链接/AppStore 分发时要单独提供可修改形式）。不要因为"有例外"就当成 MIT。

### 3.2 Triangle（Shewchuk）—— 数值质量最好，但**禁止商用**

`Triangle.c` 是约束 Delaunay + Ruppert 网格细分的标杆（1996 年，2003 Wilkinson 奖），且用的是宿主已经引入的同一套鲁棒谓词。**但作者在 README 写明：*"may not be sold or included in commercial products without a license"*** —— 学术免费，商用需单独授权。

geogram 的 license.txt 也印证了这一点，它捆绑 Triangle 时同样标注 *"free for academic use, contact Jonathan Shewchuk for other uses"*。

⇒ **对商业 CAD 产品：只能读算法、不能链接。** 宿主若要做约束 Delaunay，走 gbs（Tier 3 #28）或 Clipper2 的**裁剪**部分（避开 CDT）。

### 3.3 manifold / geogram —— 领域不匹配或授权偏重

- **manifold（elalish，Apache-2.0）**：保证流形的**三角网格**布尔（不是 B-Rep 布尔）。宿主是**精确 B-Rep 内核**，用它意味着多维护一套网格表示。移植计划 §6 已把 HACD（网格凸分解）判为"场景不匹配"，同一理由适用于此。它的价值在"拿到非流形网格输入后的修复"，属于可选工具而非内核能力。
- **geogram（BrunoLevy，BSD-3）**：几何处理全家桶（surface reconstruction / remeshing / 参数化 / 精确数 / CSG / 平行 CDT）。功能上确实有宿主缺的（**支持相交约束的多线程 CDT + 任意精度**比 gbs 的星形环 CDT 强）。但它带 OpenNL / PoissonRecon / xatlas / Lua 等一大堆捆绑，**且其中 PoissonRecon 是 BSD、xatlas 是 MIT，而 libFINT? 混杂**。只为一两个算法拖整库进来不划算。**它的 Pluggable Software Module 机制**（把子集抽成独立 (header, impl) 对，零依赖）值得借鉴——**宿主自己的 `lib/ge/` 全 inline 结构其实就是同一个思路**。

---

## 4. 一张速查表

| 缺口 | 移植计划条目 | 推荐 | 许可证 | 结论 |
|---|---|---|---|---|
| kd-tree / KNN | Tier 1 #12 | **nanoflann** | BSD-2 | ✅ 引入 |
| 2D 区域布尔 | 无（新） | **Clipper2** | BSL-1.0 | ✅ 引入（避开 CDT） |
| 2D 偏移 / 自交修剪 | Tier 2 #20 | **Clipper2** `InflatePaths` + `FixSelfIntersects` | BSL-1.0 | ✅ 引入 |
| 八叉树 remove/update | §2.2 | **btDbvt** | zlib | 📖 抄算法 |
| 几何增删的动态 BVH | Tier 1 #17 | **btDbvt**（与 embree refit 互补） | zlib | 📖 抄算法 |
| STEP reader | §2.1 | **STEPcode** | BSD-3 | ✅ 引入（仅 Part 21 层）|
| DXF/DWG | technical-plan P2 | **libdxfrw** | GPL-2+ | ⚠️ 先法务 |
| 完整 Unicode 文本 | §2.3 | **HarfBuzz + FreeType** | MIT / FTL | ✅ 引入 |
| 精确最近点 / Voronoi 分类 | Tier 1 #13 | **GTE** | Boost-1.0 | 📖 抄算法 |
| 非水密网格内外判定 | 无（新） | **libigl fast_winding_number** | MPL-2 | 📖 抄算法 |
| DCEL arrangement | Tier 1 #7 | CGAL `Arrangement_2` | **GPL-3+** | 📖 只读 |
| B-Rep 校验项清单 | Tier 0 #2/#3 | OCCT `BRepCheck_Analyzer` | LGPL-2.1+例外 | 📖 只读 |
| 约束 Delaunay | Tier 3 #28 | **Shewchuk Triangle 禁商用** | 非商用 | ⛔ 只读算法 |
| mesh 布尔 / 凸分解 | §6 已否决 | manifold / HACD | Apache-2.0 | ❌ 领域不符 |

---

## 5. 与移植计划的关系

**这份清单不替代 `algorithm-porting-plan.md`，而是给它补两类东西**：

1. **计划里 37 项的更优来源**。#12 kd-tree（nanoflann 优于 KrisLibrary/LNLib）、#20 平行曲线修剪（Clipper2 已有，qaws 那半可以省）、#13 最近点（GTE 的 Voronoi-region 分类法比 PQP 更适合宿主需要"分类"而不只是"距离"的场景）。
2. **计划里没有的新缺口**。2D 区域布尔、动态 AABB 树、STEP reader、非水密网格内外判定——这四项在 24 库里**一个都没有对应物**，是本清单的净增量。

**建议动作顺序**（与移植计划的三批次并行）：

- **立刻可做，不依赖 bash**：把 nanoflann / Clipper2 / btDbvt / STEPcode 四个库下载到 `external/` 并写 `CMakeLists.txt` 接线（宿主 CMakeLists 目前只编 `predicates` 一个 external 项目，接线模式现成，见 `CMakeLists.txt:201-215`）
- **与移植计划第 1 批并行**：STEP reader 先落 IO 层，Tier 0 的校验闸用 OCCT `BRepCheck_Analyzer` 的检查项清单校准覆盖面
- **bash 恢复后验证**：本清单所有条目**均未编译**，Clipper2 的 C++17 要求、nanoflann 与事务模型的冲突、btDbvt 剥离 Bullet 依赖的实际改动量，都需要真实构建才能确认

---

## 6. 诚实交代

- **本次全部为静态检索**：本机 bash 失效，任何库都没有下载、编译或跑过测试。表中许可证与警告均来自官方 README / 官方文档 / 官方许可证文本页，不是二手转述。
- **已核实的警告有三条**（都是作者自述，非我推测）：Clipper2 CDT 模块 bug（作者原文 "buggy and I regret releasing it"）、Shewchuk Triangle 禁止商用（作者原文 "may not be sold"）、libdxfrw 上限 V2015。
- **未核实项**：nanoflann 官方仓库地址在搜索结果里被大量 fork 镜像站淹没，我按 `jlblancoc/nanoflann` 给出，**落地前建议直接访问 GitHub 确认真实 owner**；STEPcode 的 fedex 构建链实际耗时未验证；btDbvt 剥离 Bullet 依赖的改动量是估计值，未实测。
- **许可证判定不是法律意见**：libdxfrw 的 GPL-2+ 与 CGAL 的 GPL-3+ 是否触发 copyleft，取决于宿主的分发形态（是否动态链接、是否改动了库本身）。**闭源商用前请走法务**。