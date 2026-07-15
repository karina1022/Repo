import queue
import threading
import time
import tkinter as tk
from tkinter import messagebox

import serial


# =========================
# 使用者設定區
# =========================
PORT = "COM4"          # 改成接收端 USB-UART 的 COM Port
BAUD_RATE = 115200     # 必須和 STM32 USART3 相同


# =========================
# 全域變數
# =========================
event_queue = queue.Queue()
stop_event = threading.Event()

serial_port = None
last_status = None


# STM32 傳來的文字與畫面顯示對應
STATUS_TABLE = {
    "RECEIVER READY": {
        "title": "接收端已啟動",
        "detail": "等待紅外線事件",
        "background": "#455A64",
    },
    "SAFE": {
        "title": "SAFE",
        "detail": "目前狀態安全",
        "background": "#2E7D32",
    },
    "LEVEL1": {
        "title": "LEVEL 1",
        "detail": "注意前方狀況",
        "background": "#EF6C00",
    },
    "LEVEL2": {
        "title": "LEVEL 2",
        "detail": "警告：請提高注意",
        "background": "#C62828",
    },
    "LEVEL3": {
        "title": "LEVEL 3",
        "detail": "緊急危險：立即反應",
        "background": "#8E0000",
    },
    "ERROR": {
        "title": "SYSTEM ERROR",
        "detail": "系統發生錯誤",
        "background": "#6A1B9A",
    },
}


def serial_worker() -> None:
    """背景執行緒：持續讀取 STM32 USART3 傳來的資料。"""
    global serial_port

    try:
        serial_port = serial.Serial(
            port=PORT,
            baudrate=BAUD_RATE,
            timeout=0.2,
            write_timeout=1,
        )

        # 清除開啟串口前殘留的舊資料
        serial_port.reset_input_buffer()
        serial_port.reset_output_buffer()

        event_queue.put(
            ("CONNECTED", f"已連線到 {PORT}")
        )

        while not stop_event.is_set():
            raw_data = serial_port.readline()

            if not raw_data:
                continue

            # 在執行Python的命令列顯示實際收到的原始資料
            print("RAW:", repr(raw_data))

            message = raw_data.decode(
                "utf-8",
                errors="ignore",
            ).strip()

            if message:
                event_queue.put(
                    ("MESSAGE", message)
                )

    except serial.SerialException as error:
        event_queue.put(
            (
                "SERIAL_ERROR",
                f"無法開啟 {PORT}\n\n{error}",
            )
        )

    except Exception as error:
        event_queue.put(
            (
                "SERIAL_ERROR",
                f"程式發生錯誤：\n\n{error}",
            )
        )

    finally:
        if (
            serial_port is not None
            and serial_port.is_open
        ):
            serial_port.close()

        serial_port = None

def add_log(message: str) -> None:
    """在下方紀錄區加入訊息。"""
    current_time = time.strftime("%H:%M:%S")

    log_text.config(state="normal")
    log_text.insert(
        tk.END,
        f"[{current_time}] {message}\n",
    )
    log_text.see(tk.END)
    log_text.config(state="disabled")


def update_status(message: str) -> None:
    """依照 STM32 傳來的文字更新畫面。"""
    global last_status

    normalized = message.strip().upper()

    # 相同狀態不重複更新紀錄
    if normalized == last_status:
        return

    last_status = normalized

    if normalized in STATUS_TABLE:
        status = STATUS_TABLE[normalized]

        status_frame.config(
            background=status["background"],
        )

        status_title.config(
            text=status["title"],
            background=status["background"],
        )

        status_detail.config(
            text=status["detail"],
            background=status["background"],
        )

        add_log(f"收到：{normalized}")

    else:
        add_log(f"收到未知資料：{message}")


def process_events() -> None:
    """由 Tkinter 主執行緒處理背景執行緒送來的資料。"""
    try:
        while True:
            event_type, value = event_queue.get_nowait()

            if event_type == "CONNECTED":
                connection_label.config(
                    text=f"● {value}",
                    foreground="green",
                )
                add_log(value)

            elif event_type == "MESSAGE":
                update_status(value)

            elif event_type == "SERIAL_ERROR":
                connection_label.config(
                    text="● 串口連線失敗",
                    foreground="red",
                )

                add_log(value)
                messagebox.showerror(
                    "串口錯誤",
                    value,
                )

    except queue.Empty:
        pass

    root.after(100, process_events)


def close_program() -> None:
    """關閉視窗時停止串口執行緒。"""
    stop_event.set()

    if serial_port is not None and serial_port.is_open:
        serial_port.close()

    root.destroy()


# =========================
# 建立監控視窗
# =========================
root = tk.Tk()
root.title("紅外線接收端監控")
root.geometry("650x500")
root.minsize(550, 420)

title_label = tk.Label(
    root,
    text="後車端事件監控系統",
    font=("Microsoft JhengHei", 22, "bold"),
)
title_label.pack(pady=(20, 5))

connection_label = tk.Label(
    root,
    text=f"● 正在連線 {PORT}...",
    foreground="orange",
    font=("Microsoft JhengHei", 11),
)
connection_label.pack(pady=(0, 15))

status_frame = tk.Frame(
    root,
    background="#455A64",
    width=560,
    height=190,
)
status_frame.pack(
    padx=30,
    pady=10,
    fill="x",
)
status_frame.pack_propagate(False)

status_title = tk.Label(
    status_frame,
    text="等待連線",
    foreground="white",
    background="#455A64",
    font=("Microsoft JhengHei", 32, "bold"),
)
status_title.pack(pady=(38, 10))

status_detail = tk.Label(
    status_frame,
    text="等待接收端 STM32 傳送資料",
    foreground="white",
    background="#455A64",
    font=("Microsoft JhengHei", 15),
)
status_detail.pack()

log_label = tk.Label(
    root,
    text="接收紀錄",
    font=("Microsoft JhengHei", 12, "bold"),
)
log_label.pack(
    anchor="w",
    padx=30,
    pady=(15, 5),
)

log_text = tk.Text(
    root,
    height=8,
    font=("Consolas", 10),
    state="disabled",
)
log_text.pack(
    padx=30,
    pady=(0, 20),
    fill="both",
    expand=True,
)

root.protocol(
    "WM_DELETE_WINDOW",
    close_program,
)

# 啟動串口背景執行緒
serial_thread = threading.Thread(
    target=serial_worker,
    daemon=True,
)
serial_thread.start()

root.after(100, process_events)
root.mainloop()