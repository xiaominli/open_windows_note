# P5 手工冒烟清单（托盘 + 开机自启 + 全局热键）
构建: MSBuild open_windows_note.sln -p:Configuration=Debug -p:Platform=x64
运行: x64/Debug/open_windows_note.exe

## 系统托盘
- [ ] 启动后通知区出现托盘图标（悬停提示 open_windows_note）
- [ ] 左键双击托盘：切换管理器窗口显隐
- [ ] 右键托盘弹菜单：新建便签 / 显示管理器 / 显示全部 / 隐藏全部 / 开机自启(可勾) / 退出
- [ ] 菜单“新建便签/显示管理器/显示全部/隐藏全部/退出”各自生效
- [ ] 杀掉并重启 explorer.exe 后，托盘图标自动重新出现（TaskbarCreated 自愈）

## 全局热键
- [ ] Ctrl+Alt+N 新建富文本 / Ctrl+Alt+2 清单 / Ctrl+Alt+3 涂鸦
- [ ] Ctrl+Alt+M 切换管理器 / Ctrl+Alt+H 显隐全部 / Ctrl+Alt+Q 退出
- [ ] 在 settings 表把 hotkey.new 改成别的组合（如 Ctrl+Alt+J）后重启：新键生效
- [ ] 两个绑定填成相同组合：后者被跳过（前者仍可用，程序不崩，DebugView 有 skip 日志）

## 开机自启
- [ ] 托盘菜单勾“开机自启”：启动夹（shell:startup）出现 open_windows_note.lnk，指向本 exe
- [ ] 取消勾选：该 .lnk 被删除
- [ ] 重启菜单，勾选态与实际 .lnk 一致
