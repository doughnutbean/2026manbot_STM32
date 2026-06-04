import serial
import speech_recognition as sr
import sounddevice as sd
import numpy as np
import time
import sys
import io
import wave

# ==================== 配置 ====================
SERIAL_PORT = 'COM3'      # 改成你的开发板 USB 串口号（在设备管理器中查看）
BAUDRATE = 9600

# 命令词 → 发送字节（必须与 STM32 中的 MB_CommandId_t 一致）
CMD_MAP = {
    "前进": 0x01,
    "后退": 0x02,
    "左转": 0x03,
    "右转": 0x04,
    "坐下": 0x05,
    "挥手": 0x06,
    "休眠": 0x07,
    "唤醒": 0x08,
}

def record_audio(duration=3, sample_rate=16000):
    """用 sounddevice 录制音频，返回 WAV 格式字节数据"""
    print("正在听... 说出命令（前进/后退/左转/右转/坐下/挥手/休眠/唤醒）")
    recording = sd.rec(int(duration * sample_rate), samplerate=sample_rate, channels=1, dtype='int16')
    sd.wait()  # 等待录音结束
    # 将 numpy 数组转换为 WAV 字节流
    byte_io = io.BytesIO()
    with wave.open(byte_io, 'wb') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)  # 16-bit
        wf.setframerate(sample_rate)
        wf.writeframes(recording.tobytes())
    return byte_io.getvalue()

def recognize_speech(recognizer, audio_data):
    """使用 Google 在线识别"""
    try:
        audio = sr.AudioData(audio_data, sample_rate=16000, sample_width=2)
        text = recognizer.recognize_google(audio, language='zh-CN')
        print(f"识别结果：{text}")
        return text
    except sr.UnknownValueError:
        print("无法识别语音")
        return ""
    except sr.RequestError as e:
        print(f"网络请求错误: {e}")
        return ""

def main():
    # 初始化串口
    try:
        ser = serial.Serial(SERIAL_PORT, BAUDRATE, timeout=0.1)
        print(f"串口 {SERIAL_PORT} 打开成功")
        time.sleep(2)  # 等待开发板稳定
    except Exception as e:
        print(f"串口打开失败: {e}")
        sys.exit(1)

    recognizer = sr.Recognizer()
    print("语音控制就绪！按 Ctrl+C 退出")
    try:
        while True:
            audio_data = record_audio(duration=3)      # 录制 3 秒
            text = recognize_speech(recognizer, audio_data)
            if not text:
                continue

            # 匹配命令词
            matched_cmd = None
            for cmd_word, code in CMD_MAP.items():
                if cmd_word in text:
                    matched_cmd = code
                    break

            if matched_cmd is not None:
                ser.write(bytes([matched_cmd]))
                print(f"已发送命令字节: 0x{matched_cmd:02X}")
            else:
                print("未匹配任何命令词，请重新说")

    except KeyboardInterrupt:
        print("\n程序退出")
    finally:
        ser.close()

if __name__ == "__main__":
    main()
