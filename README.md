# open_windows_note

Windows 桌面便签（Sticky Notes）。C++17 / MFC 静态链接，单 exe 便携免安装，SQLite 持久化。MIT 开源。

## 功能（v1）

- 三种便签类型：**富文本**（B/I/U/S、字号、文字色工具条）、**清单/待办**、**手绘涂鸦**
- 悬浮置顶便签窗：拖动、缩放、卷起、per-note 置顶、4 档不透明度、4 套配色主题一键循环
- 管理器窗口：全部便签列表、跨便签搜索、分组、标签
- 提醒/闹钟：单次与每日/每周/每月重复、贪睡、自定义提示音
- **贴到应用窗口**：便签跟随目标应用显隐（标题子串或 `class:类名` 匹配）
- 全局热键（可在设置里改键）、系统托盘常驻、开机自启（启动文件夹 .lnk，便携友好）
- 设置弹层：默认主题 / 默认透明度 / 默认字号 / 自启 / 热键
- **备份**：托盘一键导出（`VACUUM INTO` 一致性快照）与导入（校验 → 原库留 .bak → 原子替换 → 自动重启）

## 构建

- Visual Studio 2022（v143，含 MFC），仅 x64。
- 打开 `open_windows_note.sln`，选 `Debug|x64` 或 `Release|x64` 构建。
- 或命令行：`MSBuild open_windows_note.sln -p:Configuration=Release -p:Platform=x64`
- 测试：构建后运行 `x64/<配置>/tests.exe`（doctest，全绿为准）。

## 运行 / 便携

- 直接运行 `open_windows_note.exe`。数据库 `notes.db` 存于 exe 同目录（不可写时回落 `%APPDATA%\open_windows_note\`），拷走目录即带走全部数据。
- 单实例：重复启动会通知已有实例新建一条便签。

## 默认热键（设置里可改）

| 热键 | 动作 |
|---|---|
| Ctrl+Alt+N | 新建便签 |
| Ctrl+Alt+2 | 新建清单 |
| Ctrl+Alt+3 | 新建涂鸦 |
| Ctrl+Alt+M | 显示/隐藏管理器 |
| Ctrl+Alt+H | 显示/隐藏全部便签 |
| Ctrl+Alt+Q | 退出 |

## 目录结构

- `src/domain` 纯逻辑（无 Win32 依赖，随 tests 编译）
- `src/data` SQLite 数据层（Database/NoteStore/迁移/备份）
- `src/services` 托盘、热键、自启、提醒调度、贴窗监听
- `src/ui` 自绘窗口（便签、管理器列表、设置、提示框、toast）
- `src/app` 应用装配（NoteApp/宿主窗/路径）
- `tests` doctest 测试工程

## License

MIT
