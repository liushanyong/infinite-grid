---
name: acgs-acgi-rendering
description: >
  infinite-grid 渲染系统(ObjectARX 对齐)的架构地图、铁律与调试手册。
  涉及 acgs/AcGsManager/AcGsView/AcGsDevice、acgi 协议、bgfx 后端、
  多视口、拾取(pick)、GPU ID pass、selection outline、亮显、GRID_IMGUI、
  ImGui 集成、viewport 显示异常、拾取不到、高亮错位、透显/遮挡异常等
  话题时使用——即使没有明说 "acgs" 或 "acgi"。
---

# acgs/acgi 渲染系统

ObjectARX 对齐的渲染架构(C++23 / bgfx / SDL3 / ImGui / entt)。
本文件是地图;深坑与取证手册在 `references/` 下,按需读取。

## 三层所有权(核心心智模型)

```
main(宿主)── 帧泵四拍: beginFrame → 内容提交 → endFrame → compositeFrame → present
  │           输入事件 → 悬停面板路由 → AcGsView 交互门面(orbit/pan/zoom)
  ▼
acgs::AcGsManager(acgsGetManager())── 设备工厂 + N 个 AcGsView + 轮转调度
  ├─ AcGsView ×N ── 活视口会话: 相机/交互门面/深度板/视觉样式/绑定 AcDbViewportTableRecord
  ├─ acgs::SceneStore(entt) ── 内存镜像,按 AcDbObjectId 键控
  └─ acgs::AcGsDevice ── 渲染目标抽象 = rendering::BgfxRenderer
  ▼
acgi ── 协议层: WorldDraw / TextDevice / AcGiTextStyle(渲染器实现协议,方向不可反)
```

- **acdb 管配置**(AcDbViewportTable/ViewTable/CVPORT,SQLite 按句柄持久化);
  **acgs 管会话**(活相机);**rendering 管像素落点**。帧泵留在宿主(ObjectARX 同款)。
- 设备类型在 acgs 之外不可见:main 只说 manager 语义 API。
- **渲染是脏门控的**:静态场景不重渲(AcGsView invalidate/touchSceneViews);
  空闲帧禁止 blit。点击/D/V/O/U 等改状态后必须显式 invalidate。

## bgfx view 地图(2026-10 现状)

| view | 用途 |
|---|---|
| 0 Background | 唯一清屏 view(color+depth+stencil);无限网格(写 Z 的网格线) |
| 1 DepthPrepass | hiddenLine 深度预通道 |
| 2 SolidFill | 全部不透明/半透明面(DepthDescending 排序;CAD 填充**写 Z**) |
| 3/4 Edges/Wire | 特征边/线框(Sequential) |
| 5 Overlay | 折线/线实例/曲线/点(LEQUAL;后于 SolidFill → 靠深度被遮挡) |
| 6/7 GpuPick(+Blit) | 1×1 点击拾取目标 |
| 9/10 GpuPickDebug(+Blit) | 全场景 ID pass(常驻,单一 ID 源) |
| 12 Text | SDF 文字合成进场景离屏目标(**无深度测试**,永远在最上) |
| 16 GpuPickPixelBlit | 点击像素读回 blit(必须在 view 9 之后) |
| 17 Present | 场景→后缓冲(FXAA 门控 + 选中描边合成) |
| 18 GpuPickDebugPresent | ID 纹理全屏检视(K 键) |
| 19/20/21 | PipComposite / 宿主 UI / ImGui |
| 22 FinalBlit | ImGui 模式: 场景→final 纹理(选中描边也在此跑) |

场景离屏目标是 MSAA;只有 view 0 清屏,后续通道继承深度。

## GPU 拾取与选中描边(现状)

- **统一全场景 ID pass**(view 9)是唯一 ID 源:K/I 检视、选中描边、
  点击像素读回都采样同一张常驻 ID 纹理。ID 按实体身份稳定注册
  (**不再每帧 clear+重注册**,跨帧不漂移;离开视野的旧条目无害)。
- ID 刷新由 sceneIdSignature 门控(相机/投影/深度板/视口/描边对象变化才重渲)。
- **occlusionRank 语义**: 0=不透明面,1=半透明面/特征边,2=线/点/曲线
  (GRID_LINE_LAYER>0 时线升 3 = DEPTH_TEST_ALWAYS 屏幕覆盖层)。
  提交顺序 = rank 升序;rank 1..2 组内按深度远→近;**后提交者赢 ID**。
- **单选亮显 = 场景内重着色(统一深度缓冲,2026-10-11 定稿)**:选中实体的
  图元**在自己的 pass 里**以 `kSelectionHighlightRGB`(0.35,0.65,1.0)绘制,
  **保持原 alpha/深度/混合**——高亮的深度关系与原始渲染由构造一致
  (圆弧穿越半透明面时,高亮同样被面调淡)。覆盖所有类别:线/点
  (stroke/point 拷贝重色)、面(visibleFillVertices 插入重色 + 边界
  ribbon 重色)、曲线(batch 拷贝重色 acgiMaterial.baseColor)、网格
  (cachedMeshInstance/appendMeshEntityToScene 的 colorOverride)、SDF
  文字(AcGsView::setTextColorOverride)、tiny impostor、中心立方体。
  **present 阶段的 ID 替换已停用**(params.x=0)——它曾导致"高亮与原始
  深度关系不一致"(ID pass 的半透明面 WRITE_Z 挡住了可见通道里透过面
  的圆弧)。outlineAll(U 键调试)保留边界描边。
  U=outlineAll,O=锁定上次选中,I=ID 方块,K=全屏 ID 视图。
- **ID pass 深度语义镜像可见通道**(拾取准确性):rank==0 不透明面写 Z;
  半透明面(rank 1)**不写 Z**(可见通道混合不写 Z,面后几何在两个通道
  里都可被拾取);线/边不写 Z;rank 3 覆盖层 ALWAYS。单选已无边界描边,
  旧的"幽灵描边"不会复发。
- **ID pass 双门控 + sticky 纹理**(2026-10-11 定稿):pass 仅在拾取中/
  K 视图/签名变化时重渲;每次渲染后 blit 进 **m_gpuPickIdSticky**(只采样
  的持久纹理——RT 不跨帧保留,采样纹理才保留),present/K 视图/GRID_DUMP_ID
  都采样 sticky。选中高亮在完全门控的帧上依然稳定(0 pass 成本)。
- **代际失效铁律**(main.cpp invalidateTessellationKeyedCaches):tessellation
  按 demoSceneStore().revision() 重建时,**一次性清空**拾取注册表
  (clearRegistry)、acgi::textRequests()、文字布局缓存、以及全部以
  tess-stored 指针为键的缓存(fill 顶点/曲线采样/mesh 实例),并重置
  outlineEntity——这些指针全部随重建失效,不清就是悬空身份+无限增长
  (文字队列曾每修订翻倍)。新增指针键缓存必须挂进这把伞。
- **文字布局缓存**(AcGiTextEngine::glyphLayout):平面文字布局与相机
  无关,按请求指针+字段哈希缓存,可见/拾取两通道共享;billboard 不缓存;
  字体重载(loadSdfFont)清 glyphTextureIds_ 与布局缓存。
- **全场景 ID pass 是"变化驱动"的**(main.cpp sceneIdSignature 门控):
  仅在拾取进行中(pendingNdc/waitingResult)、K 视图开启、或签名变化
  (相机/深度板/视口/选中对象)时重渲;空闲帧既不重渲也不排队(CPU
  队列同步停止),选中高亮继续采样常驻 ID 纹理。实测 1800 帧里 ID pass
  只跑 3 帧。点击时 pendingNdc 触发当帧刷新,读回取的是新纹理。
- SHX 描边文字走 stroke 管线(有深度);SDF 文字无深度测试(永远置顶)。

## 铁律

1. **ID pass 的深度行为必须与可见 pass 一致**:面类图元(rank≤1)一律
   WRITE_Z(可见 pass 里 CAD 填充无条件写 Z)。违反的症状:选中半透明
   Solid 后,被它遮挡的线/文字以"描边色幽灵"透出来——ID 被后面的
   stroke soup 覆盖,present 沿"选中 ID vs 隐藏 ID"的接缝描边。
   (2026-10-10 修复,BgfxRenderer::submitGpuPickPrimitives。)
2. **帧首铁律**:目标尺寸变化必须在 beginFrame 之前应用(帧中重建目标 =
   渲染被裁进一角 + 残影)。
3. **失效目标必须立即重建**:各 pass 对 isValid 为假的目标静默跳过,
   只销毁不重建 = 该通道黑屏。
4. **acgs 外不可见设备类型**;实体解析走活体 tessellation,不走拾取注册表。
5. present 阶段任何"按 ID 改色"的效果都**只对可见像素负责**——它不能
   也不该补偿 ID 缓冲里的遮挡错误;遮挡错误必须在 ID pass 修。

## 关键文件

| 文件 | 职责 |
|---|---|
| lib/acgs/AcGsManager.h/.cpp | 设备工厂/视图集/时间片 guard/拾取服务 |
| lib/acgs/AcGsView.h/.cpp | 活视口: 相机/交互/viewport record 绑定/submit 管线 |
| lib/acgs/AcGsSelectionManager.h/.cpp | ID 注册表(身份稳定)/queueSoupChunks |
| lib/acgs/AcGsProtocol.h | 提交词汇: 顶点/RenderData/GpuPick 结构 |
| lib/rendering/BgfxRenderer.cpp | 全部 view 的实现;submitGpuPickPrimitives = ID pass 状态机 |
| lib/shaders/present/fs_present.sc | FXAA + 选中描边/ID 调试合成(presentIdDebug 解码的是 **s_texColor**,非 ID 纹理——临时看 ID 别走这条路) |
| lib/shaders/cadCore/depth.sh | cad_outputDepth: log 深度 + 深度板外 discard(所有几何共用,保证跨 pass 深度可比) |
| lib/acgi/AcGiTextEngine.cpp | SDF 文字布局/上传/绘制 |

## 环境开关

| 开关 | 效果 |
|---|---|
| GRID_IMGUI=0 | legacy 全窗合成(默认 ImGui 双面板) |
| GRID_PICK_DEBUG=1 | [PICK_DEBUG] click/request/result/fallback/outlineId 追溯行 |
| GRID_DEBUG_PICK=1 | [PICK_QUEUE] 每秒队列构成 + [PICK_DUMP] 首图元转储 |
| GRID_OUTLINE_ALL=1 | 启动即全实体描边(自动化验证选中合成管线) |
| GRID_ID_VISIBLE=1 | 启动即显示 ID 方块(512² 居中;连续场景模式下需 readback 才有内容) |
| GRID_LINE_LAYER>0 | 线升 rank 3: DEPTH_TEST_ALWAYS(屏幕覆盖层) |
| GRID_SELECT_NAME=名 | 启动即按名选中(描边+染色,自动化验证单选路径;Esc 可取消;文字支持消息子串、取最后匹配) |
| GRID_SELECT_FOCUS=1 | 选中后俯视对准文字(配合 NAME 验证文字亮显) |
| GRID_DUMP_ID=1 | 约第 8 帧转储全场景 ID 纹理(id_dump_f\<N\>.bin,16 位 id) |
| GRID_TEXT_PICK_QUADS=1 | 文字拾取回退整框(回归对照) |
| GRID_FXAA | present 阶段 FXAA |
| GRID_SELFTEST | =store / =kernel / =pick 三套无头自测 |
| GRID_SUBMIT_DEBUG / GRID_LINE_DEBUG / GRID_MESH_DEBUG | 提交/线/网格探针 |

## 修改前必读

- 改动视口/拾取/高亮相关代码前,先读 `references/pitfalls.md`(每条含
  why 与修法;再踩一遍的代价很高)。
- 显示异常先走 `references/debugging.md` 的取证流程(像素差分 + 实屏
  截取 + 自动化点击),不要对着代码猜。

## 验证基线

任何渲染改动后:`GRID_SELFTEST=store`=kernel=pick 三套自测 + 默认
ImGui 模式冒烟。选中/描边类改动的自动化验证:
`GRID_OUTLINE_ALL=1` 启动 vs 普通启动,像素差分应只有"可见实体真轮廓"
的新增描边;若出现"被遮挡实体的接缝描边"(描边色=1.0,0.55,0.05 按
0.48α 混入),即 ID pass 深度一致性被破坏。
