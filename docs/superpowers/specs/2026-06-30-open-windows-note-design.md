# open_windows_note —— 桌面便签(Sticky Notes)设计文档

- **状态**:已定稿,待实现
- **日期**:2026-06-30
- **仓库**:https://github.com/xiaominli/open_windows_note （开源,MIT）
- **平台**:Windows（Win10+，x64）

---

## 1. 概述

一个 Windows 桌面便签程序:可增删改查 note,每条 note 以无边框、置顶、可透明的小窗悬浮在桌面上;另有一个主管理窗口统一列表、搜索与组织。纯 C++ / MFC 实现,单文件便携,数据存本地 SQLite。

### 1.1 技术栈（已定案）

| 项 | 选择 | 理由 |
|---|---|---|
| 语言 / GUI | C++ / **MFC，静态链接** | 单 exe 免安装便携,无 MFC DLL 依赖 |
| 数据库 | **SQLite**（官方 amalgamation `sqlite3.c` 编译进 exe） | 无外部依赖 |
| 渲染 | **GDI + GDI+** | 与既有自绘表格控件 `SWTableScrollViewWnd` 同栈;与 RichEdit 原生集成;便签非图形密集,D2D 收益边际小。**D3D 否决** |
| 文本编辑 | 内嵌 **RichEdit** 控件（包在自绘边框内） | 避开自写文本引擎 / IME 深坑 |
| UI 绘制 | **一律自绘**（自 `OnPaint` 画列表/按钮/工具栏/滚动条/标题栏/背景）；唯一例外是正在编辑的 RichEdit | 用户硬约束 |
| License | MIT | 宽松、商用友好 |

### 1.2 窗口模型

- **一个主管理窗口**（`CMainFrame`）：自绘 note 列表 + 搜索 + 托盘宿主 + 全局命令。
- **N 个悬浮便签窗**（`CNoteWindow`）：每条打开的 note 一个顶层窗口，无边框、`WS_EX_LAYERED`（透明）、`WS_EX_TOPMOST`（置顶）。
- 两种视图共存；从列表双击“贴到桌面”打开便签窗。

### 1.3 v1 功能范围（全部纳入）

增删改查 · 一条 note 一个悬浮置顶窗 · 拖动 / 调整大小 · 背景色 + 多套配色主题 · 不透明度调节 · 卷起 / 折叠 · per-note 置顶开关 · 多显示器位置记忆 · 自定义字体字号 · **富文本** · 跨 note 搜索 · 一键隐藏 / 显示全部 · 文件夹 / 分组 · 标签 tags · 提醒 / 闹钟（重复 + 贪睡 + 自定义提示音）· **清单 / 待办 note 类型** · **手绘涂鸦 note 类型** · 全局热键 · 开机自启 · 便携（免安装）· 导入 / 导出备份 · **贴到应用窗口**（按标题 / 类名匹配；网页 URL 匹配后置到 v2）· SQLite 持久化 · 系统托盘常驻。

---

## 2. 架构

### 2.1 分层（依赖只向下）

```
┌─ 表现层 (MFC, 全自绘, GDI/GDI+) ──────────────────────────┐
│  CNoteApp(CWinApp 入口)                                   │
│  CMainFrame ── 托盘/热键宿主、管理窗口                       │
│     ├ CNoteListView   → 复用 SWTableScrollViewWnd 回调       │
│     ├ CSearchBox      (自绘搜索框，内嵌小 Edit)             │
│     └ CFlatButton / CCustomScrollBar (自绘通用控件)         │
│  CNoteWindow ── 顶层 无边框/分层透明/置顶 便签窗            │
│     └ INoteContentView                                     │
│         ├ TextContentView     (内嵌 RichEdit)              │
│         ├ ChecklistContentView(自绘勾选行, 编辑用就地 Edit) │
│         └ DrawingContentView  (自绘 GDI+ 画布)             │
│  弹层: 设置 / 提醒编辑 / 分组标签 / 主题选择 (自绘 CWnd)     │
├─ 服务层 ────────────────────────────────────────────────┤
│  ReminderScheduler  HotkeyManager  StickyWindowWatcher     │
│  AutostartManager   BackupService  SettingsStore           │
├─ 领域层 ────────────────────────────────────────────────┤
│  Note NoteType ChecklistItem Stroke Tag Group Reminder Theme│
├─ 数据层 ────────────────────────────────────────────────┤
│  NoteStore(仓储, 所有 SQL 集中于此)                        │
│  Database(sqlite3* RAII + 预编译语句 + 事务 + 迁移)        │
└──────────────────────────────────────────────────────────┘
```

**分层原则**:领域 / 数据 / 服务层不依赖 HWND，从而可脱离 UI 单元测试。表现层只做绘制与交互，业务逻辑下沉。

### 2.2 架构方案

采用「管理器 + N 个 note 窗口 + 可插拔内容视图」。每条 note 是独立顶层窗口，内部宿主一个多态 `INoteContentView`，按 `note.type` 选择实现。新增 note 类型 = 新增一个内容视图，不改动窗口框架。

---

## 3. 组件职责

| 组件 | 职责 | 依赖 |
|---|---|---|
| `CNoteApp` | 入口:打开 DB→迁移→加载 note→建主窗→消息泵;单实例互斥 | Database, NoteStore, CMainFrame |
| `CMainFrame` | 管理窗口容器;托盘 `Shell_NotifyIcon`;右键菜单;新建/显隐全部/设置/导入导出 的总调度 | NoteStore, 各服务 |
| `CNoteListView` | **复用 `SWTableScrollViewWnd`**,实现 `ISWTableScrollViewWndCallback`:`onTableScrollViewDrawCell` 自绘每行(色块/标题摘要/分组/标签/提醒图标/更新时间);`onTableScrollViewPrepareDraw` 加锁快照筛选后的列表;双击开 note;右键菜单 | SWTableScrollViewWnd, NoteStore |
| `CNoteWindow` | 顶层便签窗:**自绘**标题栏(拖动/卷起/关闭/换色/pin/透明度滑块)、resize 抓手、背景;`WS_EX_LAYERED`+`UpdateLayeredWindow` 透明;`WS_EX_TOPMOST` 置顶;几何/透明/pin/卷起 变化即持久化 | NoteStore, 一个 INoteContentView |
| `INoteContentView` | 内容视图接口:`Load(note)` / `Save()→(blob, plain_text)` / `OnPaint(hdc)` / `OnResize` / `IsDirty` | — |
| `TextContentView` | 宿主 RichEdit(RICHEDIT50W);RTF 读写;字体/字号;粗体斜体下划线列表工具条 | RichEdit |
| `ChecklistContentView` | 自绘勾选行;单项编辑用 `CSWInplaceEdit`;增删/勾选/拖动排序;序列化为 JSON | GDI, CSWInplaceEdit |
| `DrawingContentView` | 自绘 GDI+ 画布;鼠标捕获画矢量笔迹;画笔颜色/粗细/橡皮;序列化为笔迹 JSON | GDI+ |
| `NoteStore` | 仓储:note/分组/标签/提醒 的增删改查、搜索;返回领域对象;写操作走事务 | Database |
| `Database` | `sqlite3*` RAII;预编译语句缓存;事务;按 `PRAGMA user_version` 迁移;启动 `integrity_check` | sqlite3.c |
| `ReminderScheduler` | UI 线程 `SetTimer` 轮询到期提醒→自绘通知窗+`PlaySound`;`computeNextDue` 处理重复/贪睡 | NoteStore |
| `HotkeyManager` | `RegisterHotKey` 全局热键(新建/显隐全部)→路由 `WM_HOTKEY`;冲突则提示改键 | CMainFrame |
| `StickyWindowWatcher` | `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)`;前台窗口标题/类名匹配 `stick_target`→显隐对应 note;失败优雅降级 | NoteStore |
| `AutostartManager` | 开机自启:启动文件夹 `.lnk`(保持便携少碰注册表) | — |
| `BackupService` | 导出=复制 DB 或序列化 JSON;导入=校验后 替换/合并 | Database |
| `SettingsStore` | 应用设置(默认主题/透明度/热键/自启);存 DB `settings` 表 | Database |

---

## 4. 数据模型（SQLite）

数据库文件名 **`notes.db`**;便携路径 `<exe目录>\notes.db`,不可写则回落 `%APPDATA%\open_windows_note\notes.db`。

```sql
notes(
  id INTEGER PRIMARY KEY,
  type INTEGER NOT NULL,          -- 0=富文本 1=清单 2=涂鸦
  title TEXT,                     -- 可选/派生标题
  content_blob BLOB,              -- 富文本=RTF; 清单=JSON items; 涂鸦=JSON 笔迹
  plain_text TEXT,                -- 搜索缓存(小写纯文本)
  theme_id INTEGER,               -- 配色方案 FK
  group_id INTEGER,               -- 分组 FK, 可空
  pos_x INTEGER, pos_y INTEGER, width INTEGER, height INTEGER,
  monitor_id TEXT,                -- 显示器设备名, 多屏还原
  opacity INTEGER DEFAULT 255,    -- 0..255, WS_EX_LAYERED alpha
  pinned INTEGER DEFAULT 1,       -- per-note 置顶
  rolled_up INTEGER DEFAULT 0,    -- 卷起
  visible INTEGER DEFAULT 1,      -- 桌面显示/隐藏
  stick_target TEXT,              -- 贴到的窗口 标题/类名 模式, 可空
  created_at INTEGER, updated_at INTEGER
);
groups(id INTEGER PRIMARY KEY, name TEXT, order_idx INTEGER);
tags(id INTEGER PRIMARY KEY, name TEXT UNIQUE);
note_tags(note_id INTEGER, tag_id INTEGER, PRIMARY KEY(note_id, tag_id));
reminders(
  id INTEGER PRIMARY KEY, note_id INTEGER NOT NULL,
  due_at INTEGER NOT NULL,        -- 下次触发(unix)
  recurrence INTEGER DEFAULT 0,   -- 0无 1每天 2每周 3每月
  recur_interval INTEGER DEFAULT 1,
  snooze_until INTEGER, sound_path TEXT, enabled INTEGER DEFAULT 1
);
themes(id INTEGER PRIMARY KEY, name TEXT, bg_color INTEGER,
       title_color INTEGER, text_color INTEGER, is_builtin INTEGER);
settings(key TEXT PRIMARY KEY, value TEXT);
```

**索引**:`notes(group_id)`、`notes(visible)`、`note_tags(tag_id)`、`reminders(due_at, enabled)`。

**搜索**:v1 用 `plain_text LIKE`;数据量大时升级 FTS5（amalgamation 编译带 `SQLITE_ENABLE_FTS5`）。

**内容 blob 约定**:
- 富文本 = RTF 字节流。
- 清单 = `[{ "text": "...", "checked": true, "order": 0 }, ...]`。
- 涂鸦 = `{ "strokes": [ { "color": 0xRRGGBB, "width": 3, "points": [[x,y],...] } ] }`（**矢量笔迹**，可再编辑、可缩放）。
- 三者都同步维护 `plain_text` 供搜索（涂鸦为空或 OCR 后置到 v2）。

---

## 5. 数据流

1. **启动**:定位 `notes.db`→`Database.open`→`integrity_check`→迁移建表→播种内置主题→读 settings→`NoteStore.query(visible=1)`→每条建一个 `CNoteWindow` 还原几何/透明/置顶/卷起→`ReminderScheduler` 装载到期项→注册热键 / WinEvent hook / 托盘。
2. **编辑内容**:内容视图置脏→防抖(失焦或空闲 ~800ms)→`CNoteWindow.Save()`→`NoteStore.updateContent(id, blob, plain_text)`(事务)。
3. **窗口状态变化**(移动/缩放/透明/置顶/换色/卷起):即时或防抖→`NoteStore.updateState`。
4. **管理列表**:`setTotalRowCount(筛选后数量)`;`PrepareDraw` 快照筛选+排序后的向量;`drawCell` 画列;双击→打开/聚焦 `CNoteWindow`;右键→删除/改分组/加标签/设提醒/贴窗口/复制。
5. **搜索**:`CSearchBox` 输入→`plain_text LIKE` 过滤→刷新 `totalRowCount` 并重绘。
6. **提醒触发**:定时器→命中(`due_at<=now && enabled && 过snooze`)→自绘通知窗+`PlaySound`(缺失回落 `MessageBeep`)→打开/贪睡(重算 `snooze_until`)/关闭(重复算下次 `due_at`,否则 `enabled=0`)。
7. **贴窗口**:`SetWinEventHook(EVENT_SYSTEM_FOREGROUND)`→前台窗口标题/类名匹配 `stick_target`→显/隐对应 note。
8. **热键**:`WM_HOTKEY`→`CMainFrame`(新建 note / 显隐全部)。
9. **备份**:导出=复制 DB 文件 或 JSON 序列化;导入=校验后 替换/合并。
10. **退出**:落盘待存→保存窗口状态→删除托盘图标→注销热键/hook→关 DB。

**线程约定**:v1 把 SQLite 访问、提醒定时器、WinEvent 回调**全部收在 UI 线程**(单连接、无锁)。后续要后台化再引入串行队列。

---

## 6. 错误处理

| 场景 | 处理 |
|---|---|
| DB 打开失败 | 便携路径→回落 `%APPDATA%`→再失败弹错误让用户选位置,不静默崩 |
| DB 损坏 | 启动 `PRAGMA integrity_check`;损坏则坏库改名 `.corrupt` 备份后重建,并告知用户 |
| 迁移失败 | 迁移前先 `.bak` 整库快照;失败回滚并恢复备份 |
| 内容保存失败 | 保留脏标志重试;退出前若有未保存则提示 |
| note 跑到屏幕外 | 还原时用 `EnumDisplayMonitors` 把几何**钳制回可见工作区** |
| RTF/JSON 解析失败 | 富文本回落纯文本;清单/涂鸦解析失败当空处理,**保留原始 blob** 防丢数据 |
| 热键注册冲突 | `RegisterHotKey` 失败→提示并允许改键 |
| `SetWinEventHook` 失败 | 贴窗口功能优雅降级(记日志、禁用该特性) |
| 资源管理器重启 | 收到 `TaskbarCreated` 消息→重新添加托盘图标 |
| 提示音文件缺失 | 回落 `MessageBeep` |
| 重复启动 | 命名 `Mutex` 单实例;第二次启动聚焦已有实例后退出,避免两进程同库 |

轻量文件日志(exe 旁或 `%APPDATA%`,分级),包装 `OutputDebugString`。

---

## 7. 测试策略

- **可单测核心(无 HWND)**:`Database`+`NoteStore` 跑内存/临时 SQLite → CRUD、搜索、迁移、清单/笔迹 JSON 序列化往返、off-screen 钳制、LIKE 搜索。
- **提醒逻辑**做成纯函数 `computeNextDue(reminder, now)`,`now` 参数注入(不硬编码当前时间)→ 每天/每周/每月/间隔/贪睡 全覆盖。
- 测试框架 **doctest**(单头文件、零依赖),独立 console 测试工程链接 数据/领域/服务 层。
- **手工冒烟清单**:三种 note 类型创建;拖动/缩放/透明/置顶/卷起;多屏还原;托盘显隐;热键;提醒触发;贴窗口;导入导出往返;U 盘便携运行;重启后持久化。
- 可选 GitHub Actions(Windows runner:MSBuild 构建 + 跑 doctest)。

---

## 8. 仓库骨架

```
open_windows_note/
├─ LICENSE                 (MIT)
├─ README.md
├─ .gitignore             (docs/temp/, Debug/, Release/, x64/, .vs/, *.db, *.pdb, *.user …)
├─ open_windows_note.sln
├─ src/                   (app: MFC 静态链接)
│   ├─ presentation/  services/  domain/  data/
│   └─ third_party/sqlite/ (sqlite3.c/.h)
├─ tests/                 (console + doctest)
└─ docs/
    ├─ superpowers/specs/ (本设计文档)
    └─ temp/              (★不进 GitHub:自绘表格控件等参考资料)
```

**`.gitignore` 要点**:排除 `docs/temp/`、构建产物(`Debug/ Release/ x64/ .vs/ *.obj *.pdb`)、`*.db`、`*.db-journal`、`*.user`。

> 备注:参考控件 `SWTableScrollViewWnd.cpp/.h` 目前在仓库根目录;实现阶段应移入 `docs/temp/`(不入库)或作为受版权约束的参考,由实现计划确认其去向与授权。

---

## 9. 版本路线图（v1 之后）

- **v2**:贴到网页 URL(UI Automation 读地址栏)、云同步/多端、密码加密、局域网发送 note、涂鸦 OCR、皮肤/声音生态、FTS5 搜索。

---

## 10. 已定决策清单

1. C++ / MFC 静态链接,单 exe 便携。
2. SQLite amalgamation 编译进 exe,库名 `notes.db`。
3. 渲染 GDI + GDI+;文本编辑内嵌 RichEdit;其余 UI 一律自绘。
4. 窗口模型:主管理窗 + N 个悬浮置顶便签窗。
5. `CNoteListView` 复用 `SWTableScrollViewWnd`。
6. 三种 note 类型:富文本 / 清单 / 涂鸦,经 `INoteContentView` 多态。
7. v1 全功能(见 1.3);贴场景仅“贴到应用窗口”。
8. 单线程 DB 访问(v1)。
9. License MIT;开源仓库 `xiaominli/open_windows_note`。
10. `docs/temp/` 不入库。
