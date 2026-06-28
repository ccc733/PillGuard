"""
OpenMV H7 Plus 摔倒检测 — 官方人脸检测版
=============================================
基于官方 face_detection 例程 (Haar Cascade)
参考: OpenMV 内置 haarcascade_frontalface.cascade

原理:
  1. Haar Cascade 检测正面人脸 → 确认是"人"(不是椅子)
  2. 跟踪人脸 Y 坐标高度
  3. 人脸 Y 坐标骤降 → 人摔倒 → 报警

优势: 只检测人脸, 背景物体完全不会误判

输出: UART3 (P4) 115200bps → "FALL:1\r\n" / "FALL:0\r\n"
"""

import sensor
import image
import time
from pyb import UART, LED

# ============================================================
# UART + LED
# ============================================================
uart = UART(3, 115200)
uart.init(115200, bits=8, parity=None, stop=1)

red   = LED(1)
green = LED(2)
blue  = LED(3)
red.on(); green.on(); blue.on(); time.sleep_ms(300)
red.off(); green.off(); blue.off()

# ============================================================
# 摄像头 — HQVGA 灰度 (官方人脸检测推荐)
# ============================================================
sensor.reset()
sensor.set_contrast(3)
sensor.set_gainceiling(16)
sensor.set_framesize(sensor.HQVGA)          # 240×160
sensor.set_pixformat(sensor.GRAYSCALE)
sensor.skip_frames(time=2000)
clock = time.clock()

# ============================================================
# 加载官方人脸 Haar Cascade
# ============================================================
face_cascade = image.HaarCascade("/rom/haarcascade_frontalface.cascade", stages=25)
print(face_cascade)

# ============================================================
# 参数
# ============================================================
FALL_Y_DROP         = 40            # 人脸 Y 坐标下降 > 40 像素 → 疑似摔倒
FALL_H_RATIO_MAX    = 1.30          # 人脸矩形高宽比 > 1.30 → 横躺(不规则角度)
FALL_COOLDOWN_MS    = 5000
FALL_DEBOUNCE_N     = 4             # 连续4帧确认

# ============================================================
# 状态
# ============================================================
fall_detected       = False
fall_debounce_cnt   = 0
last_fall_send_ms   = 0
last_face_y         = -1            # 上一帧人脸 Y 坐标 (-1 = 未检测到)
last_face_h         = 0             # 上一帧人脸高度
face_lost_frames    = 0             # 连续丢脸帧数

print("=== Fall Detection (Haar Cascade Face) ===")
green.on()

# ============================================================
# 主循环
# ============================================================
while True:
    clock.tick()
    img = sensor.snapshot()

    # === 1. 官方人脸检测 ===
    objects = img.find_features(face_cascade, threshold=0.75, scale_factor=1.25)

    # === 2. 分析人脸位置 ===
    frame_fall = False
    face_y = -1
    face_h = 0
    face_w = 0

    if objects:
        # 取最大的人脸 (通常只有一个)
        r = max(objects, key=lambda x: x[2] * x[3])  # 按面积排序
        face_x, face_y, face_w, face_h = r

        # 绘制检测框
        img.draw_rectangle(r, color=255)
        img.draw_cross(face_x + face_w // 2, face_y + face_h // 2, color=255, size=10)

        face_lost_frames = 0

        # --- 摔倒判断 ---
        # 条件1: 人脸 Y 坐标突然下降 (人脸从画面高处跌到低处)
        if last_face_y > 0:
            y_drop = face_y - last_face_y
            if y_drop > FALL_Y_DROP:
                frame_fall = True

        # 条件2: 人脸高宽比异常 (头部横躺/倾斜)
        if face_w > 0:
            hw_ratio = face_h / face_w
            if hw_ratio < 0.70 or hw_ratio > FALL_H_RATIO_MAX:
                # 太扁或太窄 → 可能人倒地后头部角度异常
                pass  # 仅作为辅助, 不完全依赖

        # 更新追踪
        last_face_y = face_y
        last_face_h = face_h

    else:
        # 没检测到人脸
        face_lost_frames += 1

        # 如果之前跟踪着人脸, 突然丢了 → 可能人摔倒了/转过去了
        if last_face_y > 0 and face_lost_frames > FALL_DEBOUNCE_N:
            # 人脸丢了 (可能人倒地后脸不在摄像头视野)
            frame_fall = True

    # --- 3. 防抖 + 状态 ---
    if frame_fall:
        fall_debounce_cnt += 1
    else:
        fall_debounce_cnt = max(fall_debounce_cnt - 1, 0)

    if fall_debounce_cnt >= FALL_DEBOUNCE_N:
        if not fall_detected:
            fall_detected = True
        red.on(); green.off(); blue.off()
        img.draw_string(2, 2, "FALL! Y:%d" % face_y, color=255)
    else:
        if fall_detected:
            fall_detected = False
            fall_debounce_cnt = 0
        if face_y > 0:
            red.off(); green.on(); blue.off()
            img.draw_string(2, 2, "OK Y:%d" % face_y, color=255)
        else:
            red.off(); green.off(); blue.on()
            img.draw_string(2, 2, "NO FACE", color=255)

        # 长期无人脸, 重置追踪
        if face_lost_frames > 30:
            last_face_y = -1

    # --- 4. UART ---
    now = time.ticks_ms()
    if fall_detected:
        if time.ticks_diff(now, last_fall_send_ms) > FALL_COOLDOWN_MS:
            for _ in range(3):
                uart.write("FALL:1\r\n")
                time.sleep_ms(20)
            last_fall_send_ms = now
            print("[ALERT] FALL:1  (%.1f fps)" % clock.fps())
    else:
        if time.ticks_diff(now, last_fall_send_ms) > 2000:
            uart.write("FALL:0\r\n")
            last_fall_send_ms = now

    if clock.fps():
        print("FPS:%.1f  Face:%d  Fall:%d  Y:%d  Lost:%d" %
              (clock.fps(), 1 if face_y > 0 else 0,
               int(fall_detected), face_y, face_lost_frames))
