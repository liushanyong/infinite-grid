# infinite-grid Demo 操作说明

面向演示与调试的完整操作手册。构建产物位于 `build2022/bin/Release/WINDOW.exe`
（或 `bin/Debug/WINDOW.exe`），直接运行即可；所有环境变量均为可选。

---

## 1. 鼠标操作

| 操作 | 效果 |
|---|---|
| **中键拖动** | 平移（沿相机像平面移动，正交/透视行为一致） |
| **Shift + 中键拖动** | 环绕旋转（绕按下中键瞬间捕获的场景支点旋转） |
| **滚轮** | 缩放（透视改相机距离，正交改 OrthoSize，目标点保持居中） |
| **左键双击** | 拾取/自动聚焦：命中光标下最近的物体，聚焦到其深度 |

说明：环绕支点在每次中键按下时重新捕获（VSG 式持久支点），连续环绕同一区域
时不必重新对准。

## 2. 键盘快捷键

| 按键 | 功能 |
|---|---|
| **V** | 循环切换视觉样式（见下节） |
| **P** | 切换正交 / 透视投影 |
| **1 / 2 / 3 / 4** | 网格平面：XY / XZ / YZ / 自定义平面 |
| **R** | 从当前相机姿态重置世界向上轴与网格平面 |
| **C** | 目标平面约束开关（把目标锁定在网格平面上） |
| **L** | 场景传送：原点场景 ↔ 大坐标应力场（相机直达 1e7 量级区域） |
| **F** | 捕获视锥线框（调试显示） |
| **K** | 全场景 GPU ID 视图开关（整屏显示拾取 ID 缓冲，用于验证拾取） |
| **I** | GPU 拾取调试纹理开关（窗口中央 512×512 显示最近一次拾取的 ID 缓冲） |
| **O** | 选中轮廓锁定开关（锁定最近一次拾取的实体轮廓） |
| **U** | 全体轮廓显示开关（所有实体画选中轮廓） |
| **ESC** | 退出 |

启动横幅会打印最常用的按键提示。

## 3. 视觉样式（V 键循环）

循环顺序与 `VISUALSTYLES` 命令对应关系：

| 顺序 | 样式 | 行为 |
|---|---|---|
| 1 | Wireframe 2D | 全部线框按提交顺序叠放；2D 实心填充（Hatch/Solid 等）保留 |
| 2 | Wireframe 3D | 线框参与深度遮挡；填充抑制，纯填充实体显示**彩色边界线** |
| 3 | Hidden Line | 消隐：可见面 + 可见边，被遮挡部分不画 |
| 4 | Shaded | 着色填充，无边线 |
| 5 | Shaded + Edges | 着色填充 + 特征边（mesh 边线为不透明加深 10% 的 ribbon） |
| 6 | Depth Buffer | 深度缓冲灰度可视化 |

启动时可用 `GRID_START_STYLE=<1..5>` 直接指定初始样式（用于截图/自动化）。

## 4. Demo 场景内容

- **CAD 实体组**（围绕原点网格）：Line、Arc、Circle、Ellipse/EllipseArc、
  Polyline（含 bulge 圆弧）、LwPolyline（闭合）、Spline（控制点形式）、
  FitSpline/ClosedFitSpline（拟合点形式，C2 自然三次插值）、SpaceSpline
  （非平面空间曲线）、ClosedFitRing（闭合样条的直边参考环）、Point、Ray
  （单向无限线）、XLine（双向构造线）、MLine/ClosedMLine（双线）、
  Text/MText（当前以布局框显示）、Hatch（实心）、PatternHatch（ANSI31
  图案 + 内环挖孔）、AngledHatch（45° 加粗图案）、Solid/Rectangle
  （DXF 锯齿序四边形）、Solid3d（特征边描边 + 反色边线）、ArrowHead、
  DashedArrow（DASHED 线型）、Hexagon、Light、ParamSurface（参数曲面 +
  等参线）。
- **程序化网格场**：Cube/Sphere/Cone/Torus 实例阵列，带 GPU 实例化棱边
  ribbon（Shaded + Edges 下可见）。
- **中心立方体**与无限网格平面（1-4 键切换基平面）。
- **大坐标应力场**（L 键传送）：1e7 量级坐标下的同构场景，验证双精度
  相机重定基与深度切片。

## 5. 常用环境变量

### 场景内容

| 变量 | 说明 |
|---|---|
| `GRID_DWG=<路径>` | 加载 .dwg 文件，其中的 LINE/ARC/CIRCLE/ELLIPSE/POINT/RAY/XLINE 以 ACI 颜色并入场景（走与 demo 实体完全相同的管线） |
| `GRID_CAD_DEMO=0` | 关闭 CAD 实体组 |
| `GRID_DEMO_MESHES=0` | 关闭程序化网格场 |
| `GRID_VALIDATION_MESHES=0` | 关闭验证网格 |
| `GRID_CENTER_CUBE=0` | 关闭中心立方体 |
| `GRID_STRESS_COUNT=<n>` | 应力场物体数量（默认 1000，上限 1e6） |
| `GRID_PARAM_SURFACE=0` | 关闭参数曲面及其等参线 |
| `GRID_PARAM_SURFACE_SEGMENTS=<n>` | 曲面细分密度（默认 24，用于性能对比） |
| `GRID_PARAM_SURFACE_ISOLINES=0` | 只画曲面体不画等参线 |

### 渲染

| 变量 | 说明 |
|---|---|
| `GRID_START_STYLE=<1..5>` | 初始视觉样式 |
| `GRID_FXAA=1` | 启用 FXAA 后处理 |
| `GRID_REALISTIC=0` | 关闭真实感材质路径 |
| `GRID_MESH_TEXTURE=<图片路径>` | 给网格加载纹理 |
| `GRID_MESH_HEADLIGHT=<0..1>` | 头灯强度 |
| `GRID_MESH_TRIPLANAR=<0..1>` | 三平面映射混合因子 |
| `GRID_GPU_PICK=0` | 拾取走 CPU 路径（默认 GPU） |

### 自动化 / 截图

| 变量 | 说明 |
|---|---|
| `GRID_SCREENSHOT=<路径>` | 启动约 30 帧后自动截图 |
| `GRID_EXIT_FRAMES=<n>` | 运行 n 帧后自动退出（回归测试） |
| `GRID_CAMERA_START_TARGET=<x,y,z>` | 指定初始相机目标 |
| `GRID_CAMERA_TEST_ORTHO=1` | 以正交启动（配合下一个变量） |
| `GRID_CAMERA_TEST_ORTHO_HALFH=<值>` | 指定初始 OrthoSize |
| `GRID_CAMERA_TEST_PAN_X=<值>` + `GRID_CAMERA_TEST_PAN_AT_SECONDS=<秒>` | 定时自动平移（无焦点自动化） |
| `GRID_CAMERA_TEST_DOUBLE_CLICK_X/Y=<像素>` | 启动时模拟双击拾取 |

### 诊断

| 变量 | 说明 |
|---|---|
| `GRID_DEBUG_SLAB=1` | 打印正交深度切片（near/far）的稳定器变化，验证切片收敛 |
| `GRID_DEBUG_PICK=1` | 每秒打印 GPU 拾取队列构成（`[PICK_QUEUE]`），样式切换时打印完整载荷（`[PICK_DUMP]`：实例矩阵、eye 高低位、首个图元顶点） |
| `GRID_FRAME_LOG=1` | 打印 60 帧平均帧时间 |
| `GRID_LINE_DEBUG=1` | 打印线带顶点/NDC 范围与无限线裁剪细节 |
| `GRID_PICK_DEBUG=1` | 打印拾取流程细节（双击坐标、GPU/CPU 路径选择） |
| `GRID_PICK_AUDIT=1` | 拾取审计输出 |
| `GRID_CAMERA_DEBUG=1` | 相机状态跟踪输出 |
| `GRID_GPU_DEBUG=1` | 渲染器 GPU 调试信息 |

## 6. 推荐的演示路径

1. 启动 → 观察网格平面 + CAD 实体组 + 网格场（Shaded + Edges 样式最直观，
   `GRID_START_STYLE=5`）；
2. **V** 循环六种样式，重点看 Wireframe 3D 下纯填充实体的彩色边界、
   Hidden Line 的消隐；
3. **中键拖动**平移、**Shift+中键**环绕、滚轮缩放；**左键双击**拾取聚焦；
4. **1/2/3** 切换基平面，**P** 切换正交/透视，观察两个投影下的操作一致性；
5. **L** 传送到大坐标应力场，验证 1e7 量级下平移/缩放/拾取依然平滑精确；
6. `GRID_DWG=某文件.dwg` 运行，查看真实 DWG 数据与 demo 实体同屏渲染；
7. 疑难排查时按第 5 节打开对应诊断开关，把控制台输出发给开发。
