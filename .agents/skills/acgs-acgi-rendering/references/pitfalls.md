# 已踩坑清单(pitfalls)

每条含现象、根因、修法。改动相关代码前先扫一遍——这些坑每一个都真实
花费过半天以上的排查时间。

## 拾取、ID pass 与选中描边

- **半透明面在 ID pass 不写深度 = 选中后"被遮挡物体透出来"**(2026-10-10)。
  现象:点选半透明 AcDbSolid(α=0.85)后,它后面的椭圆/文字/红线以橙色
  幽灵线显形;取消选中就消失。根因链:`submitGpuPickPrimitives` 只给
  rank 0 写 WRITE_Z;半透明 CAD 填充 rank=1 不写 Z → rank 2 的
  stroke/curve soup 后画,LEQUAL 对着清屏远深度全部通过,ID 纹理里
  被遮挡实体覆盖了选中实体的 ID → present 阶段 `neighbor != selectedId`
  的接缝描边混入 0.48α 描边色(实测像素恰好等于
  0.52·平面色+0.48·描边色)。修法:rank≤1 的面图元(mesh + triangle
  soup)一律 WRITE_Z,与可见 pass 对齐;"在前面的线后画、更近、LEQUAL
  通过"仍然赢 ID,原语义保留。
- **"后提交者赢 ID"只在深度一致时成立**:rank 排序(rank 升序、
  1..2 组内远→近)+ WRITE_Z(面)共同构成遮挡语义。给新图元类型定
  rank 时先回答"它在可见 pass 里写不写 Z",再选 rank。
- **选中内部染色 = present 阶段 u_selection_fill**(2026-10-10 恢复并
  参数化):rgb=染蓝, a=强度(默认 0.45;1.0=纯蓝)。染色只在
  `selected && !boundary` 生效——它依赖 ID 纹理里选中实体的像素完整,
  即上一条铁律成立;ID 深度不一致时染色会碎成补丁(这正是旧 tint 被
  误删的真正病灶:错把 ID pass 的深度 bug 当成 tint 本身的问题)。
- **线状实体选中必须走实色高亮,不走描边**(2026-10-10):细线/文字的
  几乎每个像素都满足"邻居 ID 不同",present 的边界混合把整条线变成
  0.48α 橙洗(用户感知为"半透明+描边")。修法 =
  `setSelectionOutlineLineLike(kind∈{CadStroke,CadCurve,CadText})` →
  `u_selection_line`(rgb=高亮蓝, a=使能),shader 里 selected 时直接
  `return u_selection_line.rgb`(跳过边界与染色)。注意:拾取 soup 比
  虚线的可见笔画宽(整条线点亮,虚线间隙也被覆盖)——当前接受;要改
  需让 pick soup 按 dash 模式裁剪。
- **SDF 文字的 ID 几何必须是字形形状，不是文字方框**(2026-10-10):
  文字拾取 soup 曾直接入 layoutGlyphs 的整框四边形，选中亮显输出为
  "文字方框"。修法 = `glyphCoverageUV`(main.cpp):按字形 R8 SDF 在
  **0.5 等值线**(与 fs_text_sdf 的可见阈值一致)做 marching squares,
  全内部单元填充、步进 2 texel 控制顶点量,按 codepoint 缓存,UV 双线性
  映射回字形 quad。raster 不可用时回退整框。
- **字形 UV→quad 映射必须反转 v**(2026-10-11):SDF 栅格行序首行=字顶,
  而字形 quad 的 cornerV=0 在字底;可见 pass 靠 `uv.v = 1 - cornerV`
  (AcGiTextEngine::drawText)反演。soup 若直接用 cornerV=v 会得到上下
  翻转的覆盖——拉丁字母(M/W、E/Ǝ)视觉上像"水平镜像",且 ID 纹理与
  亮显一起翻转。修法 = world = c0 + (c1-c0)*u + (c3-c0)*(1-v)。
- **rank-3 覆盖层 soup 必须免深度板**(fs_filled `uPrimParams.y`):可见
  SDF 文字无深度测试、无板外 discard,其 ID 几何若走共享 fill shader 的
  `cad_outputDepth`,文字在 [near,far] 外(如对焦后板收窄)时屏幕可见
  但 ID 消失——点击落空、亮显消失。修法 = 提交侧置 uPrimParams.y,
  shader 分支直接用 gl_FragCoord.z。
- **textRequests() 重切后含陈旧重复条目**:同一消息会有旧/新两个
  TextRequest,旧拷贝的指针指向失效布局,其 soup 栅格化到屏幕外
  (ID 缓冲无像素、点击落空)。按消息解析文字实体时**取最后匹配**
  (最新入队 = 活拷贝)。
- **验证文字亮显时相机会"侧对"地面文字**:沿地面平铺的文字从水平相机
  看投影面积为零——ID 缓冲 0 像素不代表 soup 丢失。用
  `GRID_SELECT_FOCUS=1`(钩子自带俯视)或先俯视再断言。
- **ID 纹理离线取证 = GRID_DUMP_ID=1**:约第 8 帧自动 blit+readTexture
  写 `id_dump_f<N>.bin`(w*h BGRA,16 位内 id = 低两字节),自闭环完成、
  不依赖点击 poll 链。配合按 id 上色可视化,一步看清"某实体的 ID 形状"。
- **点实体的 ID 拾取圆片必须镜像可见点的深度偏置**(2026-10-11):可见点
  在 drawTargetPointInstances 里带**半像素相机偏置**(透视分支,
  `max(0.5*pixelSizeWorld, 0.01)` 世界单位)使其浮在所标注的表面上;
  ID 拾取圆片若按真实位置入队,深度与表面打架(LEQUAL 靠舍数)会输给
  表面 → 点的 ID 在纹理中缺失/被面顶替,点击命中面而不是点。
  修法 = queueCadPoint 与 tiny impostor 四边形按同量偏置圆片
  (正交分支不偏置,镜像可见条件)。
- **ID 渲染目标内容不跨帧保留**(2026-10-11):bgfx RT 在未重新提交的
  帧内容失效(GRID_DUMP_ID 隔帧为空可证),"高亮继续采样上一帧 ID 纹理"
  的假设不成立——会闪烁。修法 = 双门控:有选中时 pass 用保留图元做
  纯 GPU 重渲(CPU 排队仍变化驱动);无选中才完全停转。若要彻底省掉
  这次 GPU 重渲,可 blit 到一张只采样的 sticky 纹理(未实施)。
- **转储/轮询不能等"下一次 pass 执行"**:门控后 pass 执行稀疏,
  GRID_DUMP_ID 的 issue+write 移到 endFrame 每帧 tick(见
  gpuPickIdDumpTick),issue 一次、readyFrame 后写盘。
- **构建期抑制不得跳过 ID 排队**(2026-10-11 回归):网格抑制初版把
  `continue` 放在 `queueGpuMeshEntity` 之前(CadMesh/应力网格两条路径),
  选中网格的 ID 汤没入队 → 无高亮,原始又被抑制 → **选中网格整体消失**
  (其它类别正常,因为它们的排队在独立块里)。修法 = 三处网格循环改为
  "跳过可见追加、保留 ID 排队"。通则:抑制的 `continue` 只许跳过可见
  绘制调用,任何 queue*/ID 构建必须照跑;新抑制点落笔前先回答
  "这个 continue 跳过了哪些调用?"。
- **GRID_SELECT_NAME 名字来自拾取注册表**:网格名是 `StressMeshN`
  (indexedDisplayName),不是 meshDisplayName 前缀;只有被排队过的
  (可见)实体才可按名选中。诊断:`GRID_PICK_DEBUG` 下钩子会一次性
  打印注册表前 12 个名字([SELECT_DEBUG])。GRID_SELECT_FOCUS 已支持
  网格(瞄准 worldPosition + 俯视 35°)。
- **"亮显+原始同时输出"的黄色描边**(2026-10-11):文字被选中后,原始
  SDF 字形的 AA 边缘像素落在 ID 字形掩码之外(present 9-tap 覆盖率为
  0),原色描边绕着高亮显示。修法 = 选中文字在可见通道跳过
  (`AcGsView::setTextSuppression` + flushTextRequests 过滤),输出完全
  交给 present 高亮。通用原则:替换式高亮要求被选中实体的原始绘制
  让位;只对"ID 掩码 != 可见几何"的类别需要(文字),掩码与几何重合的
  类别(ribbon 线、面/网格三角形)天然无泄漏。
- **自动化截屏必须先把光标停到窗外**(2026-10-11):ImGui 面板路由在
  `IsMouseReleased(Left)` 时发起面板拾取;残留的鼠标按下状态/位置会让
  无头启动的实例收到"幽灵释放"→ 触发 autofocus 滑相机(截图视角漂移,
  曾被误判为"选中导致相机漂移的 bug";stash A/B 证明与代码无关)。
  脚本启动前 `SetCursorPos(5,5)` 即可。
- **ID pass 门控后的验证指标**:sceneQueueActive/outlineId 的
  [PICK_DEBUG] 行只在值变化时打印,不能当"每帧状态"读。用
  GRID_DEBUG_PICK 的 [PICK_QUEUE](每秒一条,active 时)计数;或
  TEXT_PICK 行(排队帧才打印)。点击链路回归用
  GRID_CAMERA_TEST_DOUBLE_CLICK_AT_SECONDS/X/Y 合成点击(应用内真实
  SDL 事件路径),比外部 mouse_event 可靠。
- **presentIdDebug 解码的是 s_texColor(场景颜色),不是 ID 纹理**
  (fs_present.sc `presentIdDebug` 第一行读 s_texColor)。想"看 ID"时
  把 uPresentParams.w 置 1 只会得到把场景色当 ID 解码的红屏。
  正确姿势:K 键全屏 ID 视图(view 18 直接呈现 ID 纹理),或
  GRID_PICK_DEBUG 的点击像素读回。
- **GPU ID 注册表身份稳定,不再每帧清空**:ID 按实体身份复用,跨帧
  不漂移(老坑"描边停在第一个人身上/画到别的实体"已随统一 ID pass 关闭)。
  新代码不要再引入 per-frame clearRegistry。
- **实体解析对活体 tessellation**,注册表只做 ID↔身份映射,不存几何指针。
- **点击 ndc 是窗口系,ID 纹理是场景面板系**:cursorToNdcX/Y 用整个
  SDL 窗口尺寸,而 ID texel 按 ndc×场景尺寸采样——ImGui 面板有内边距
  时存在系统偏移(自动化点击小目标会脱靶;点击大目标无感)。
  自动化验证要么用大目标,要么先校准偏移;这也是潜在的待修偏移 bug。
- **半无限构造线**(Ray/XLine,semiInfinite)沿方向无限延伸拾取是语义
  正确的;"点哪里都选中它"不是 bug。
- **面板拾取不触发自动对焦**:单击选中+描边;双击自动对焦是 legacy 行为。

## 交互与事件

- **ImGui 事件桥必须接**:SDL3 事件先喂 `ImGui_ImplSDL3_ProcessEvent`,
  否则症状是"应用聚焦时面板全死,失焦反而活"。
- **后台进程抢前台**:SetForegroundWindow 会被前台锁拒绝;先
  keybd_event(VK_MENU) 按一下 ALT 再 SetForegroundWindow 可解锁。
  自动化点击(PrintWindow 截图 + mouse_event)对 SDL 窗口有效,
  但要真实按键/鼠标(SDL 忽略合成消息的老结论只适用于部分路径)。
- **截图坐标系**:PrintWindow 给物理像素(含非客户区边框),SDL 事件
  给逻辑点,DPI≠100% 时两者差 scale;ndc 是比例所以点击不受 DPI 影响,
  受窗口边框/面板内边距影响(见上条)。
- **taskkill //F 丢 stdout 缓冲**:诊断日志必须显式 flush。
- **views()[n] 越界 = 调试器断点冻结整个应用**:第二视口惰性创建,
  访问前查 viewCount("相机锁死"真身)。

## 视口与显示

- **帧中重建目标 = 渲染被裁剪 + 残影**(帧首铁律,见 SKILL.md)。
- **正交射线**:NDC 平移的是射线原点(跨视平面),不是倾斜方向。
- **DPI**:ImGui DisplaySize 逻辑点 vs 后备缓冲物理像素,draw data 乘
  FramebufferScale。
- **静态引用绑定视图子对象禁止**(`AcGsOrbitCamera &cam = ...()`),
  用访问器;views() 越界即断点冻结。
- **文字/轮廓合成进场景离屏目标(view 12/17)**:backbuffer 空间的
  glyph 会被面板内边距位移并在窗口边被裁;SDF 文字无深度测试=永远置顶,
  SHX 描边文字=普通 stroke 有深度。两者遮挡行为不同是设计使然。
- **交换链孤儿行**:present 的 viewRect 必须跟 swapchain 尺寸而非场景
  目标尺寸,否则渲染窗底部出现"重叠边框"条带。

## mesh 高亮重绘(描边)

- eye 约定按目标路径:应力对象路径 eye=相机位置(RTE);cadmesh/tess
  路径 eye=rebase 原点。用错 = 高亮偏移 (相机−重原点),大坐标下远超屏幕。
- 描边 if-链按 kind 分支(CadFill/CadPoint/CadStroke/CadCurve/MeshObject),
  新增实体类型补分支——漏分支症状是"选中后无任何显示"。

## 设备与帧泵

- **bgfx view 配置每帧"最后设置者生效"**:多场景共用 viewId 会互相
  覆盖;用通道基址偏移或独立 view 区间。
- **bgfx 调试文本永远渲染进 view 0**:ImGui 模式下会进面板图像;
  窗口级悬浮字用宿主 ImGui 画。
- **`struct ImDrawData` 前置声明必须在全局命名空间**,否则各命名空间
  各自成为独立类型,override 静默失配。
- **Dirty 门控**:改场景/样式/相机后记得 invalidate;否则静态场景
  不重渲,排障时"改了没反应"先怀疑门控。

## 工程环境(bash + MSVC + 自动化)

- **Git Bash heredoc 会折叠 `\\0` 为真 NUL**;含反斜杠转义的编辑用
  Edit 工具或数值构造。
- **混合行尾**:python 批量改文件前先统一 `replace('\r\n','\n')`,
  写盘明确 newline。
- **批量脚本断言失败 = 半途弃写**:全部成功后统一写盘。
- **大段删除锚定"下一个真实方法"**,删完 diff 复查。
- **LNK1163/LNK1104**:陈旧 obj 删除重编;exe 被占用先 taskkill。
- **默认参数只能出现在前向声明**(C2572)。
- **像素级验证工作流**(2026-10-10 实战有效):两张全窗截图
  `ImageChops.difference` + 放大阈值,把"哪些像素变了"压缩成一张图;
  对变化像素做颜色代数(如 0.52A+0.48B)可直接指认合成公式与来源
  pass。比逐 shader 猜快一个数量级。
