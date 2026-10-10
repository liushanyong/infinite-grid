# 调试取证手册(debugging playbook)

显示/拾取/高亮异常时按此流程取证,不要对着代码猜。

## 快速判读表

| 现象 | 首选工具 | 章节 |
|---|---|---|
| 选中后"被遮挡物体透出来"/幽灵描边 | 像素差分 + ID pass 深度状态检查 | §1、§5 |
| 拾取不到 / picked 名字不对 | GRID_PICK_DEBUG 追溯行 + GRID_DEBUG_PICK 队列 | §2 |
| 高亮/描边位置或颜色不对 | 实屏截取 + 像素颜色代数 | §3 |
| 交互无响应 | 事件桥检查 + [IO] 式探针 | §4 |
| ID 缓冲内容存疑 | K 键全屏 ID 视图 / 点击像素读回 | §5 |

## §1 像素差分(首选,2026-10-10 实战)

同一相机下截"正常态"与"异常态"两张全窗截图(PrintWindow),
`PIL.ImageChops.difference` 后按 6× 放大阈值存图——**哪些像素变了**
一张图看清。对变化像素做颜色代数即可指认合成公式:

- 选中描边色 = (1.0, 0.55, 0.05),present 里按 `0.48*alpha` 混入:
  某像素 = 0.52·底色 + 0.48·描边色 ⇒ present 边界描边。
- 半透明面(α)后画在 X 上 = (1-α)·X + α·面色 ⇒ 场景期混合次序问题。

判读实例(半透明 Solid 亮显 bug):变化像素恰为
0.52·平面紫 + 0.48·描边橙 ⇒ 不是场景几何透出来,而是 present 沿
ID 接缝描边 → 查 ID pass 为什么让被遮挡实体持有 ID(→§5)。

## §2 拾取追溯(GRID_PICK_DEBUG=1)

关键行:`[PICK_DEBUG] click/request token/ndc` → `result ready id=` →
`registry size/nextId/found` → `fallback ndc`。

- `result id=0` + fallback 也空 → 该 texel 在 ID 纹理里是背景:
  图元没进 ID pass(队列丢弃/深度板裁剪/几何错位)或点击坐标偏移
  (窗口系 vs 面板系,见 pitfalls)。
- `[PICK_QUEUE] meshes/edges/triangles/dropped*` = 0 但实体可见 →
  可见性候选或 queue 门控问题(gpuPickQueueActive 等)。
- 名字不对 → 查 occlusionRank 排序(谁后提交)与深度一致性(→§5)。

## §3 实屏截取与自动化点击

python ctypes 流程(仓库根 `repro_highlight*.py` 可作模板):
subprocess 启动 → tasklist 找 pid → EnumWindows 找标题 →
ALT+SetForegroundWindow 抢前台 → PrintWindow 截图 →
SetCursorPos+mouse_event 注入点击/滚轮 → 截图对比。

坑:PrintWindow 是物理像素含边框;SDL 事件是逻辑点;点击 ndc 按
**整窗**算,ID texel 按**场景面板**算,小目标有系统偏移;
键盘注入(K/U/O/I)需真前台。诊断日志显式 flush(taskkill 丢缓冲)。

## §4 交互探针

在路由分支内加周期探针打印 io 状态与相机(模板见 git 历史
`[IO] pos=... mid=... wheel=...`),45 帧一次即可;必须 flush。

## §5 ID pass 深度一致性检查单

ID 纹理应当是"可见帧的深度剪影":每个像素的 ID = 该像素深度最近
的可见实体。出问题时按序检查:

1. `submitGpuPickPrimitives` 各 state:面图元(rank≤1,mesh 与
   triangle soup)必须 WRITE_Z;线/点(rank≥2)不写;rank 3 才
   DEPTH_TEST_ALWAYS。
2. occlusionRank 分配点:queueGpuMeshPick(α≥0.999→0 否则 1)、
   queueCadFill(同规则)、queueSoupChunks 默认 2、GRID_LINE_LAYER
   升 3。新实体类型先回答"可见 pass 里写不写 Z"。
3. 排序 = rank 升序,1..2 组内深度远→近(stable_sort);
   "后提交者赢 ID"依赖上面两条同时成立。
4. 深度板/近远阵:pick request 的 nearDepth/farDepth/logDepth 与
   场景帧一致;cad_outputDepth 会在板外 discard。
5. K 键(view 18)直接呈现 ID 纹理;注意 presentIdDebug 走的是
   s_texColor——不要用 uPresentParams.w=1 看 ID(红屏假象)。
