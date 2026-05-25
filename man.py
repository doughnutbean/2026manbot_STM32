import serial
from datetime import datetime

# =========================
# 需要你根据实际情况修改的地方
# =========================
PORT = "COM11"          # 改成你电脑实际显示的串口号，例如 COM3、COM5、COM7
BAUDRATE = 115200      # 必须和 STM32 程序里的 USART1 波特率一致
LOG_FILE = "manbot_test_log.txt"


def main():
    print("===================================")
    print("Manbot 串口日志监控工具")
    print("===================================")
    print(f"串口号：{PORT}")
    print(f"波特率：{BAUDRATE}")
    print(f"日志文件：{LOG_FILE}")
    print("按 Ctrl + C 可结束程序")
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
        print("串口打开失败！")
        print("错误信息：", e)
        print("请检查：")
        print("1. COM 口是否写对")
        print("2. 串口是否被其他软件占用")
        print("3. USB-TTL 是否已经插入电脑")
        return

    print("串口打开成功，开始接收 STM32 日志...\n")

    with open(LOG_FILE, "a", encoding="utf-8") as f:
        f.write("\n")
        f.write("========== Manbot Test Start ==========\n")
        f.write("Start time: " + datetime.now().strftime("%Y-%m-%d %H:%M:%S") + "\n")
        f.write(f"Port: {PORT}, Baudrate: {BAUDRATE}\n")
        f.write("---------------------------------------\n")

        try:
            while True:
                data = ser.readline()

                if data:
                    text = data.decode("utf-8", errors="ignore").strip()

                    if text:
                        now = datetime.now().strftime("%H:%M:%S")
                        line = f"[{now}] {text}"

                        print(line)

                        f.write(line + "\n")
                        f.flush()

        except KeyboardInterrupt:
            print("\n检测到 Ctrl + C，程序结束。")

        finally:
            ser.close()
            f.write("---------------------------------------\n")
            f.write("End time: " + datetime.now().strftime("%Y-%m-%d %H:%M:%S") + "\n")
            f.write("========== Manbot Test End ==========\n")
            print("串口已关闭。")
            print(f"日志已保存到：{LOG_FILE}")


if __name__ == "__main__":
    main()