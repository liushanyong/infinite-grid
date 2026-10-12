# CAD 图标设计规范（原创几何 · v2）

**许可声明**：本目录下所有 SVG 为原创几何。设计**思路**参考了业界成熟 CAD 工具的图标语言惯例
（24px 网格、线宽/色值分级、夹点语义、语义分族）。所有具体路径坐标均为本项目独立设计，
未描摹、未改写、未导入任何第三方图标的路径数据或像素。

---

## 一、v2 的依据：实测而非目测

对参考图标集做了像素级统计（`tools/ref_measure.py`），并用同一口径回测了我们自己的
两版渲染（`tools/coverage.py`）：

| 指标 | 参考实测 | v1 现状 | **v2 实测** |
| --- | --- | --- | --- |
| 留白（背景占比，中位） | 66–94%，中位 **82%** | 75.7% | **79.4%** |
| accent 面积占比（中位） | **4.69%** | 4.75%（本来就已经贴住了） | **4.48%** |
| accent 面积占比（最大） | 38.96% | 16.42% | 15.56% |
| 中性色墨量（中位） | 5–30% | 19.6% | 17.8% |
| 总墨量（中位） | — | 24.3% | **20.6%** |

> 分类用 `b - r > 50`。曾经的 `> 18` 是错的：我们的近白 neutral 恰好落在
> `b-r == 18`，会把全部中性色墨量误算成 accent，中位数虚高到 22.8%。
> `tools/coverage.py` 和 `tools/ref_measure.py` 现在共用这一个阈值。

### 关键结论

**第一版的问题不是"accent 太多"。** 上表显示 v1 的 accent 面积中位数 4.75%，
和参考集的 4.69% 几乎一样。所以差距来自另外三件事：

1. **中性色太暗。** v1 用 `#B9C4D4`——一个偏灰的蓝。在深色 ribbon 上它只有中等
   对比度，整套图标读起来发虚、发闷。参考集用的是**近乎纯白**的轮廓线。
   改到 `#D8E0EA` 之后细线才真正"脆"。
2. **形体语言错了。** v1 把大量形体画成**实心块面**，参考集里只有**有体积的东西**
   （立方体、手、文档、对话框、漏斗）才用实心，几何命令一律是细轮廓 + 夹点。
   同样的墨量，画成大灰块和画成细白线，视觉重量完全不同——这是"质感"差距的主因。
3. **accent 落错了位置。** 参考集把蓝色放在**夹点和数据**上：直线端点的方 grip、
   圆的定心半径、标注的跨距、图层的当前层。v1 的蓝色常落在"这块形状比较重要"，
   属于构图判断而不是 CAD 语义。

另外两条直接照搬的惯例：**大方 grip 而非圆点**（CAD 抓取点的视觉约定，且 26px 下更利落）、
**能删就删**（参考集的 `arc3` 只有弧+角楔+十字+一个 grip）。


---

## 二、网格与色板

| 项 | 值 |
| --- | --- |
| viewBox | `0 0 24 24` |
| 描边宽度 | **主 1.4**；强调层 1.75 |
| 端点/连接 | `round` |
| 圆角 | `rx="0.8"`（框）、`rx="0.6"`（小形体） |
| 图元数 | 4–9，够表达就停 |

### 颜色角色

SVG 里写死这三个值，转换器据此判角色（`tools/svg2icon.py::ROLE_TOKENS`），
运行时实际着色由 `ui::MakeToolbarPalette()` 提供——两者必须保持一致。

| 角色 | 值 | 用途 |
| --- | --- | --- |
| `neutral` | `#D8E0EA` | 图标主体轮廓（近白，保证 26px 下的清脆度） |
| `accent` | `#4FA3F2` | **恰好一处语义焦点**，面积必须小 |
| `muted` | `#8B96A6` | 构造线 / 虚线 / 推导结果 |

---

## 三、语义分族（v2 核心）

### A 族 · 构造类（几何与参数命令）

`line` `polyline` `circle` `arc` `rect` `text` `dim_linear` `leader` `measure`
`move` `rotate` `scale` `zoom_ext` `layiso` `ortho` `osnap`
`match_prop` `group` `ungroup` `copy_clip`

**硬约束**
- **禁止大面积实心块面**。形体靠 `stroke`，`fill` 只允许给 ≤ 7% 的小夹点。
- **夹点用实心小方块**（side 2.0–2.6，rx 0.4），不是圆——方 grip 是 CAD 抓取点的
  视觉约定，且在 26px 下比圆点更利落。
- accent 面积 **≤ 7%**，且必须落在**同一语义焦点**上（允许由多个图元组成，
  例如"圆心 + 半径"算一个焦点）。

### B 族 · 实体类（有物理体积的文档与容器）

`doc_new` `folder_open` `save` `copy` `paste` `layers` `table` `viewport` `filter`
`block_make` `block_insert`

**硬约束**
- 允许实心填充作为主体（实心=有体积，这是参考集的真实做法）。
- accent 面积 **≤ 18%**，只用来标注"当前 / 激活 / 主体"的那一个面或元素。
- 至少 40% 画布必须留空，否则 26px 下糊成一坨。

### C 族 · 通用 UI 符号

`menu` `plus` `gear`

纯符号，靠剪影识别，最简即可。accent 只允许一处极小标记。

---

## 四、尺寸分级

工具栏 26px、状态栏 17px，**必须按用途占不同画布比例**：

| 用途 | 最大边 |
| --- | --- |
| 工具栏图标（26px） | **18–22** |
| 状态栏图标（17px） | **14–18** |

用 `tools/audit_icons.py` 验证（几何 + 半描边宽度膨胀后的光学包围盒）。

### 旋转图标的换算

45° 旋转会把 `(w, h)` 包围盒压成 `(w+h)/√2`。**旋转后要达到 19，旋转前需要 L+W ≈ 27**
的窄长比例，否则尺寸审计会判越界。

---

## 五、验收流程

```sh
# 转换 + 接触表（2 倍看构图，1 倍看 26px 可辨识度）
python tools/svg2icon.py --map tools/icon_map.json --src resources/icons \
    --out ports/OpenCADImGuiPort/src/ui/IconGlyphs.hpp \
    --preview build2022/icon_preview_2x.png --cell 56 --cols 6

# 带图标名的联络表，逐个核对用
python tools/label_sheet.py --cell 64 --cols 6 --out build2022/icon_labels.png

# 尺寸 / 重心审计
python tools/audit_icons.py --map tools/icon_map.json --src resources/icons

# 墨量实测：留白 / accent / neutral 占比（与参考集同口径）
python tools/coverage.py --sheet build2022/icon_preview_2x.png --label v2
python tools/ref_measure.py "<cui>/cmd_icon_dark"
```

**自检清单**（逐个图标）：
- [ ] accent 落在一个语义焦点上（夹点 / 数据），面积在所属分族的预算内
- [ ] A 族无大块面；B 族实心只用于"有体积的东西"
- [ ] 最大边落在 §4 对应区间
- [ ] 2 倍联络表里一眼认出是什么 CAD 命令
- [ ] 1 倍（26px / 17px）下细节不糊，线条不虚

---

## 六、文件

全部 36 个图标。映射表 `tools/icon_map.json` 决定哪个 ID 用哪个 SVG。

### 工具栏 · 26px · 最大边 18–22

| 文件 | 图标 | 族 |
| --- | --- | --- |
| `line.svg` | 直线 | A |
| `polyline.svg` | 多段线 | A |
| `circle.svg` | 圆 | A |
| `arc.svg` | 弧 | A |
| `rect.svg` | 矩形 | A |
| `text.svg` | 文字 | A |
| `dim_linear.svg` | 线性标注 | A |
| `leader.svg` | 引线标注 | A |
| `measure.svg` | 测量 | A |
| `move.svg` | 移动 | A |
| `rotate.svg` | 旋转 | A |
| `scale.svg` | 缩放 | A |
| `match_prop.svg` | 匹配特性 | A |
| `group.svg` | 组合 | A |
| `ungroup.svg` | 取消组合 | A |
| `copy_clip.svg` | 复制到剪贴板 | A |
| `doc_new.svg` | 新建 | B |
| `folder_open.svg` | 打开 | B |
| `save.svg` | 保存 | B |
| `copy.svg` | 复制 | B |
| `paste.svg` | 粘贴 | B |
| `layers.svg` | 图层 | B |
| `block_make.svg` | 创建块 | B |
| `block_insert.svg` | 插入块 | B |
| `table.svg` | 列表 | B |

### 状态栏 · 17px · 最大边 14–18

| 文件 | 图标 | 族 |
| --- | --- | --- |
| `menu.svg` | 菜单 | C |
| `plus.svg` | 加号 | C |
| `gear.svg` | 设置 | C |
| `ortho.svg` | 正交 | A |
| `osnap.svg` | 对象捕捉 | A |
| `layiso.svg` | 孤立图层 | A |
| `zoom_ext.svg` | 范围缩放 | A |
| `viewport.svg` | 视口 | B |
| `filter.svg` | 过滤 | B |
| `table.svg` | 列表 | B |
