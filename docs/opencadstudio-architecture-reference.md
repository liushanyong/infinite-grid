# OpenCADStudio 架构与算法技术参考

> 面向 E:/infinite-grid（SDL3 + bgfx, Rebase + RTE）的借鉴式技术文档。
>
> - 分析对象：https://github.com/HakanSeven12/OpenCADStudio （main，2026.37.0）
> - 关联引擎：https://github.com/HakanSeven12/cadcodec （acadrust，DWG/DXF 编解码）、https://github.com/HakanSeven12/cadkernel （几何内核）
> - 文档快照：2026-09-20。上游项目更新频繁，版本号/模块名可能漂移，本文以"架构模式与算法思想"为主要价值。
> - 许可提示：OpenCADStudio 为 GPL-3.0；acadrust/cadkernel 为 MPL-2.0。**逐行翻译其源码属于衍生作品，需继承相应许可证；本工程应只借鉴架构与算法思路并独立实现。**

## 1. 概述

OpenCADStudio 是一个纯 Rust 编写的跨平台 CAD（2D 制图 + 3D 实体建模），桌面与浏览器共享同一编辑内核。其价值不在于语言，而在于以下工程决策：

1. 原生 DWG/DXF 编解码（R12–R2018+）与 ACIS SAT/SAB 处理，不依赖任何商业 SDK；
2. 分层几何内核：2D 曲线代数 → B-rep → ACIS 提升/降落，全部按"解析解优先、收敛细分兜底"实现；
3. "文档模型 / 场景模型 / GPU 批次"三级分离，图元到渲染有且仅有三条 tessellation 路径；
4. 面向老 GPU 的可降级渲染（后端探测、能力探测、packed fallback）；
5. 进程外插件 + 版本化 ABI + 共享内存快照；
6. 桌面 / WebAssembly 同构，差异收敛在 sys/io 边界与并行 shim。

规模参考：主仓约 630 个 Rust 源文件 / 17.5 MB；acadrust 约 265 个源文件 / 7.5 MB；cadkernel 约 110 个源文件 / 2.3 MB。本工程不必照搬其全部规模，而是**复用其边界划分与算法骨架**。

## 2. 总体分层架构

OpenCADStudio 的逻辑分层（本工程应保留同样的依赖方向：上层依赖下层，下层不感知上位概念）：

    ┌──────────────────────────────────────────────────────────┐
    │ L5 UI / 交互层                                            │
    │     Ribbon 模块 | 命令注册表 | 输入/捕捉/追踪 | 状态栏 | 对话框 │
    ├──────────────────────────────────────────────────────────┤
    │ L4 应用 / 文档层                                           │
    │     Document | Undo/Redo | 图层/块/样式/布局 | 打印样式表    │
    ├──────────────────────────────────────────────────────────┤
    │ L3 场景层                                                  │
    │     Scene → RenderObject 转换 | 渲染缓存 | 拾取 | 相机/视口 │
    ├──────────────────────────────────────────────────────────┤
    │ L2 几何 / 内核层                                           │
    │     entity_curve 单源几何 | 细分容差 | cadkernel(geom2d/brep) │
    │     约束求解 | offset | ACIS lift/lower                     │
    ├──────────────────────────────────────────────────────────┤
    │ L1 编解码层                                                │
    │     acadrust: DWG/DXF/ACIS | STL/OBJ/LandXML 导入 | 导出      │
    ├──────────────────────────────────────────────────────────┤
    │ L0 运行时 / 平台层                                         │
    │     渲染后端探测 | 字体 | 系统集成 | 插件 IPC/SHM | CLI/无头 │
    │     wasm/web shim | 文件系统抽象                             │
    └──────────────────────────────────────────────────────────┘

关键数据流（打开文件 → 上屏）：

    file bytes → acadrust CadDocument
              → 编辑/undo（文档是唯一事实来源）
              → Scene 转换（按实体产出 RenderObject，命中缓存则复用）
              → 分批次（wire 线批 / mesh 三角批 / glyph 文字批 / hatch 批）
              → 后端管道（wgpu ↔ 本工程 bgfx）→ 呈现

对应到本工程（infinite-grid）的映射：

| OpenCADStudio | 本工程现有位置 | 说明 |
|---|---|---|
| Scene / viewport 相机 | lib/camera | 已有轨道/FPS 相机，缺"正交 CAD 工作平面"语义 |
| 大坐标 CPU double + 局部帧 | lib/coordinate/WorldRebase.h | 完全同构，可作为 L2/L3 精度策略继续延伸 |
| RendererBackend / 管道批次 | lib/rendering | 按pass绘制，需从"画图形"升级为"画实体批次" |
| 无对应（文档/几何/编解码/插件） | 新增 lib/domain lib/geom lib/io lib/plugin | 本文第 11 章给出目录规划 |

## 3. 数据模型设计（L4 / L1 之间）

### 3.1 文档即唯一事实来源

- 文档对象（CadDocument）持有实体集合、层表、样式表、块表、布局与字典；界面、undo、场景、插件看到的是同一份对象图或只读快照。
- 实体为"扁平枚举 + 值结构"（约 48 种 EntityType），不是深继承树：线、圆、弧、样条、多段线、尺寸、文字、块引用、面/网格、Solid3D 等各为独立 struct。
- 实体引用与流转用 handle/item 标识，避免裸指针穿透 IPC 与 undo。

对本工程建议（C++）：
- 采用 std::variant&lt;Line, Arc, Circle, Spline, Polyline, ...&gt; 或 "type enum + 连续存储 + SoA 列式表"，优先后者以获得缓存友好与批量 tessellation。
- entity_id 用单调递增 uint64 + 分代位（generation），删除后不复用；弱引用处保存 id+generation 校验。
- 图层/块引用采用"符号表"（interned string → symbol id），避免字符串在热路径重复比较。
- CadDocument 序列化字段尽量 POD/稳定布局，为共享内存快照与 undo 差分预留空间。

### 3.2 扩展数据（XDATA）原则

插件与行业信息不落旁路数据库，而是作为实体上的扩展键值（XDATA）随 DWG 往返。教训：**领域数据必须与几何绑定并随文档序列化**，否则插件状态在另存为/复制粘贴后丢失。

### 3.3 Undo 与变更通知

- undo 栈记录 Command（命令对象）而非原始 diff；命令可 do/undo/redo，并驱动订阅者重算。
- 文档变更通过通知（DocumentChanged / SelectionChanged / TabClosed 等）发给插件与 UI，避免轮询。
- C++ 落地：Command 接口保持简单，先在编辑层做 Command，再考虑把撤销动作序列化发给插件。


## 4. 几何与算法（L2，本文核心借鉴点）

### 4.1 单源几何定义：entity_curve

OpenCADStudio 为每种曲线实体定义一次解析几何（entities::curve::entity_curve），所有消费方共用：

- 捕捉候选点（object snap）
- EXTRUDE / REVOLVE 的截面
- hatch 边界与裁剪轮廓
- 屏幕 tessellation

好处：屏幕上看到的圆与传入建模命令的圆来自同一份定义，几何永不双重实现、永不漂移。

C++ 对应接口草案（放入 lib/geom/EntityCurve.h）：

    enum class CurveKind { Line, Arc, Circle, Ellipse, Spline, Polyline };

    struct SampleRequest {
        double tolerance;          // 全局弦容差（屏幕像素→世界单位，见 4.2）
        bool   closeByBend;        // 弯头处是否闭环加样
    };

    // 一次性产出：最近点、参数点、切线、分段采样多边形
    class EntityCurve {
    public:
        virtual glm::dvec2 point_at(double t) const = 0;
        virtual glm::dvec2 tangent_at(double t) const = 0;
        virtual double nearest_t(const glm::dvec2& p) const = 0;
        virtual std::vector<glm::dvec2> sample(const SampleRequest&) const = 0;
        virtual CurveKind kind() const = 0;
    };

    // 工厂：实体 → 曲线；这也是"实体几何唯一出处"
    std::shared_ptr<EntityCurve> entity_curve(const Entity& e);

### 4.2 自适应弦公差细分

- 每帧把屏幕像素容差换算成世界单位弦容差（视口缩放相关），而非固定密度采样。
- 半径相关相对容差（CURVE_REL_TOL = 容差 / 曲面半径的比例），保证大圆柱与大圆的细分数量不会爆炸，且与圆/弧的线框细分一致。
- 弯头（bulge）处保留圆弧而不是直接折线；plinegen 语义决定展开方式。

角度细分步长估算（圆/弧，半径 r，容差 tol）：

    if (tol >= r) { n = 2; }
    else { n = max(3, ceil(2 * pi / (2 * acos(1 - tol / r)))); }

关键规则（作为 ADR，见 12）：**永远不要按固定点数采样曲线**。固定密度会产生两类故障——大半径抖齿、小半径浪费顶点；容差驱动的采样与相机动态绑定后，平移/缩放不需要重建文档，只需要重建渲染批次。

### 4.3 局部坐标系不变式（Large-coordinate precision）

cadkernel 的每个运算先把输入平移到"平移量唯一的局部坐标系"，计算完成再平移回去。这是面向测量坐标（如 x=1.2e6）的几何侧精度方案：

- 截断发生后不是孤立的显示错误，而是圆弧拟合退化为折线、曲线步进溢出、点在多边形内判定符号翻转等连锁失效；
- 局部帧把有效尾数保留给几何计算，边界一次修复、处处生效。

这与本工程的 Rebase+RTE 是同一种思想的两个层面，可以直接在文档里声明为全局不变式：

| 层面 | OpenCADStudio | 本工程 |
|---|---|---|
| 几何运算（CPU，double） | cadkernel 局部坐标系 | 建议每次运算以"局部帧"包裹输入输出 |
| 相机/视图（CPU） | double 相机、远离原点视锥 | 现有 camera 代码 |
| GPU 顶点（float32） | 相对坐标有界 | WorldRebase localize() |
| 批间隙不变量 | 数学等价 | 已有 Rebase+RTE 两层抵消 |

推论：**任何 GPU 可见坐标都必须经 WorldRebase::localize() 或等价的局部帧产生**；新增实体/批量渲染代码不得越过这一约束直接把世界坐标塞进 uniform。

### 4.4 求交/邻近点：解析解优先 + 细分收敛兜底

曲线求交的层次（cadkernel geom2d）：

1. 有闭式解的（线-线、线-圆、圆-圆等）直接用闭式解；
2. 其余曲线拆成"含有闭式解的片段"逐段求解；
3. 无法拆解的部分进行细分/迭代直到达到容差要求。

保证的是"最终收敛"而不是"固定步数近似"。最近点与包含判断复用同一条 dispatch：

- 最近点：曲线上离光标最近的点；
- 点在边界内：射线+穿越计数法，射线贴角点/贴边时从另一角度重试，而不是直接判错。

C++ 落地顺序建议：

    阶段 1：Line/Arc/Circle/Polyline-with-bulge 的闭式求交 + 稳定角点重试
    阶段 2：Ellipse、NURBS 的细分-细化融合
    阶段 3：offset / 短线链化 / 区域面积长度度量

### 4.5 内核分层与 Provenance（源字节保真）

cadkernel 把内核拆成可单独编译的能力层：

    acis    （ACIS 文档 lift 为 B-rep，或将 B-rep lower 回去）
      ↑
    brep    （自有可变拓扑、SSI、布尔、圆角/倒角）
      ↑
    geom2d  （曲线、求交、包含、细分）
      └ offset （多段线平行偏置）

两个关键不变式：

- **Local frames**（见 4.3）：一切运算边界平移。
- **Provenance**：每个 B-rep 节点记住"从哪个 ACIS 记录提升而来，是否被编辑过"。lower 时，未被编辑的节点按原字节写回，只重写被编辑/新建的节点。否则一次布尔会重写整个文件，并把内核无法表达的属性悄悄丢掉。

对 C++ 移植的含义：若接 OpenCASCADE，后者自带 BRep+布尔+STEP；但"未编辑节点原样回写 + 脏节点才重写"的 provenance 思想必须自己做，落在 ACIS 编解码与造型历史之间。

### 4.6 ACIS 实体三角化与降级路径

Solid3D/Region/Body/Surface 的渲染是"内核优先 + 兜底"：

1. 把 ACIS 的 SAT/SAB 提升为内核 Body；
2. 每个面在其自身参数空间（u,v）内三角化（唯一生成填充网格的路径）；
3. 内核无法表达的某个面 → 用定制的逐曲面 LOD 采样器兜底；
4. 如果任何面没有全部提升成功，网格被标记为 incomplete（缺一面墙看起来仍是"完整实体"，必须显式暴露）。

配套规则：
- 曲面细分密度采用半径相对容差，圆柱侧面数量与圆/弧线框细分保持一致；
- SWEEP / LOFT 目前只产出网格、不建 B-rep（把"网格结果"和"内核结果"作为不同能力等级处理，这一点值得照搬）。

## 5. 渲染架构（L3 / L0，重点借鉴）

### 5.1 三级模型与 RenderObject

    CadDocument（持久模型）
        │ 编辑/undo 时失效
        ▼
    Scene 转换缓存：按实体产出 RenderObject，仅脏实体重算
        │ 帧级 culling/batching
        ▼
    GPU 批次：wire / mesh / glyph / hatch，各自独立顶点布局

RenderObject 路由（scene/convert/tessellate.rs）把可绘制几何分为：

| RenderObject | 内容 | 对应 bgfx pass 建议 |
|---|---|---|
| Wire（Lines） | 线框、多段线、尺寸、块边界 | 一个动态顶点缓冲 + 线宽/颜色实例属性 |
| Mesh（fill_tris） | 3D 实体、面/区域、网格、表格填充 | 索引三角缓冲 + 逐实例 model/色 |
| Glyph（LFF stroked text） | 文字、标注、公差框 | 每个字形 or 每批文字的线段批 |
| Hatch | 填充图案 | 独立 compute/fragment 或专用管线 |

### 5.2 三条 tessellation 路径（图元→几何的唯一规定）

入口 tessellate_entity(实体) 分派：

| 路径 | 适用实体 | 输出 |
|---|---|---|
| A. 内核 B-rep 网格 | Solid3D、Region、Body、Surface | 解析曲面逐面三角化填充 |
| B. 曲线采样（entity_curve → 弦容差） | 线/圆/弧/椭圆/样条/多段线（非厚） | 折线 |
| C. 直接发射 | 其余：文字、块、标注、面/网格、表格 | 点/线段/三角形直接生成 |

三条路径的接口建议在本工程做成三个具体类（KernelMesher / CurveSampler / DirectEmitter），在编译期选择、运行期仅调度。

### 5.3 兼容性降级：后端探测 + 能力探测

OpenCADStudio 启动时逐后端在独立子进程里做最小化 GPU 冒烟测试（gpu-probe），避免驱动崩溃拖死主进程：

    candidate_backends = 平台候选（Win: dx12, vulkan, gl；Linux: vulkan, gl）
    上次崩溃的 backend 从 crash sentinel 排除
    for backend in candidates:
        子进程内：请求 device → 建 pipeline → 离屏渲染 → readback
        第一个全通过者胜出
    若全失败：软件适配器（WARP / llvmpipe / SwiftShader），慢但可用

同时读取适配器 limits：没有 shader storage buffer 的老 GPU 自动切换 packed 渲染器。hatch 的 GPU 填充管线因依赖顶点阶段只读 storage buffer（WebGL2 没有 VERTEX_STORAGE），在能力不足时整体跳过并绘制边界退化版本。

映射到 bgfx：bgfx 已跨 D3D11/12、Vulkan、GL，不需要自建后端子进程；但以下三条仍然值得引入：

1. 初始化前用一个离屏帧冒烟测试当前后端成功后再进入主循环；
2. 记录启动后端/崩溃 sentinel 到 settings，下次自动切换渲染后端；
3. renderer 能力查询（caps: drawIndirect / compute / instancing / lineWidth）驱动功能降级，而不是 if(平台)。

### 5.4 相机与深度：与本工程已有方案对齐

- 正交/透视相机在 CAD 下必须按"像素-世界"标定：正交尺寸随视口而变，透视按目标距离换算 chord tolerance（照第 4.2）。
- 本工程已实现 perspect/logDepth（infinite depth slab）与 Rebase+RTE；借鉴点在于让"弦容差换算、近平面深度 slab 调整、相机前线"进入同一个 PerFrameCamera 结构，类似现有 GridRenderData 的打包方式，保证渲染参数原子一致性。

### 5.5 拾取与命中

- 拾取候选只依赖 entity_curve 的最近点查询（4.1），保证"点选得到的就是屏幕画的那条线"。
- 每个 RenderObject 携带实体 id 回传，GPU picking 或射线拾取都回到 entity_id 层，避免几何重建。

## 6. UI / 交互与命令架构（L5）

### 6.1 命令注册表

- 文本命令（如 LINE、CIRCLE、REGEN）与 Ribbon 按钮、脚本、自动化共用同一命令注册表；命令带显式类型化参数，可被序列化。
- 模块化：src/modules/ 分 draw/annotate/modify/layers/properties/layout/model/manage/view/insert 等模块，各自向 registry 注册工具，宿主不写死工具列表。

C++ 建议接口：

    struct CommandContext {
        Document* doc;
        Scene* scene;
        ViewportState* viewport;
        std::vector<std::string>* log;
    };
    class Command {
        virtual std::string name() const = 0;
        virtual void execute(CommandContext&) = 0;   // 或返回 UndoableAction
    };
    class CommandRegistry {
        void add(const char* name, std::shared_ptr<Command>);
        std::shared_ptr<Command> find(const std::string& name);
        std::vector<std::string> names() const;
    };

这套接口同时被键盘、菜单、脚本和未来 MCP 调用——这是 OpenCADStudio 自动化能力成立的前提。

### 6.2 捕捉/追踪同样以几何单源为输入

- 端点、中点、圆心、象限点、最近点、交点都是 EntityCurve 上的参数化查询结果，缓存快照而非重建；
- 拾取优先、捕捉优先、相对/绝对坐标输入建议在 ViewportState/InputPending 的显式状态机中实现，而不是散落在窗口回调里。

### 6.3 Web 端兼容性副作用

单画布 Web 版把所有管理对话框做成画布内模态，导致桌面端也收敛为单窗口（计划 B）。借鉴意义：**对话框状态机独立于"窗口"实现，桌面多窗口与画布内模态只是两个 shell**。本工程若考虑未来 wasm（bgfx/Emscripten 可行），应现在就把 UI 状态与 SDL 窗口事件解耦。


## 7. 插件与扩展运行时（借鉴其进程隔离设计）

### 7.1 三层模型

    A 宿主（Host）      ：ice UI / Document / Undo / 命令行，自带核心 Ribbon
                           ↓ 本地 socket IPC（版本化协议）
    B 插件进程（Plugin）：宿主以 --plugin-runner 模式重新执行自身
                        ：动态加载插件 cdylib，2 个导出符号、版本握手
                           ↓ 纯语言 API
    C 引擎 crate（可选） ：无 iced/acadrust 依赖，可进 wasm/CLI 的领域算法库

硬规则（值得照搬）：

1. 宿主只认识契约类型，绝不 import 插件源码；
2. 引擎库不依赖 UI/渲染；
3. 插件运行在独立进程，崩溃只影响自己；
4. 协议用"长度前缀帧 + 最大 64 MiB + 每调用超时（默认 30 s）+ 每次 spawn 新 token"。

### 7.2 共享内存双缓冲快照

大文档不通过 socket 复制，而是双缓冲共享内存：

    host 写活跃段 → 段版本号 +1 → 插件读时校验版本
    │   ┌─ 段0 ─┐   ┌─ 段1 ─┐
    │   │ 文档快照 │   │ 文档快照 │   默认每段 16 MiB，可配置
    └─► └────────┘ ↔ └────────┘
        插件只读映射，宿主控制 publish/close

- 按 tab_id 键控；插件只读，宿主独占写，避免锁竞争。
- 快照结构尽量 POD/稳定布局，发布方 swap 活跃段并递增版本计数。

C++ 替代映射：

| 职责 | C++ 方案 |
|---|---|
| 动态加载 cdylib | LoadLibrary / dlopen + 两个导出符号 |
| 本机 IPC | 具名管道 / QLocalSocket / boost::interprocess message_queue |
| 帧化序列化 | FlatBuffers / Cap'n Proto（零拷贝优先） |
| 共享内存快照 | boost::interprocess 共享内存 + 双缓冲 + 原子版本号 |
| 超时/生命周期 | 子进程句柄 + 看门狗；dead child 在下一次 dispatch 时回收 |

### 7.3 版本化 ABI 的三道闸

1. 插件声明 API major 必须落在宿主支持的 [min, max] 区间；
2. 新字段/枚举只允许追加，保证旧宿主-新插件前向兼容；
3. 二进制混用场景校验编译器/依赖来源一致性；未来升级为 repr(C) vtable（真正的 C ABI，本工程 C++ 天然适用）。

### 7.4 插件市场与更新

- 仓库内的 registry.json 是"可发现插件清单"，运行时从 main 拉取；每条只含 repo/name/description。
- 安装 = 下载 GitHub Release 资产 + plugin.toml、校验 api_version 后再启用；升级/重装/卸载与启用开关全部落在"下次重启生效"边界。
- 本工程可先不建市场，只保留 registry + release 资产协议设计。

## 8. 无头与自动化接口（L0，强烈建议借鉴）

同一套编辑核心暴露三种无界面入口：

    OpenCADStudio --export in.dwg out.dxf        # 一次性转换
    OpenCADStudio --serve [--port 4242]          # 每行一个 JSON 的无头服务器（stdio/TCP）
    OpenCADStudio --mcp                          # MCP/JSON-RPC 端点，供 AI 客户端驱动

关键决策：**GUI 与无头路径共用同一种命令/文档 API，而不是另写一套转换器**——否则能力会长期分叉。C++ 落地：

- command 层（6.1）天然可序列化：命令行参数、JSONL 请求、MCP tool call 最终都落成 CommandContext 调用；
- 日志/错误经统一 event sink 回传 MCP/JSONL，GUI 只是其中一个 sink；
- 本工程的 main 控制台已存在，可在现有主循环上先用 CmdLine → 命令翻译做 verifier 脚本（类似现有 verify_camera_target.py 的自动化调用模式）。

## 9. 文件编解码与互操作（L1，成本最高、不可急于自研）

### 9.1 能力矩阵（acadrust 实测声明）

- DXF：ASCII/二进制读写，R12（AC1009）— R2018+（AC1032）
- DWG：原生二进制读写，R13（AC1012）— R2018+（AC1032）
- ACIS：SAT/SAB 解析与写出、B-rep 拓扑、实体历史与 primitive builder
- 容错：可选 failsafe 恢复 + 有界结构化诊断 + 读取统计
- 编码：约 40 种 code page 自动识别（2007 前后绘制文件兼容）
- 导入：STL/COLLADA/OBJ/glTF/FBX；导出：STL/STEP/PDF/CSV，加载 CTB/STB 打印样式

C++ 思路上必须明确：**这一层不要随手"我写个简单的 DXF parser"**。DWG 位流/句柄/循环冗余结构庞大（仅 acadrust 就 7.5 MB）。现实选项排序：

1. 混合方案：用 MPL-2.0 的 acadrust 编成 C ABI 库，本工程 C++ 调用（保能力、省工期）；
2. 商用 ODA Drawings SDK（功能最全，商用授权）；
3. LibreDWG（GPLv3；读为主，写出不完整/实验性）+ 自研 DXF 扩展；
4. 纯自研（参考 Rust 版算法推演，只适合长期投入）。

无论选哪个，都把编解码封装在本工程统一接口后：

    namespace io {
    struct DocumentSource { enum { Path, Bytes } kind; std::string path; std::vector<uint8_t> bytes; };
    Result<Document> open(const DocumentSource&);
    Result<void>   save(const Document&, const SaveOptions&);   // 版本目标 R14–R2018 等
    }

### 9.2 容错读取原则

- 读取器区分"硬错误/可恢复坏实体/警告"，恢复模式下坏实体进入诊断列表而不是整个文件失败；
- 未识别的版本/实体做 pass-through 保留，禁止静默丢弃。

## 10. 多平台策略（桌面 / Web，一码多端）

摘录 OpenCADStudio 的 native-vs-web 差异，本工程可按同样粒度划分：

| 维度 | 原生桌面 | Web | 本工程（bgfx）建议 |
|---|---|---|---|
| 窗口 | iced daemon 多窗口 | 单画布模态 | SDL3 原生；未来 Emscripten+bgfx 单画布 |
| 3D 内核 | cadkernel | 同内核（纯 Rust、无平台差异） | 内核必须无平台分支（OCCT 需要移动端子集） |
| 并行 | rayon | 顺序迭代 shim | TBB/tbb:: + 条件编译顺序退化 |
| GPU | Vulkan/DX12/Metal/GL | WebGL2（可用时 WebGPU） | bgfx 天然多端；保留能力降级 |
| hatch | 顶点阶段 storage buffer | WebGL2 无 VERTEX_STORAGE 故跳过 | IMGUI 无；自研管线做能力开关 |
| 字体 | 系统 TTF + 内嵌 LFF 笔画 | 仅内嵌笔画 | 桌面 FreeType/HarfBuzz，wasm 内嵌 LFF |
| 文件 | 路径读写 | bytes + 下载 | DocumentSource 抽象（9.1） |
| 插件 | 原生进程外 | 无 | 原生 only，web 显示升级引导 |

## 11. 对本工程（infinite-grid）的落地路线图

### 11.1 推荐新增的模块树

    lib/domain/
        Document.h            // CadDocument 等效：实体/层/块/符号表
        Entity.h EntityVariant.h EntityId.h Layer.h Block.h
    lib/geom/
        EntityCurve.h         // 4.1 单源几何
        Tolerance.h           // 4.2 像素→世界 弦容差
        Tessellator.h         // 4.1/5.2 采样器
        Frame.h               // 4.3 局部坐标系 invariant
        Offset.h              // 保护 offset 能力
        ContourOps.h          // 交点/包含/最近点
    lib/scene/
        Scene.h RenderObject.h Batcher.h Picker.h RenderCache.h PerFrameCamera.h
    lib/io/
        DocumentSource.h Codec.h DxfCodec.h DwgCodec.h Diagnostics.h
    lib/app/
        Command.h CommandRegistry.h UndoStack.h SnapEngine.h Modules.h
    lib/plugin/
        Slab.h Ipc.h Protocol.h Runner.h Host.h

现有 lib/camera、lib/coordinate、lib/rendering 保持不变，新层引用它们而不反向依赖。

### 11.2 阶段计划（每阶段可独立交付）

| 阶段 | 目标 | 主要产出 | 参考本文 |
|---|---|---|---|
| 0 抽象基座 | 清爽的渲染边界 | RendererBackend 增加 capability、Bounded* 坐标约定 | 5.3/5.4 |
| 1 原型几何 | 圆/弧/多段线在视口正确 | domain+geom 小集、Tolerance、CurveSampler | 4.1/4.2 |
| 2 编辑核心 | 命令/undo/捕捉可用 | CommandRegistry、UndoStack、SnapEngine | 6 |
| 3 DXF 互通 | 打开常见 DXF 上屏 | io 接口 + 自研/第三方 DXF | 9.1/9.2 |
| 4 3D 内核 | 拉伸/旋转/布尔 | OpenCASCADE（含本地帧 wrap + provenance） | 4.5/4.6 |
| 5 插件与无头 | 进程外插件 + 批量转换 | lib/plugin + io 批处理 | 7/8 |
| 6 平台降级 | 老 GPU 稳定 / wasm 降级对照 | 冒烟探测、能力 fallback、DocumentSource | 5.3/10 |

阶段 1-3 是"最小可用 CAD"，投入明显小于整体；阶段 4-6 是能力扩展。

### 11.3 与本工程现有算法的接口约定

- 所有新增几何采样后的三角/线段，坐标一律先经 WorldRebase::localize() 再写入 GPU uniform 或顶点；相机统一新建 PerFrameCamera（含 chord tolerance、depth slab、cameraFront）。
- RendererBackend 逐步由"drawGrid/drawCube"成长为 drawWireBatch/drawMeshBatch/drawGlyphBatch/drawHatch，但每个 pass 仍然只做"接受渲染数据 → 提交 bgfx"，不参与文档/几何逻辑。
- 网格 LOD 沿用现有"原点吸附 + 视距分级"经验，实体 tessellation 容差视为同一切片上更细的一层。

## 12. 值得固化为 ADR 的关键决策

| # | 决策 | 理由 |
|---|---|---|
| ADR-1 | 实体几何只有 entity_curve 一个来源 | 屏幕/捕捉/建模/裁剪不可分叉 |
| ADR-2 | 曲线一律容差驱动采样，禁止固定点数 | 避免大半径锯齿/小半径浪费 |
| ADR-3 | CPU double+局部帧；GPU 一律 bounded float | 共用一个精度 invariant |
| ADR-4 | 文档是唯一事实；渲染只是可丢弃缓存 | undo/插件/快照都以文档为界 |
| ADR-5 | 命令即 API：GUI/脚本/MCP 同注册表 | 无头能力不产生第二套实现 |
| ADR-6 | 渲染能力探测驱动降级，少 if(平台) | 老 GPU/未来 wasm 行为一致 |
| ADR-7 | 内核结果与网格结果分层标记（incomplete + fallback） | 缺面/降级必须显式可见 |
| ADR-8 | 插件进程外运行 + 双缓冲只读快照 | 崩溃隔离、大文档不复制 |

## 13. 主要风险与应对

1. **DWG codec**：自研成本极高 → 优先 C ABI 调用 acadrust 或 ODA，接口隔离以便日后替换。
2. **许可证**：GPL/MPL 分文件传染，照搬源代码会改变本工程许可 → 只移植算法思想、独立实现。
3. **大规模并行与缓存失效**：渲染缓存更新与 undo 交叠最容易出 bug → 用文档变更通知驱动失效，禁止在两个模块重复维护状态。
4. **性能/大坐标连锁 bug**：统一局部坐标系后才能开大批量批量渲染，避免每个 pass 各自修正精度。
5. **范围失控**：先把第 11 章阶段 1-3 收敛成可演示产物，不在初期做插件/约束/ACIS 全量同步。

## 14. 参考

- OpenCADStudio：https://github.com/HakanSeven12/OpenCADStudio
- 实体 tessellation 路径：docs/tessellation.md
- 桌面/Web 差异：docs/native-vs-web.md
- 插件架构规格：docs/plugin-architecture.md、crates/ocs_plugin_api/ARCHITECTURE.md
- 自动化/MCP：docs/automation/README.md
- cadcodec（acadrust）：https://github.com/HakanSeven12/cadcodec
- cadkernel：https://github.com/HakanSeven12/cadkernel
- 本工程渲染/坐标现状：lib/rendering/RendererBackend.h、lib/coordinate/WorldRebase.h、lib/camera
