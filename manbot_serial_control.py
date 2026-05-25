import serial
import threading
from datetime import datetime

# 改成你电脑设备管理器里显示的实际串口号
PORT = "COM11"

BAUDRATE = 115200
LOG_FILE = "manbot_control_log.txt"

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
    "bad": 0x99,   # 用于测试非法命令
}


def read_serial(ser):
    with open(LOG_FILE, "a", encoding="utf-8") as f:
        f.write("\n========== Manbot Control Test Start ==========\n")
        f.write("Start time: " + datetime.now().strftime("%Y-%m-%d %H:%M:%S") + "\n")
        f.write(f"Port: {PORT}, Baudrate: {BAUDRATE}\n")
        f.write("-----------------------------------------------\n")

        while True:
            try:
                data = ser.readline()

                if data:
                    text = data.decode("utf-8", errors="ignore").strip()

                    if text:
                        now = datetime.now().strftime("%H:%M:%S")
                        line = f"[{now}] {text}"
                        print(line)
                        f.write(line + "\n")
                        f.flush()

            except Exception as e:
                print("串口读取异常：", e)
                break


def send_command(ser, cmd_name):
    cmd_name = cmd_name.strip().lower()

    if cmd_name not in COMMANDS:
        print("未知命令。可用命令：")
        print(", ".join(COMMANDS.keys()))
        return

    cmd_code = COMMANDS[cmd_name]
    ser.write(bytes([cmd_code]))

    now = datetime.now().strftime("%H:%M:%S")
    print(f"[{now}] [PC] send {cmd_name}, code = 0x{cmd_code:02X}")


def main():
    print("===================================")
    print("Manbot 串口控制与日志监控工具")
    print("===================================")
    print(f"串口号：{PORT}")
    print(f"波特率：{BAUDRATE}")
    print(f"日志文件：{LOG_FILE}")
    print("输入命令后按回车发送")
    print("输入 exit 退出")
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
        print("请检查 COM 口是否正确，或者串口是否被其他软件占用。")
        return

    print("串口打开成功。")

    reader = threading.Thread(target=read_serial, args=(ser,), daemon=True)
    reader.start()

    print("可用命令：")
    print(", ".join(COMMANDS.keys()))
    print("-----------------------------------")

    while True:
        cmd = input(">>> ").strip()

        if cmd.lower() == "exit":
            break

        send_command(ser, cmd)

    ser.close()
    print("串口已关闭。")


if __name__ == "__main__":
    main()