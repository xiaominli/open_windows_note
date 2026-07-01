# P2 手工冒烟清单（open_windows_note.exe）
构建: MSBuild open_windows_note.sln -p:Configuration=Debug -p:Platform=x64
运行: x64/Debug/open_windows_note.exe

- [ ] 首次启动：出现一条“welcome”悬浮便签，置顶、无标准边框
- [ ] 数据库文件生成在 exe 目录（便携）或 %APPDATA%\open_windows_note（只读目录回落）
- [ ] 拖动标题栏空白处：窗口跟随移动
- [ ] 拖动四边/四角：窗口缩放；缩到最小锁死（≥120x80）
- [ ] 关闭按钮(×)：便签隐藏；重启后不再出现（visible=0）
- [ ] pin 按钮：切换置顶/非置顶（用另一窗口盖上验证）
- [ ] 卷起按钮(─)：折叠为标题栏；再点展开恢复原高
- [ ] 透明按钮(○)：在 100%/80%/60%/40% 间循环
- [ ] Ctrl+Alt+N：新建一条便签
- [ ] 移动/缩放/pin/卷起/透明后关闭程序再启动：状态全部保留
- [ ] 再次启动 exe（第二实例）：不新开进程，转发为“新建一条”，随即退出
- [ ] Ctrl+Alt+Q：程序退出
- [ ] 多显示器：把便签拖到副屏，重启后仍在可见区；拔掉副屏后重启，便签回到主屏可见区（clampRectToWorkArea）
