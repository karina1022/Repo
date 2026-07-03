import serial
import time
import re
from datetime import datetime


# =========================
# 使用者設定區
# =========================
PORT = "COM8"             # 接收端 STM32 UART4 的 COM Port
BAUD_RATE = 115200        # STM32 UART4 的鮑率
SHOW_NO_FRAME = False     # True = 顯示每一筆 RX no frame；False = 避免洗版
NO_FRAME_SUMMARY_SEC = 2  # SHOW_NO_FRAME=False 時，每幾秒統計一次 no frame


# =========================
# Payload 對照表
# =========================
PAYLOAD_TABLE = {
    "00000000": "無事件 / SAFE",
    "00101000": "弱勢用路人穿越 Level 1 注意",
    "00110011": "弱勢用路人穿越 Level 2 警告",
    "00111010": "弱勢用路人穿越 Level 3 緊急",
    "11100111": "系統錯誤",
}


def now_time():
    return datetime.now().strftime("%H:%M:%S")


def payload_to_name(payload_bits):
    return PAYLOAD_TABLE.get(payload_bits, "未定義 / 未來擴充事件")


def print_header():
    print("=" * 78)
    print("IR NEC timing-based 8-bit payload 接收端 UART 顯示程式")
    print(f"COM Port  : {PORT}")
    print(f"Baud Rate : {BAUD_RATE}")
    print("-" * 78)
    print("預期 Payload：")
    for payload, name in PAYLOAD_TABLE.items():
        print(f"  {payload}  ->  {name}")
    print("=" * 78)


def main():
    print_header()

    current = {
        "payload": None,
        "type": None,
        "level": None,
        "event": None,
        "output": None,
    }

    no_frame_count = 0
    last_no_frame_report = time.time()

    try:
        with serial.Serial(PORT, BAUD_RATE, timeout=1) as ser:
            time.sleep(2)

            while True:
                raw = ser.readline().decode("utf-8", errors="ignore").strip()

                if raw == "":
                    continue

                # 1) 新版接收端成功解出 payload
                # 格式：
                # RX OK: payload = 00110011, type = 1, level = 2
                if raw.startswith("RX OK"):
                    match = re.search(
                        r"payload\s*=\s*([01]{8})\s*,\s*type\s*=\s*(\d+)\s*,\s*level\s*=\s*(\d+)",
                        raw
                    )

                    if match:
                        current["payload"] = match.group(1)
                        current["type"] = int(match.group(2))
                        current["level"] = int(match.group(3))
                        current["event"] = payload_to_name(current["payload"])
                        current["output"] = None

                        print(
                            f"[{now_time()}] RX OK | "
                            f"Payload: {current['payload']} | "
                            f"Type: {current['type']} | "
                            f"Level: {current['level']} | "
                            f"{current['event']}"
                        )
                    else:
                        print(f"[{now_time()}] RX OK 但格式無法解析：{raw}")

                # 2) SAFE 候選次數
                # 格式：
                # Event: SAFE candidate 1 / 3
                elif raw.startswith("Event: SAFE candidate"):
                    match = re.search(r"SAFE candidate\s+(\d+)\s*/\s*(\d+)", raw)

                    if match:
                        count = int(match.group(1))
                        target = int(match.group(2))
                        print(f"[{now_time()}] SAFE 候選：{count}/{target}，尚未一定判定安全")
                    else:
                        print(f"[{now_time()}] {raw}")

                # 3) 一般事件文字
                # 例如：
                # Event: VRU CROSS Level 2 警告
                # Event: SYSTEM ERROR
                elif raw.startswith("Event:"):
                    event_text = raw.replace("Event:", "").strip()
                    current["event"] = event_text
                    print(f"[{now_time()}] 事件內容：{event_text}")

                # 4) 最終輸出狀態
                # 例如：
                # Output: SAFE confirmed
                # Output: event updated immediately
                # Output: keep previous state, SAFE not confirmed yet
                elif raw.startswith("Output:"):
                    output_text = raw.replace("Output:", "").strip()
                    current["output"] = output_text

                    if output_text == "SAFE confirmed":
                        print(f"[{now_time()}] ✅ 最終判斷：安全 confirmed")
                    elif output_text == "event updated immediately":
                        print(f"[{now_time()}] ⚠️ 最終判斷：事件立即更新")
                    elif "SAFE not confirmed" in output_text:
                        print(f"[{now_time()}] ⏳ 最終判斷：維持前一狀態，SAFE 尚未連續達標")
                    else:
                        print(f"[{now_time()}] 最終判斷：{output_text}")

                    print("-" * 78)

                # 5) checksum 錯誤
                # 格式：
                # RX checksum error: payload = xxxxxxxx, keep previous state
                elif raw.startswith("RX checksum error"):
                    match = re.search(r"payload\s*=\s*([01]{8})", raw)

                    if match:
                        payload = match.group(1)
                        print(
                            f"[{now_time()}] ❌ Checksum 錯誤 | "
                            f"Payload: {payload} | 維持前一狀態"
                        )
                    else:
                        print(f"[{now_time()}] ❌ Checksum 錯誤：{raw}")

                    print("-" * 78)

                # 6) 收到起始或資料，但時間不合法
                elif raw.startswith("RX invalid frame"):
                    print(f"[{now_time()}] ⚠️ 無效 frame：可能距離、角度、光線或 timing 門檻不穩，維持前一狀態")
                    print("-" * 78)

                # 7) 沒收到完整 frame
                elif raw.startswith("RX no frame"):
                    no_frame_count += 1

                    if SHOW_NO_FRAME:
                        print(f"[{now_time()}] 沒收到完整 frame，維持前一狀態")
                    else:
                        now = time.time()
                        if now - last_no_frame_report >= NO_FRAME_SUMMARY_SEC:
                            print(f"[{now_time()}] no frame 累計 {no_frame_count} 次，維持前一狀態")
                            no_frame_count = 0
                            last_no_frame_report = now

                # 8) STM32 開機訊息或其他資訊
                else:
                    print(f"[{now_time()}] 系統訊息：{raw}")

    except serial.SerialException as error:
        print("無法開啟 COM Port。")
        print(f"錯誤內容：{error}")
        print("請確認：")
        print("1. COM Port 是否正確，例如 COM8")
        print("2. 接收端 STM32 是否已連接電腦")
        print("3. Tera Term、Serial Monitor 或其他 Python 程式是否正在占用 COM Port")

    except KeyboardInterrupt:
        print("\n程式已停止。")


if __name__ == "__main__":
    main()
