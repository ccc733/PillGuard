"""
ESP-01 + CP2102 自动测试脚本
==============================
自动发送 AT 指令 → 连接 WiFi → SSL → HTTP GET → Server酱

使用方法:
  1. CP2102 接好 ESP-01, 插上电脑
  2. 修改下方 COM_PORT 为你的 CP2102 端口号
  3. 在终端运行: python test_esp01.py
"""

import serial
import time

# ============================================================
# 配置 — 改这里
# ============================================================
COM_PORT = "COM19"           # CP2102 的 COM 口号, 在设备管理器里查看
BAUDRATE = 115200
WIFI_SSID = "QQNeNe"
WIFI_PASS = "Cc20060618"
SENDKEY   = "SCT359372TJL0R2eT8Nvqk2naBmWV0zyde"

# ============================================================
# 打开串口
# ============================================================
ser = serial.Serial(COM_PORT, BAUDRATE, timeout=1)
time.sleep(1)
ser.flushInput()
print(f"[*] 已连接 {COM_PORT} @ {BAUDRATE}")

# ============================================================
# 发送 AT 并等待响应
# ============================================================
def at_send(cmd, wait_for=b"OK", timeout=15):
    """发送 AT 指令, 等待期望的响应"""
    full_cmd = cmd + "\r\n"
    print(f"\n>>> {cmd}")
    ser.write(full_cmd.encode())

    # 如果是 CIPSEND, 特殊处理: 等 > 符号
    if cmd.startswith("AT+CIPSEND"):
        time.sleep(0.5)
        resp = ser.read(ser.in_waiting)
        print(f"<<< {resp.decode('utf-8', errors='replace')}")
        return b">" in resp

    # 普通指令: 等待期望响应
    start = time.time()
    buf = b""
    while time.time() - start < timeout:
        if ser.in_waiting:
            chunk = ser.read(ser.in_waiting)
            buf += chunk
            # 实时打印
            decoded = chunk.decode('utf-8', errors='replace')
            if decoded.strip():
                print(f"<<< {decoded}", end="")
        if wait_for and wait_for in buf:
            return True
        time.sleep(0.1)

    if buf:
        print(f"\n[!] 超时, 收到: {buf.decode('utf-8', errors='replace')[:200]}")
    return False

def at_send_raw(data):
    """发送原始数据 (用于 HTTP 请求体)"""
    print(f"\n>>> [HTTP请求 {len(data)} 字节]")
    ser.write(data)
    time.sleep(3)
    resp = ser.read(ser.in_waiting)
    decoded = resp.decode('utf-8', errors='replace')
    if decoded.strip():
        print(f"<<< {decoded}")

# ============================================================
# 测试流程
# ============================================================
print("\n" + "=" * 50)
print("步骤 1/6: 测试 AT 通信")
print("=" * 50)
if not at_send("AT", timeout=3):
    print("[FAIL] ESP-01 无响应! 检查接线和波特率")
    ser.close()
    exit(1)
print("[OK] AT 通信正常")

print("\n" + "=" * 50)
print("步骤 2/6: 查看固件版本")
print("=" * 50)
at_send("AT+GMR", timeout=3)

print("\n" + "=" * 50)
print("步骤 3/6: 连接 WiFi")
print("=" * 50)
at_send("AT+CWMODE=1")
time.sleep(1)
cmd = f'AT+CWJAP="{WIFI_SSID}","{WIFI_PASS}"'
if not at_send(cmd, wait_for=b"WIFI GOT IP", timeout=20):
    # 可能已经连上了
    if not at_send(cmd, wait_for=b"OK", timeout=5):
        print("[FAIL] WiFi 连接失败!")
        ser.close()
        exit(1)
print("[OK] WiFi 已连接")

print("\n" + "=" * 50)
print("步骤 4/6: 建立 SSL 连接")
print("=" * 50)
at_send("AT+CIPMUX=0")
time.sleep(1)
if not at_send('AT+CIPSTART="SSL","sctapi.ftqq.com",443', wait_for=b"CONNECT", timeout=20):
    print("[FAIL] SSL 连接失败!")
    ser.close()
    exit(1)
print("[OK] SSL 连接已建立")

print("\n" + "=" * 50)
print("步骤 5/6: 发送 HTTP GET 请求")
print("=" * 50)

# 构建 HTTP 请求 (精确计算)
http_request = (
    f"GET /{SENDKEY}.send?title=摔倒警报&desp=提醒：检测到老人摔倒请注意！ HTTP/1.1\r\n"
    "Host: sctapi.ftqq.com\r\n"
    "Connection: close\r\n"
    "\r\n"
)
req_bytes = http_request.encode()
req_len  = len(req_bytes)

print(f"[*] 请求长度: {req_len} 字节")

cmd = f"AT+CIPSEND={req_len}"
if not at_send(cmd, timeout=5):
    print("[FAIL] CIPSEND 失败!")
    ser.close()
    exit(1)

time.sleep(0.5)
at_send_raw(req_bytes)

print("\n" + "=" * 50)
print("步骤 6/6: 关闭连接")
print("=" * 50)
at_send("AT+CIPCLOSE", timeout=5)

print("\n" + "=" * 50)
print("测试完成! 请检查微信是否收到 Server酱 推送")
print("=" * 50)

ser.close()
