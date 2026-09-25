# 基于 bgfx 的 OpenCADStudio 风格渲染模式技术方案

## 1. 目标

将 OpenCADStudio 的 CAD 渲染模式移植到本项目当前的 `SDL3 + bgfx` 渲染路径中，复用现有 `BgfxRenderer`、RTE/rebase 坐标体系和 D3D 深度投影转换。当前公共视觉样式已合并为五种：

- Wireframe 2D
- Wireframe 3D
- Hidden Line
- Shaded
- Shaded + Edges

Flat / Gouraud 在当前统一 shader 中视觉差异不足，且会造成模式重复；二者合并为 Shaded。Flat shading 仍可作为材质/effect 分支保留，但不再作为第二套公共 RenderMode 暴露。

设计目标是让上层只需要传一个 `RenderMode`，底层自动决定 view 顺序、shader 程序、绘制状态和遮挡策略。

---

## 2. 总体思路

bgfx 没有 wgpu 那种显式 render pipeline 对象，因此将 OpenCADStudio 中的 pipeline 分支翻译为三件事：

1. **不同 `bgfx::ProgramHandle`**
2. **不同 `bgfx::setState()` 组合**
3. **不同 `bgfx::ViewId` 顺序**

核心映射：

| OpenCADStudio 概念 | bgfx 实现方式 |
|---|---|
| Render pipeline | Program + State + View |
| Depth-only prepass | 关闭 color write 的 draw pass |
| Flat / Gouraud 历史分支 | 已合并为 Shaded；`u_flatShade` 仅作为可选材质分支 |
| Wireframe 2D | CAD draw-order depth 或 Sequential 提交 |
| Wireframe 3D | 真实 3D 深度 |
| Hidden Line | Depth-only prepass + edge depth test |
| Shaded + Edges | Shaded fill 后绘制黑色 edge overlay |

---

## 3. 模式与标志

### 3.1 RenderMode

```cpp
enum class RenderMode {
    Wireframe2D,
    Wireframe3D,
    HiddenLine,
    Shaded,
    ShadedWithEdges,
};
```

### 3.2 RenderModeFlags

```cpp
struct RenderModeFlags {
    bool face3dFill;
    bool meshFill;
    bool show3dEdges;
    bool hiddenLine;
    bool show2dSolidFills;
    bool wireframe3d;
};
```

### 3.3 模式到标志的映射

| 模式 | face3dFill | meshFill | show3dEdges | hiddenLine | show2dSolidFills | wireframe3d |
|---|---:|---:|---:|---:|---:|---:|
| Wireframe 2D | 0 | 0 | 1 | 0 | 1 | 0 |
| Wireframe 3D | 0 | 0 | 1 | 0 | 0 | 1 |
| Hidden Line | 1 | 1 | 1 | 1 | 1 | 0 |
| Shaded | 1 | 1 | 0 | 0 | 1 | 0 |
| Shaded + Edges | 1 | 1 | 1 | 0 | 1 | 0 |

---

## 4. 渲染视图布局

建议使用固定 view 顺序：

```cpp
enum : bgfx::ViewId {
    kViewBackground = 0,
    kViewDepthPrepass,
    kViewSolidFill,
    kViewEdges,
    kViewWire,
    kViewOverlay,
};
```

### 4.1 Clear 策略

只有 `kViewBackground` 清 color + depth + stencil：

```cpp
bgfx::setViewClear(kViewBackground,
                   BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
                   clearColor, 1.0f, 0);
```

后续 view 不清 depth，避免破坏 Hidden Line 的 prepass 结果：

```cpp
bgfx::setViewClear(kViewDepthPrepass, BGFX_CLEAR_NONE);
bgfx::setViewClear(kViewSolidFill,    BGFX_CLEAR_NONE);
bgfx::setViewClear(kViewEdges,        BGFX_CLEAR_NONE);
bgfx::setViewClear(kViewWire,         BGFX_CLEAR_NONE);
```

### 4.2 Framebuffer

如果启用 MSAA 或离屏渲染，所有相关 view 必须绑定同一个 framebuffer。

---

## 5. Program 集合

初始化阶段创建以下程序：

```cpp
bgfx::ProgramHandle mMeshFillProgram;
bgfx::ProgramHandle mMeshDepthProgram;
bgfx::ProgramHandle mMeshEdgeProgram;
bgfx::ProgramHandle mFaceFillProgram;
bgfx::ProgramHandle mFaceDepthProgram;
bgfx::ProgramHandle mWireProgram;
bgfx::ProgramHandle mSilhouetteProgram; // 可选
```

用途：

- `mMeshFillProgram`：统一 Shaded fill；可选 flat-normal 材质分支由 uniform 控制。
- `mMeshDepthProgram`：Hidden Line 的 depth-only prepass。
- `mMeshEdgeProgram`：绘制 mesh / B-rep 边线。
- `mFaceFillProgram`：绘制 3DFACE / planar solid。
- `mFaceDepthProgram`：Hidden Line 下 3DFACE 的深度遮挡。
- `mWireProgram`：绘制 CAD wire。
- `mSilhouetteProgram`：可选，用于曲面轮廓线。

---

## 6. State 组合

### 6.1 Depth-only prepass

```cpp
bgfx::setState(
    BGFX_STATE_DEPTH_TEST_LEQUAL |
    BGFX_STATE_DEPTH_WRITE |
    BGFX_STATE_MSAA
);
```

关键是不要启用：

```cpp
BGFX_STATE_RGB_WRITE
BGFX_STATE_ALPHA_WRITE
```

因此只写深度，不写颜色。

### 6.2 不透明 shaded fill

```cpp
bgfx::setState(
    BGFX_STATE_DEPTH_TEST_LEQUAL |
    BGFX_STATE_DEPTH_WRITE |
    BGFX_STATE_RGB_WRITE |
    BGFX_STATE_ALPHA_WRITE |
    BGFX_STATE_MSAA
);
```

### 6.3 透明 shaded fill

```cpp
bgfx::setState(
    BGFX_STATE_DEPTH_TEST_LEQUAL |
    BGFX_STATE_RGB_WRITE |
    BGFX_STATE_ALPHA_WRITE |
    BGFX_STATE_BLEND_ALPHA |
    BGFX_STATE_MSAA
);
```

透明物体一般不写 depth。

### 6.4 Edge / wire

```cpp
bgfx::setState(
    BGFX_STATE_DEPTH_TEST_LEQUAL |
    BGFX_STATE_RGB_WRITE |
    BGFX_STATE_ALPHA_WRITE |
    BGFX_STATE_MSAA
);
```

如需 edge 影响后续深度，可加 `BGFX_STATE_DEPTH_WRITE`。

---

## 7. 渲染主流程

伪代码：

```cpp
void drawScene(RenderMode mode) {
    const RenderModeFlags f = flagsFor(mode);

    if (f.hiddenLine) {
        submitDepthOnlyPrepass();
    }

    if (f.meshFill) {
        submitMeshFill();
    }

    if (f.face3dFill) {
        submitFace3dFill(f.hiddenLine);
    }

    if (f.show3dEdges || !f.meshFill) {
        submitMeshEdges(f.show3dEdges);
    }

    submitCadWires(f);
    submitOverlay();
}
```

### 7.1 模式到路径映射

| 模式 | Depth prepass | Solid fill | Edge / wire |
|---|---:|---:|---|
| Wireframe 2D | 否 | 否 | 是 |
| Wireframe 3D | 否 | 否 | 是 |
| Hidden Line | 是 | 否 | 是 |
| Shaded | 否 | 是 | 否 |
| Shaded + Edges | 否 | 是 | 是 |

---

## 8. Shading 分支与合并策略

### 8.1 可选 Flat uniform

```cpp
bgfx::UniformHandle uFlatShade;
uFlatShade = bgfx::createUniform("uFlatShade", bgfx::UniformType::Float1);
```

提交前：

```cpp
// RenderMode 不再区分 Flat/Gouraud；仅材质/effect 需要时启用。
float flat = 0.0f;
bgfx::setUniform(uFlatShade, &flat);
```

### 8.2 Shader 逻辑

```hlsl
float3 n;
if (uFlatShade > 0.5) {
    n = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));
} else {
    n = normalize(vNormal);
}
```

说明：

- Flat：使用屏幕空间导数重新计算 per-triangle face normal。
- Smooth/Gouraud：使用插值后的顶点 normal。
- 公共视觉样式统一走 Shaded；Flat 只作为材质/effect 选项，不单独参与 RenderMode 循环。

---

## 9. Hidden Line 实现

Hidden Line 不需要屏幕空间 edge detection，用 depth prepass 即可。

### 9.1 流程

1. 先把 mesh / 3DFACE / solid surface 提交到 `kViewDepthPrepass`。
2. 这些 draw call 只写 depth，不写颜色。
3. 再在 `kViewEdges` 提交 edge / wire。
4. edge / wire 使用 `LEQUAL` depth test，被实体挡住的线被剔除。

### 9.2 示例

```cpp
void submitDepthOnlyPrepass() {
    for (const MeshBatch& mesh : opaqueMeshes) {
        bgfx::setTransform(mesh.model);
        bgfx::setVertexBuffer(0, mesh.vbh);
        bgfx::setIndexBuffer(mesh.ibh);

        bgfx::setState(
            BGFX_STATE_DEPTH_TEST_LEQUAL |
            BGFX_STATE_DEPTH_WRITE |
            BGFX_STATE_MSAA
        );

        bgfx::submit(kViewDepthPrepass, mMeshDepthProgram);
    }
}
```

如果 mesh 有透明面，且希望透明面也遮挡 hidden edge，可以在 depth-only shader 中增加 alpha threshold：

```hlsl
if (color.a < 0.001) {
    discard;
}
```

---

## 10. Wireframe 2D vs Wireframe 3D

### 10.1 Wireframe 2D

目标是保留 CAD 2D 绘制顺序。

可选两种做法：

1. **简单版**
   - 使用 `bgfx::ViewMode::Sequential`。
   - 按 CAD 文档中的实体顺序提交。
   - 对纯 2D wireframe 最直观。

```cpp
bgfx::setViewMode(kViewWire, bgfx::ViewMode::Sequential);
```

2. **忠实版**
   - 每个 wire/entity 带一个 `drawOrderDepth`。
   - 在 vertex shader 中对 clip-space z 做偏移。
   - 适合 2D / 3D 混合或需要精确 CAD 顺序的场景。

### 10.2 Wireframe 3D

目标是使用真实 3D 深度，忽略 CAD draw order。

- 不使用 draw-order depth bias。
- edge / wire 直接依赖 mesh / solid 写入的 depth。
- 若 depth buffer 中没有遮挡体，则 wire 不会被剔除。

---

## 11. Edge buffer 生成

对普通三角 mesh，可以将三角形索引展开成 LineList：

```cpp
void buildEdgeIndices(
    const std::vector<uint32_t>& triangleIndices,
    std::vector<uint32_t>& edgeIndices)
{
    edgeIndices.reserve(triangleIndices.size() * 2);

    for (size_t i = 0; i < triangleIndices.size(); i += 3) {
        uint32_t a = triangleIndices[i + 0];
        uint32_t b = triangleIndices[i + 1];
        uint32_t c = triangleIndices[i + 2];

        edgeIndices.push_back(a);
        edgeIndices.push_back(b);

        edgeIndices.push_back(b);
        edgeIndices.push_back(c);

        edgeIndices.push_back(c);
        edgeIndices.push_back(a);
    }
}
```

对 ACIS / B-rep 实体，优先使用真实 feature edge，而不是三角边，视觉更干净。

---

## 12. 与当前项目的对接

### 12.1 API 修改

在 `RendererBackend` 中增加：

```cpp
virtual void setRenderMode(RenderMode mode) = 0;
```

`BgfxRenderer` 内部保存：

```cpp
RenderMode mRenderMode = RenderMode::Wireframe2D;
RenderModeFlags mModeFlags;
```

在每帧或每次 draw 前更新：

```cpp
mModeFlags = flagsFor(mRenderMode);
```

### 12.2 绘制接口

`drawCube()` / `drawMesh()` 根据 flags 决定走：

- fill program
- edge program
- depth-only program

`drawWorldLine()` / CAD wire 根据 Wireframe2D / 3D 决定是否使用 draw-order depth。

### 12.3 坐标与投影

继续沿用现有项目规则：

- CPU 使用 double 精度。
- GPU 使用 rebased / RTE 坐标。
- 投影必须通过 `projectionForDirect3D()`。
- shader 中不要对 D3D depth 做 GL 风格重映射。

---

## 13. 实施阶段

### Phase 1：基础能力

- 增加 `RenderMode` 与 `RenderModeFlags`。
- 增加 view 布局。
- 增加 mesh fill / edge / wire program。
- 合并 Flat / Gouraud 为 Shaded，并保留可选 flat-normal 材质分支。
- 实现基础 shaded fill 与 edge overlay。

### Phase 2：Hidden Line

- 增加 depth-only prepass。
- 将 mesh / 3DFACE 接入 depth-only view。
- edge / wire 接入 depth test。
- 验证遮挡效果。

### Phase 3：Wireframe 2D / 3D

- 实现 2D wireframe 的 Sequential 或 draw-order depth。
- 实现 3D wireframe 的真实深度路径。
- 处理 2D planar solid fill 的保留与移除。

### Phase 4：收尾

- 统一 MSAA / framebuffer。
- 增加模式切换 UI / 命令。
- 增加渲染截图回归。
- 优化 edge buffer 与 GPU 资源缓存。

---

## 14. 验证用例

至少覆盖：

1. 简单立方体。
2. 多个重叠实体。
3. 大坐标场景下的 pan / zoom / orbit。
4. Wireframe 2D 与 Wireframe 3D 的遮挡差异。
5. Hidden Line 下被挡住的边线是否消失。
6. Shaded 与 Shaded + Edges 的边界/填充差异。
7. 可选 flat-normal 材质分支与默认 smooth normal 的差异。
8. 透明实体的显示顺序。
9. MSAA 与非 MSAA 下的一致性。

---

## 15. 风险与注意事项

- bgfx view 内的绘制顺序受 sort key 影响，涉及 prepass + edge 时建议使用独立 view 或明确 Sequential。
- D3D 深度范围不同于 GL，不要在 shader 中做 GL 风格深度转换。
- ACIS 导入 winding 可能不可靠，cull mode 应保持可配置，默认可先关闭 culling。
- 透明实体在 depth-only pass 中是否遮挡，需要产品层面明确。
- draw-order depth bias 需要仔细控制 scale，避免顶点被推到 near/far 之外。
