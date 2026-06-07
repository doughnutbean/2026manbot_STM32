import time
import threading
from datetime import datetime

import serial


# =========================
# 串口配置
# =========================
PORT = "COM3"          # 改成你现在能正常控制 STM32 的串口号
BAUDRATE = 115200


# =========================
# STM32 命令表
# =========================
COMMANDS = {
    "wakeup": 0x10,
    "sleep": 0x11,
    "forward": 0x20,
    "backward": 0x21,
    "left": 0x22,
    "right": 0x23,
    "sit": 0x24,
    "wave": 0x25,
    "happy": 0x30,
    "sad": 0x31,
    "sleepy": 0x32,
    "confused": 0x33,
}


# =========================
# 中文关键词 → 标准命令
# =========================
KEYWORDS = [
    ("wakeup", ["唤醒", "醒来", "开始", "开始工作"]),
    ("sleep", ["休眠", "睡觉", "停下", "停止", "休息"]),
    ("forward", ["前进", "往前", "向前", "朝前"]),
    ("backward", ["后退", "往后", "向后", "退后"]),
    ("left", ["左转", "向左", "往左"]),
    ("right", ["右转", "向右", "往右"]),
    ("sit", ["坐下"]),
    ("wave", ["挥手", "打招呼", "你好"]),
    ("happy", ["开心", "高兴"]),
    ("sad", ["难过", "伤心"]),
    ("sleepy", ["困", "困倦", "想睡觉"]),
    ("confused", ["疑惑", "不懂", "不理解"]),
]


def parse_text_to_commands(text):
    """
    把中文自然语言解析成命令列表。
    例如：
    “机器人先前进，然后左转，再挥手”
    → ["forward", "left", "wave"]
    """
    results = []

    for cmd, words in KEYWORDS:
        for word in words:
            pos = text.find(word)
            if pos != -1:
                results.append((pos, cmd))
                break

    results.sort(key=lambda x: x[0])

    commands = []
    for _, cmd in results:
        commands.append(cmd)

    return commands


def send_command_to_stm32(ser, cmd_name):
    if cmd_name not in COMMANDS:
        print("未知命令：", cmd_name)
        return

    code = COMMANDS[cmd_name]
    ser.write(bytes([code]))

    now = datetime.now().strftime("%H:%M:%S")
    print(f"[{now}] [PC] send {cmd_name}, code = 0x{code:02X}")

    time.sleep(0.5)


def read_stm32_log_thread(ser):
    while True:
        try:
            data = ser.readline()
            if data:
                text = data.decode("utf-8", errors="ignore").strip()
                if text:
                    now = datetime.now().strftime("%H:%M:%S")
                    print(f"[{now}] [STM32] {text}")
        except Exception:
            break


def main():
    print("===================================")
    print("Manbot 简易语音控制版")
    print("===================================")
    print("使用方法：")
    print("1. 光标停在输入框后，按 Win + H")
    print("2. 对电脑说：机器人先前进，然后左转，再挥手")
    print("3. 语音变成文字后，按回车执行")
    print("4. 输入 exit 退出")
    print("===================================")

    try:
        ser = serial.Serial(
            port=PORT,
            baudrate=BAUDRATE,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.2
        )
    except Exception as e:
        print("串口打开失败：", e)
        return

    print(f"串口打开成功：{PORT}, {BAUDRATE}")

    reader = threading.Thread(
        target=read_stm32_log_thread,
        args=(ser,),
        daemon=True
    )
    reader.start()

    while True:
        text = input("\n请说话或输入指令：").strip()

        if text.lower() == "exit":
            break

        if not text:
            continue

        print("识别/输入文本：", text)

        commands = parse_text_to_commands(text)

        if not commands:
            print("没有解析出有效命令。")
            continue

        print("解析出的命令：", commands)

        for cmd in commands:
            send_command_to_stm32(ser, cmd)

    ser.close()
    print("程序结束，串口已关闭。")


if __name__ == "__main__":
    main()