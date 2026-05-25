# C同学显示模块测试步骤（5分钟）

1. 编译下载
- 打开 manbot.uvprojx，选择 Target 1。
- 执行 Rebuild + Download。

2. 启动存活检查
- 观察 PB0 心跳灯是否约 500ms 翻转。

3. OLED 上电检查
- 上电后观察 OLED 是否立即显示启动占位图。

4. 打开联调宏并复测
- 修改 App/Src/mb_tasks.c：
	- MB_DEBUG_LOG_ENABLE = 1
	- MB_TEST_INJECT_ENABLE = 1
- 重新编译下载。
- 观察串口是否有 HB/VOICE/MAIN/DISPLAY/TTS 日志，屏幕是否随事件变化。

5. 型号兼容检查
- 默认先用 MB_OLED_MODEL_SH1106 = 0（SSD1306 路径）观察显示。
- 若屏幕为 SH1106，再改 MB_OLED_MODEL_SH1106 = 1 重新下载。
- 对比两种配置下是否存在明显左右偏移或截断。
