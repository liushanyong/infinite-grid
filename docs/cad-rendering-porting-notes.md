# CAD 渲染架构移植决策记录

> 本文汇总 2026-09-27 围绕 "SYCAD / ObjectARX / ECS / 多文档" 的调查与架构决策，
> 作为 infinite-grid（SDL3 + bgfx, Rebase + RTE）CAD 渲染层演进的依据。
> 配套图谱：`docs/objectarx-rendering-knowledge-graph.md`（含完整继承图、节点/边表与来源页索引）。

## 1. 调查结论：SYCAD 的渲染基类不是 Drawable

对 `E:/SYCAD` 全库检索（含 `--no-ignore`）与源码通读的结论：

1. SYCAD 中不存在名为 `Drawable` 的渲染基类。所有 CAD 实体的渲染基类是
   `acdb::AcDbEntity`（继承 `AcDbObject`），渲染协议是 ObjectARX 风格：
   - [acdb_entity.h](E:/SYCAD/sources/core/acdb/acdb_entity.h:77)：
     `virtual bool worldDraw(CADCore::AcGiWorldDraw& wd) const = 0;`（纯虚）。
   - [acdb_curve.h](E:/SYCAD/sources/core/acdb/acdb_curve.h:10)：曲线族中间基类
     `AcDbCurve : public AcDbEntity`。
   - [acgi_geometry.h](E:/SYCAD/sources/render/acgi_geometry.h:1043)：
     `AcGiWorldDraw` 包裹 `AcGiGeometry` + `AcGiSubEntityTraits`，把实体画成
     `LineEntry/MeshEntry/TextEntry/CurveEntry` 等塞进 `VgDrawListComponent`。
   - 驱动方：[editor_file_import_service.cpp](E:/SYCAD/sources/editor/services/editor_file_import_service.cpp:358)
     在导入时 `ent->worldDraw(wd)`；插件（hello_plugin）走同一入口。
2. "Drawable" 一词的两处来源（与 SYCAD 实体架构无关）：
   - `E:/SYCAD/rendering_large_coordinate_guide.md:653`：通用渲染指南里透明排序伪代码的 `Drawable*`（该指南还引用 wecad_sdk）。
   - libredwg 术语 "drawable"（DWG 图形记录），见 `sources/core/dwg/libredwg_reader.h` 与 `sources/tools/dwg_import_test`。
3. 本项目（infinite-grid）已有同一模式的"无虚函数版"：
   - `entities::tessellate()` 重载集 = `worldDraw` 协议（[tessellate.h](E:/infinite-grid/lib/entities/tessellate.h:110)，
     [main.cpp](E:/infinite-grid/main.cpp:1159) 每帧驱动）；
   - `TessellatedEntity`（strokes/fills/points）= 几何收集器；
   - `EntityCommon` = traits 子集；`RendererBackend::drawPolylines/drawFilledTriangles` = 批量提交端。

## 2. ObjectARX 2026 渲染协议要点（arxref 实证）

来源 `C:/objectarx2026/docs/arxref`（25,505 个 HTML；完整图谱见配套文档）。继承链：

```
AcRxObject
  └─ AcGiDrawable                    // 可绘制协议根（用户印象中的 "Drawable" 在真实 ARX 中即此类）
      └─ AcDbObject                  // 数据库驻留：objectId/handle/filer/reactor/xdata/deepClone
          └─ AcDbEntity              // 图形数据库对象基类
              ├─ AcDbCurve → Line/Circle/Arc/...
              ├─ AcDbBlockReference  // 块引用递归
              └─ AcDbDimension/Hatch/3dSolid/Surface/...
AcRxObject
  └─ AcGiCommonDraw
      ├─ AcGiWorldDraw    (owns AcGiWorldGeometry + AcGiSubEntityTraits)
      └─ AcGiViewportDraw (owns AcGiViewportGeometry + AcGiSubEntityTraits + AcGiViewport)
AcGiGeometry
  ├─ AcGiWorldGeometry
  └─ AcGiViewportGeometry
```

决定移植设计的语义（均来自 arxref 页面原文）：

| # | 语义 | 移植含义 |
|---|---|---|
| S1 | 2026 起 `AcGiDrawable::worldDraw` 为 `ADESK_SEALED_VIRTUAL`，派生类覆盖 `subWorldDraw` | 本项目无二进制兼容包袱，直接暴露 `worldDraw` 语义即可 |
| S2 | 两段式绘制：`worldDraw` 产跨视口共享几何；仅当它返回 `kFalse` 才逐视口调 `viewportDraw` | "共享几何 + 每视口补画" 分层是官方协议，不是优化技巧 |
| S3 | GS 缓存 `worldDraw` 产物，`AcGsModel::onModified()` 显式失效 | DrawList 缓存 + 文档脏通知的权威出处 |
| S4 | `AcGiCommonDraw::deviation()` 向实体下发细分容差；`numberOfIsolines()` 控制素线 | 与本仓库 ADR-2（禁止固定点数采样）同构，作为容差通道移植 |
| S5 | `geometry.draw(subDrawable)` + `push/popModelTransform` = 块引用递归官方协议 | DrawSink 需要递归入口与 transform 栈 |
| S6 | `setSelectionMarker/pushMarkerOverride` 绑定图元与子实体 | 拾取可从实体级细化到子图元级 |
| S7 | 官方认可派生 `AcGiWorldDraw/AcGiWorldGeometry` 录制几何（无实体源码也可捕获） | "录制式 geometry sink" 是移植合法性与设计模板 |

## 3. 移植决策：AcGi-lite（最终方案）

**移植协议形状，不移植对象图。**

值得移植：`WorldDraw/ViewportDraw + GeometrySink + DrawTraits` 这组接口形状、
两段式绘制、deviation 容差通道、递归入口、selection marker。

不值得移植（负价值）：`AcRxObject` RTTI/类注册、`AcDbObject` filer/reactor/deepClone
对象图、Overrule、GS 内部流（AcGsModel/AcGsView）、ownerDraw(GDI)。
理由：本仓库 ADR-4 已定"值类型文档是唯一事实"；堆对象图破坏 undo/快照/列式遍历；
SYCAD 已证明该协议可脱离 ARX 宿主独立实现。

三方案对比（定案依据）：

| 维度 | A. 严格 OOP（Entity 虚基类堆对象） | B. 纯演进（只建 DrawList 缓存） | **AcGi-lite（采纳）** |
|---|---|---|---|
| 协议形状 | 最像 ARX，但绑死对象图 | 无显式协议 | 显式（WorldDraw/ViewportDraw/Traits），实体保持值类型 |
| 每视口 LOD/deviation | 有，但先付对象图代价 | 无 | 有，成本低 |
| 块引用/标注递归 | 天然 | visitor 内递归 | `sink.draw()` + transform 栈，最贴 ARX |
| 改动量/风险 | 大（undo/快照/拾取全动） | 小但协议缺失 | 中：新增 `DrawContext.h` + 收集器改造 |
| 与仓库 ADR | 冲突 | 一致 | 一致并补齐容差通道 |

实施四步：

1. `lib/scene/DrawContext.h`：`DrawTraits`（AcGiSubEntityTraits 子集）、`GeometrySink`
   （worldLine/polyline/polygon/circle/circularArc/ellipticalArc/shell/mesh/text/ray/xline
   + push/popTransform + `draw(entityVariant)` 递归）、`WorldDraw`（共享几何，可缓存）、
   `ViewportDraw`（附 `pixelsPerUnit()/deviation()`）。
2. `tessellate()` 重载集升级为 `worldDraw(entity, WorldDraw&)` 语义，收集时应用
   `EntityCommon` traits；BlockReference/Dimension 用 `sink.draw()` 递归。
3. `DrawListCache`：键 `(entityRevision, renderOrigin, chordToleranceBucket, traitsVersion)`；
   文档修改通知失效（S3 的最小等价物）；产物仍是 `Stroke/Triangle`，喂现有
   `drawPolylines/drawFilledTriangles`。
4. 视口通道：相机每帧算 `pixelsPerUnit`/弦容差，经 `ViewportDraw` 下发；先服务曲线 LOD。

## 4. ECS 化后的渲染系统设计

前提：ECS 管数据与生命周期，绘制协议管几何产出，`RendererBackend` 管提交——三层解耦，
后端永不感知 ECS 类型。

**关键决策：ECS 先做"渲染投影"，不做存储。** CadDocument 保持唯一事实（ADR-4），
command 提交后同步组件；后期若投影维护成本过高，再升级 ECS 为存储、文档 facade 化。
不推荐把 `AcDbEntity` 对象整体挂组件（SYCAD 现状模式）：双份存储、查询穿透不了几何、
undo/IO 仍走对象图。

组件规划：

| 类别 | 组件 | 对应 ARX |
|---|---|---|
| 每实体 | `EntityCommon`(+revision)、几何 payload（或 variant）、`BlockRef`+transform、`DrawOutput`（缓存条目+键）、Dirty | `AcDbEntity` 属性、块变换栈、GS 缓存 |
| 单例/共享 | 每生产者 `VgDrawList`（纯数据：producer_name/render_order/visible/dirty/lines/meshes/texts/curves）、`RenderContext`（render origin/视口/deviation）、`GpuResources`、`LayerTable`、`FrameArena`、`SelectionSet` | `AcGiWorldDraw`、`AcGiSubEntityTraits`、`AcGsModel`、sortents |

系统流水线（按序；构建与提交严格分离，`worldDraw` 严禁进入 submit 循环）：

```
DocumentSync → TraitsResolve → WorldDraw(脏驱动, 可并行)
   → ViewportDraw(每视口, deviation/pixelsPerUnit)
   → Batch(合并 draw list → 线/面/字批次, WorldRebase 局部化, geometryKey 复用)
   → PassSystems(grid/wire/fill/text/overlay/pick → RendererBackend)
   → Picking(selectionMarker → GPU id 读回 → (entityId, 子图元))
```

缓存与失效：`DrawOutput` 键同第 3 节；命令提交置 Dirty + revision++（S3 的等价物）；
生产者级 `VgDrawList::dirty` 控制重批（SYCAD Phase C2.1 实证：不按生产者拆分会出现
"一处脏、全体重批"）。

实证过的坑（SYCAD 已踩，直接规避）：

1. EnTT 组件跨 EXE/DLL 的 id 不一致：组件必须 `ENTT_NAMED_TYPE`
   （[vg_draw_list_component.h](E:/SYCAD/sources/render/vg_draw_list_component.h) 文件头是完整事故记录）。
2. 共享 system 对象内部禁止藏文档状态，per-doc 状态一律经 World/组件传入。
3. 每帧条目走 `FrameArena`；GPU 顶点用稳定 index + generation。

## 5. 多文档编辑：一文档一 World

结论：可行且成本低。`DocumentManager` 持有 N 个 World，`View(docId, camera, viewport)`
绑定视图；`World` = `AcApDocument + AcDbDatabase` 的 ECS 合体，`DocumentManager`
对应 `AcApDocumentManager`。

| 按文档隔离 | 全局共享 |
|---|---|
| 实体 id/handle 空间、图层/线型/样式符号表、undo 栈、选择集、Dirty/revision、每生产者 draw list、文档级 render origin | system 定义与调度、GPU 资源（程序/管线/纹理）、`RendererBackend`、字体/SHX、命令注册表 |

渲染联动：每帧只跑有可见视图的文档的流水线；提交按视图收集其文档批次；
同文档多视图共享 world 几何缓存，仅 viewport 段按视图计算；文档切换 = 切活跃 World，
后台文档重建可跨 World 并行。

跨文档操作：复制粘贴 = 值序列化 + 目标文档 id 重映射 + 符号表合并（值类型实体的强项）；
对比视图 = 两 View 绑两 World 只读；xref = 只读借用源文档块表（后期）。

不采用"单 World + DocumentTag"：所有查询要带 tag 过滤、关闭文档要按 tag 销毁、
undo/符号表按 tag 分桶，隔离纪律全靠人，CAD 场景不划算。

对既有方案的冲击：零。AcGi-lite 协议、DrawListCache、七段流水线全部不变，
仅运行单位从"全局一次"变为"每 World 一次"。

## 6. 路线图

| 阶段 | 内容 | 依赖 |
|---|---|---|
| P1 | `DrawContext.h` + `tessellate()` → `worldDraw()` 语义迁移 + traits 应用（已完成） | 无（纯协议层） |
| P2 | `DrawListCache` + 文档脏通知失效 + batch 提交接通（demo 缓存已完成，动态文档通知接入 P4） | P1 |
| P3 | `ViewportDraw` 容差/LOD 通道 + SceneDrawList 全示例迁移 + `AcGiDrawable` 根（已完成） | P2 |
| P4 | ECS 投影（组件化 + 七段流水线），文档仍为事实源 | P1-P3 |
| P5 | DocumentManager 多文档（World per doc + View 绑定） | P4 |
| 后续 | selection marker 子图元拾取、块引用/标注递归补全、xref | 按需 |

## 7. 证据索引

| 主题 | 位置 |
|---|---|
| `AcDbEntity::worldDraw` 纯虚 | `E:/SYCAD/sources/core/acdb/acdb_entity.h:77` |
| AcGiWorldDraw/AcGiGeometry | `E:/SYCAD/sources/render/acgi_geometry.h`（:1043 起） |
| worldDraw 调用方（导入/插件） | `E:/SYCAD/sources/editor/services/editor_file_import_service.cpp:358`、`sources/plugins/hello/hello_plugin.cpp` |
| ARX 继承链/协议语义 | `C:/objectarx2026/docs/arxref/AcGiDrawable.html`、`AcDbEntity.html`、`AcGiWorldDraw.html`、`AcGiViewportDraw.html`、`AcGiCommonDraw.html`、`AcGiGeometry.html`、`AcGiSubEntityTraits.html` |
| 本项目现状协议等价物 | `E:/infinite-grid/lib/entities/tessellate.h:110`、`main.cpp:1159`、`lib/rendering/RendererBackend.h` |
| ECS 渲染参照与坑 | `E:/SYCAD/sources/foundation/ecs/system.h`、`sources/render/vg_draw_list_component.h` |
