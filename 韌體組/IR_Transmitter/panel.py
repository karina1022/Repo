import tkinter as tk
import threading
import queue

event_queue = queue.Queue()

# 事件資料表
EVENTS = {
    "B1": {
        "event": "前方急煞",
        "subtitle": "前前車緊急煞車，請留意",
        "level": "注意",
        "color": "#FFD700",   # 黃色
        "icon": "⚠️",
        "blink": 0
    },
    "B2": {
        "event": "前方急煞",
        "subtitle": "前前車緊急煞車，建議減速",
        "level": "警示",
        "color": "#FF8C00",   # 橘色
        "icon": "⚠️",
        "blink": 800
    },
    "B3": {
        "event": "前方急煞",
        "subtitle": "前前車緊急煞車，立即減速",
        "level": "危險",
        "color": "#FF0000",   # 紅色
        "icon": "🚨",
        "blink": 300
    },
    "P1": {
        "event": "弱勢用路人穿越",
        "subtitle": "前方有弱勢用路人，請留意",
        "level": "注意",
        "color": "#FFD700",
        "icon": "🚶‍♂️⚠️",
        "blink": 0
    },
    "P2": {
        "event": "弱勢用路人穿越",
        "subtitle": "弱勢用路人接近車道，建議減速",
        "level": "警示",
        "color": "#FF8C00",
        "icon": "🚶‍♂️⚠️",
        "blink": 800
    },
    "P3": {
        "event": "弱勢用路人穿越",
        "subtitle": "弱勢用路人近距穿越，立即注意",
        "level": "危險",
        "color": "#FF0000",
        "icon": "🚨🚶‍♂️",
        "blink": 300
    }
}

current_blink_job = None
blink_visible = True


def stop_blink():
    global current_blink_job
    if current_blink_job is not None:
        root.after_cancel(current_blink_job)
        current_blink_job = None


def blink_panel(interval, color):
    global blink_visible, current_blink_job

    blink_visible = not blink_visible

    if blink_visible:
        frame.config(highlightbackground=color)
        label_level.config(fg=color)
        label_code.config(fg=color)
        label_icon.config(fg=color)
    else:
        frame.config(highlightbackground="black")
        label_level.config(fg="black")
        label_code.config(fg="black")
        label_icon.config(fg="black")

    current_blink_job = root.after(interval, blink_panel, interval, color)


def show_event(code):
    global blink_visible

    code = code.strip().upper()
    stop_blink()
    blink_visible = True

    if code in EVENTS:
        data = EVENTS[code]

        label_code.config(text=f"EVENT CODE：{code}", fg=data["color"])
        label_level.config(text=f"風險等級：{data['level']}", fg=data["color"])
        label_icon.config(text=data["icon"], fg=data["color"])
        label_title.config(text=data["event"])
        label_msg.config(text=data["subtitle"])

        frame.config(highlightbackground=data["color"], highlightthickness=8)

        # 依照風險等級控制閃爍
        if data["blink"] > 0:
            current_interval = data["blink"]
            blink_panel(current_interval, data["color"])

    else:
        label_code.config(text="EVENT CODE：UNKNOWN", fg="white")
        label_level.config(text="風險等級：未知", fg="white")
        label_icon.config(text="--", fg="white")
        label_title.config(text="未知事件編碼")
        label_msg.config(text=f"收到：{code}，請輸入 B1/B2/B3 或 P1/P2/P3")
        frame.config(highlightbackground="white", highlightthickness=4)


def terminal_input_thread():
    print("請在終端機輸入事件編碼：")
    print("B1 = 前方急煞｜注意")
    print("B2 = 前方急煞｜警示")
    print("B3 = 前方急煞｜危險")
    print("P1 = 弱勢用路人穿越｜注意")
    print("P2 = 弱勢用路人穿越｜警示")
    print("P3 = 弱勢用路人穿越｜危險")
    print("-" * 40)

    while True:
        code = input("請輸入事件編碼：")
        event_queue.put(code)


def check_queue():
    while not event_queue.empty():
        code = event_queue.get()
        show_event(code)

    root.after(100, check_queue)


# 主視窗
root = tk.Tk()
root.title("公車後方警示面板模擬")
root.geometry("900x560")
root.configure(bg="black")

frame = tk.Frame(
    root,
    bg="black",
    highlightbackground="white",
    highlightthickness=4
)
frame.pack(expand=True, fill="both", padx=25, pady=25)

label_code = tk.Label(
    frame,
    text="EVENT CODE：WAITING",
    font=("Arial", 24, "bold"),
    fg="white",
    bg="black"
)
label_code.pack(pady=(20, 5))

label_level = tk.Label(
    frame,
    text="風險等級：等待",
    font=("Microsoft JhengHei", 26, "bold"),
    fg="white",
    bg="black"
)
label_level.pack(pady=5)

label_icon = tk.Label(
    frame,
    text="--",
    font=("Arial", 90),
    fg="white",
    bg="black"
)
label_icon.pack(pady=15)

label_title = tk.Label(
    frame,
    text="等待事件編碼",
    font=("Microsoft JhengHei", 44, "bold"),
    fg="white",
    bg="black"
)
label_title.pack(pady=10)

label_msg = tk.Label(
    frame,
    text="請在終端機輸入 B1/B2/B3 或 P1/P2/P3",
    font=("Microsoft JhengHei", 24),
    fg="white",
    bg="black"
)
label_msg.pack(pady=15)

thread = threading.Thread(target=terminal_input_thread, daemon=True)
thread.start()

root.after(100, check_queue)
root.mainloop()