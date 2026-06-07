import serial
import speech_recognition as sr
import sounddevice as sd
import numpy as np
import time
import sys
import io
import wave

# ==================== 配置 ====================
SERIAL_PORT = 'COM3'      # 改成你的开发板串口号
BAUDRATE = 9600

# 命令词 → 发送字节（必须与 STM32 MB_CommandId_t 一致）
CMD_MAP = {
    "前进": 0x20,
    "后退": 0x21,
    "左转": 0x22,
    "右转": 0x23,
    "坐下": 0x24,
    "挥手": 0x25,
    "休眠": 0x11,
    "唤醒": 0x10,
    # 新增四个动作
    "趴下": 0x26,
    "转圈": 0x27,
    "慢走": 0x28,
    "握手": 0x29,
}

def record_audio(duration=3, sample_rate=16000):
    """使用 sounddevice 录音，返回 WAV 字节数据"""
    print("正在听... 说出命令（前进/后退/左转/右转/坐下/挥手/休眠/唤醒/趴下/转圈/慢走/握手）")
    recording = sd.rec(int(duration * sample_rate), samplerate=sample_rate, channels=1, dtype='int16')
    sd.wait()
    byte_io = io.BytesIO()
    with wave.open(byte_io, 'wb') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)  # 16-bit
        wf.setframerate(sample_rate)
        wf.writeframes(recording.tobytes())
    return byte_io.getvalue()

def recognize_speech_google(recognizer, audio_data):
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
        print(f"Google 请求错误: {e}")
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
    print("语音控制就绪（Google 识别）！按 Ctrl+C 退出")
    try:
        while True:
            audio_data = record_audio(duration=3)
            text = recognize_speech_google(recognizer, audio_data)
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
