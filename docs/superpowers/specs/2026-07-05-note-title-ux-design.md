# 便签标题体验改进 — 设计

日期:2026-07-05
状态:待评审

## 问题

便签窗标题栏当前显示 `noteTitleText()` 的回落结果 = 内容第一行(前 40 字节),
与紧邻其下的正文首行完全重复,体验差(用户反馈)。
根源:`Note.title` 字段自建库起就存在,但全程序没有任何设置入口,永远为空,
标题永远走"首行回落"分支。

## 方案(对齐主流便签产品:MS Sticky Notes / macOS Stickies / Zhorn)

按窗口状态区分标题栏文案:

| 场景 | 显示内容 |
|------|---------|
| 便签窗**展开** | 仅自定义标题;`title` 为空则**留空**(纯拖动区,不画首行、不画 #id) |
| 便签窗**卷起** | 自定义标题 → 回落内容首行 → 回落 "(无标题)"(卷起后只剩标题栏,需要可识别) |
| 管理器列表 | 维持现状:`noteTitleText()`(标题 → 首行 → "(无标题)") |

补齐改标题入口(两处,共用现有 `own_ui::promptText` 弹窗):

1. 管理器列表右键菜单新增「重命名…」(命令号 4,放在「打开」之后);
   确认后 `NoteStore::updateTitle`,`reload()` 刷列表,`m_host->refreshNoteWindow(id)` 刷已开窗口。
2. 便签窗标题栏拖动区**双击**弹重命名(当前无双击处理,无冲突);
   确认后更新 `m_note.title`、落库、`Invalidate` 重画。
   弹窗预填当前标题;清空确认 = 清除自定义标题(回到留空/首行回落)。

## 改动点

- `src/domain/NoteListFormat.{h,cpp}`:新增
  `std::string noteWindowTitleText(const Note& n, bool rolledUp)`,
  纯域函数,规则如上表。`noteTitleText` 不动(列表/排序继续用)。
- `src/data/NoteStore.{h,cpp}`:新增 `void updateTitle(int64_t id, const std::string& titleU8)`,
  只 UPDATE `title` 列,**不动 `updated_at`**(重命名不算内容编辑,不扰动"按更新时间排序")。
- `src/ui/NoteWindow.cpp`:标题栏绘制(约 156-170 行)改用 `noteWindowTitleText(m_note, m_note.rolledUp)`;
  新增 `WM_LBUTTONDBLCLK` 处理:命中拖动区 → 重命名弹窗。
- `src/ui/NoteListView.cpp`:右键菜单加「重命名…」及处理分支。

## 不做

- 不在数据库层生成/固化标题(保持 title 为用户显式输入,空 = 未设置)。
- 不改管理器列表与排序逻辑。
- 不做标题栏内联编辑框(弹窗够用,与新建分组/标签交互一致)。

## 测试

- doctest(域层,Linux/CI 可跑):`noteWindowTitleText` 全分支 —
  有标题×展开/卷起、无标题×展开(空串)、无标题有内容×卷起(首行)、全空×卷起("(无标题)")、
  首行 40 字节截断与现有 `noteTitleText` 一致。
- Windows 冒烟:展开无重复文案;卷起可识别;两处重命名入口生效;
  清空标题恢复回落;重启后标题持久;列表排序不受重命名影响。
