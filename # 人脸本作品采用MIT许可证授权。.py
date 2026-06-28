# 本作品采用MIT许可证授权。
# 版权所有 (c) 2013-2023 OpenMV LLC。保留所有权利。
# https://github.com/openmv/openmv/blob/master/LICENSE
#
# 人脸跟踪示例
#
# 此示例展示了如何使用OpenMV Cam的关键点功能来跟踪
# 通过Haar Cascade检测到的人脸。脚本的第一部分使用
# frontalface Haar Cascade在图像中找到人脸。
# 之后，脚本使用关键点功能自动学习并跟踪
# 你的人脸。关键点可用于自动跟踪任何物体。
import sensor
import time
import image

# 重置传感器
sensor.reset()
sensor.set_contrast(3)
sensor.set_gainceiling(16)
sensor.set_framesize(sensor.VGA)
sensor.set_windowing((320, 240))
sensor.set_pixformat(sensor.GRAYSCALE)

# 跳过几帧以使传感器稳定下来
sensor.skip_frames(time=2000)

# 加载 Haar 级联
# 默认情况下，这将使用所有阶段，较低的阶段速度更快但准确性较低。
face_cascade = image.HaarCascade("/rom/haarcascade_frontalface.cascade", stages=25)
print(face_cascade)

# 第一组关键点
kpts1 = None

# 找一张脸！
while kpts1 is None:
    img = sensor.snapshot()
    img.draw_string(0, 0, "Looking for a face...")
    # 寻找面部
    objects = img.find_features(face_cascade, threshold=0.5, scale=1.25)
    if objects:
        # 将ROI区域向四周各扩展31像素
        face = (
            objects[0][0] - 31,
            objects[0][1] - 31,
            objects[0][2] + 31 * 2,
            objects[0][3] + 31 * 2,
        )
        # 使用检测到的人脸大小作为ROI提取关键点
        kpts1 = img.find_keypoints(
            threshold=10, scale_factor=1.1, max_keypoints=100, roi=face
        )
        # 在第一个脸周围画一个矩形
        img.draw_rectangle(objects[0])

# 绘制关键点
print(kpts1)
img.draw_keypoints(kpts1, size=24)
img = sensor.snapshot()
time.sleep_ms(2000)

# FPS时钟
clock = time.clock()

while True:
    clock.tick()
    img = sensor.snapshot()
    # 从整个帧中提取关键点
    kpts2 = img.find_keypoints(
        threshold=10, scale_factor=1.1, max_keypoints=100, normalized=True
    )

    if kpts2:
        # 将第一组关键点与第二组进行匹配
        c = image.match_descriptor(kpts1, kpts2, threshold=85)
        match = c[6]  # C[6]中存储了匹配的数量。
        if match > 5:
            img.draw_rectangle(c[2:6])
            img.draw_cross(c[0], c[1], size=10)
            print(kpts2, "matched:%d dt:%d" % (match, c[7]))

    # 绘制帧率(FPS)
    img.draw_string(0, 0, "FPS:%.2f" % (clock.fps()))