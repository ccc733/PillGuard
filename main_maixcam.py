"""
MaixCAM Pro 摔倒检测 — 基于官方姿态检测 + 官方 Maix 通信协议
=============================================================
基于官方 body_key_points 例程修改, 使用 YOLO11-Pose 检测 17 个关键点

摔倒判断:
  1. 躯干倾斜角 (肩-髋连线 与 垂直线的夹角) > 阈值 → 疑似摔倒
  2. 包围盒宽高比 w/h > 阈值 → 疑似摔倒
  3. 两个条件同时满足 + 连续多帧确认 → 确认摔倒

通信: 严格使用官方 maix.comm.CommProtocol 二进制协议主动上报
  自定义命令: 0x03 = 摔倒警报
  body: status(1B) 0=正常 1=摔倒

硬件连接 (与 STM32 通信):
  MaixCAM Pro TX (A19, UART1_TX) → STM32 PC11 (USART3_RX)
  MaixCAM Pro RX (A18, UART1_RX) → STM32 PC10 (USART3_TX)
  MaixCAM Pro GND → STM32 GND

部署:
  将本文件保存为 TF 卡 /root/main.py 即可开机自启动
"""

from maix import camera, display, image, nn, app, comm, time
import math

# ============================================================
# 1. 摔倒检测参数 — 已调低阈值方便测试
# ============================================================
TORSO_ANGLE_THRESHOLD = 45.0      # 躯干倾斜角阈值（°）
ASPECT_RATIO_THRESHOLD = 0.9      # 包围盒 w/h 比 > 0.9 认为躺倒
FALL_FRAME_CONFIRM = 2            # 连续2帧确认
CONF_THRESHOLD = 0.5              # 检测置信度阈值
KEYPOINT_THRESHOLD = 0.5          # 关键点置信度阈值
FALL_REPORT_COOLDOWN_MS = 2000    # 两次上报冷却时间（ms）

# ============================================================
# 2. 自定义通信协议命令
# ============================================================
APP_CMD_FALL_ALERT  = 0x03   # 摔倒警报上报
APP_CMD_HEARTBEAT   = 0x04   # 心跳包上报（可选）

# ============================================================
# 3. 初始化官方通信协议 (comm.CommProtocol)
# ============================================================
p = comm.CommProtocol(buff_size=1024)
print("[Comm] 通信协议初始化完成, 默认端口 115200bps")

# ============================================================
# 4. 初始化姿态检测模型
# ============================================================
detector = nn.YOLO11(model="/root/models/yolo11n_pose.mud", dual_buff=True)
# 备选: detector = nn.YOLOv8(model="/root/models/yolov8n_pose.mud", dual_buff=True)

cam = camera.Camera(
    detector.input_width(),   # 默认 320
    detector.input_height(),  # 默认 224
    detector.input_format()
)
disp = display.Display()

# ============================================================
# 5. 工具函数
# ============================================================

def get_point(obj, idx):
    """安全获取关键点坐标，返回 (x, y) 或 None"""
    x = obj.points[idx * 2]
    y = obj.points[idx * 2 + 1]
    if x < 0 or y < 0:
        return None
    return (x, y)


def angle_between(v1, v2):
    """计算两个向量的夹角（度）"""
    dot = v1[0] * v2[0] + v1[1] * v2[1]
    mag1 = math.sqrt(v1[0]**2 + v1[1]**2)
    mag2 = math.sqrt(v2[0]**2 + v2[1]**2)
    if mag1 == 0 or mag2 == 0:
        return 0
    cos_a = max(-1.0, min(1.0, dot / (mag1 * mag2)))
    return math.degrees(math.acos(cos_a))


def get_torso_angle(obj):
    """
    计算躯干倾斜角：肩中点→髋中点 的向量 与 垂直向下(0,1) 的夹角
    返回: 角度(°), 0=完全直立, 90=完全躺平

    关键点索引:
      5=左肩  6=右肩  11=左髋  12=右髋
    """
    l_shoulder = get_point(obj, 5)
    r_shoulder = get_point(obj, 6)
    l_hip = get_point(obj, 11)
    r_hip = get_point(obj, 12)

    if l_shoulder and r_shoulder and l_hip and r_hip:
        mid_shoulder = ((l_shoulder[0] + r_shoulder[0]) / 2,
                        (l_shoulder[1] + r_shoulder[1]) / 2)
        mid_hip = ((l_hip[0] + r_hip[0]) / 2,
                   (l_hip[1] + r_hip[1]) / 2)
        v = (mid_hip[0] - mid_shoulder[0], mid_hip[1] - mid_shoulder[1])
        return angle_between(v, (0, 1))

    if l_shoulder and l_hip:
        v = (l_hip[0] - l_shoulder[0], l_hip[1] - l_shoulder[1])
        return angle_between(v, (0, 1))
    if r_shoulder and r_hip:
        v = (r_hip[0] - r_shoulder[0], r_hip[1] - r_shoulder[1])
        return angle_between(v, (0, 1))

    return -1  # 无法计算


def is_fallen(obj):
    """
    综合判断是否摔倒
    返回: (is_fall, torso_angle, aspect)
    """
    torso_angle = get_torso_angle(obj)
    aspect = obj.w / obj.h if obj.h > 0 else 0

    angle_fall = (torso_angle >= 0 and torso_angle > TORSO_ANGLE_THRESHOLD)
    aspect_fall = aspect > ASPECT_RATIO_THRESHOLD

    fall = angle_fall and aspect_fall

    return fall, torso_angle, aspect


# ============================================================
# 6. 主循环
# ============================================================
print("=" * 50)
print("摔倒检测程序启动 (官方通信协议) [阈值已调低]")
print(f"躯干倾斜角阈值: {TORSO_ANGLE_THRESHOLD}°")
print(f"包围盒宽高比阈值: {ASPECT_RATIO_THRESHOLD}")
print(f"连续确认帧数: {FALL_FRAME_CONFIRM}")
print("=" * 50)

fall_counter = 0
stand_counter = 0
current_state = "STAND"
last_state = "STAND"
last_fall_report_ms = 0          # 哨兵值: 0 表示从未上报过
frame_count = 0

while not app.need_exit():
    img = cam.read()
    objs = detector.detect(img, conf_th=CONF_THRESHOLD, iou_th=0.45,
                           keypoint_th=KEYPOINT_THRESHOLD)

    has_person = False
    frame_count += 1

    for obj in objs:
        if obj.class_id != 0:
            continue

        has_person = True

        fall, torso_angle, aspect = is_fallen(obj)

        # 每150帧打印一次诊断 (约5秒@30fps)
        if frame_count % 150 == 0:
            print(f"[DEBUG] angle={torso_angle:.0f}°(>{TORSO_ANGLE_THRESHOLD}?)  "
                  f"w/h={aspect:.2f}(>{ASPECT_RATIO_THRESHOLD}?)  "
                  f"fall={fall_counter}/{FALL_FRAME_CONFIRM}  "
                  f"state={current_state}")

        if fall:
            fall_counter += 1
            stand_counter = 0
            if fall_counter >= FALL_FRAME_CONFIRM:
                current_state = "FALL"
        else:
            stand_counter += 1
            fall_counter = 0
            if stand_counter >= FALL_FRAME_CONFIRM:
                current_state = "STAND"

        now_ms = time.ticks_ms()
        if current_state != last_state:
            print(f"[状态变化] {last_state} -> {current_state}  "
                  f"angle={torso_angle:.0f}° aspect={aspect:.2f}")

            if current_state == "FALL":
                # 直接用减法算时间差 (5000ms=5s内不会溢出32位)
                # 不用 ticks_diff() — MaixPy 实现可能有问题
                elapsed = now_ms - last_fall_report_ms
                if last_fall_report_ms == 0 or elapsed >= FALL_REPORT_COOLDOWN_MS:
                    body = b'\x01'
                    p.report(APP_CMD_FALL_ALERT, body)
                    last_fall_report_ms = now_ms
                    print(f"[上报] ★★★ 摔倒警报! body=0x01 ★★★")
                    print(f"  -> 期望收到: AA CA AC BB 05 00 00 00 xx 03 01 xx xx")
            else:
                body = b'\x00'
                p.report(APP_CMD_FALL_ALERT, body)
                print(f"[上报] 恢复正常 body=0x00")

            last_state = current_state

        # 画面绘制
        color = image.COLOR_RED if current_state == "FALL" else image.COLOR_GREEN
        img.draw_rect(obj.x, obj.y, obj.w, obj.h, color=color, thickness=2)

        detector.draw_pose(
            img, obj.points,
            radius=4 if detector.input_width() > 480 else 3,
            color=color
        )

        status_text = "FALL DOWN!" if current_state == "FALL" else "STAND/SIT"
        img.draw_string(10, 10, f"Status: {status_text}",
                        color=color, scale=1.5)

        if torso_angle >= 0:
            info = f"Angle:{torso_angle:.0f}deg W/H:{aspect:.2f}"
            img.draw_string(10, 30, info, color=image.COLOR_YELLOW, scale=1.0)

        # 画躯干轴线
        l_shoulder = get_point(obj, 5)
        r_shoulder = get_point(obj, 6)
        l_hip = get_point(obj, 11)
        r_hip = get_point(obj, 12)

        if l_shoulder and r_shoulder and l_hip and r_hip:
            mid_s = (int((l_shoulder[0] + r_shoulder[0]) / 2),
                     int((l_shoulder[1] + r_shoulder[1]) / 2))
            mid_h = (int((l_hip[0] + r_hip[0]) / 2),
                     int((l_hip[1] + r_hip[1]) / 2))
            img.draw_line(mid_s[0], mid_s[1], mid_h[0], mid_h[1],
                          color=image.COLOR_BLUE, thickness=2)

    if not has_person:
        fall_counter = 0
        stand_counter = 0
        if current_state == "FALL":
            current_state = "STAND"
            last_state = "STAND"
            body = b'\x00'
            p.report(APP_CMD_FALL_ALERT, body)
            print("[上报] 人消失, 恢复正常")
        img.draw_string(10, 10, "No person detected",
                        color=image.COLOR_WHITE, scale=1.2)

    disp.show(img)

    msg = p.get_msg()
    if msg:
        pass

print("程序退出")
