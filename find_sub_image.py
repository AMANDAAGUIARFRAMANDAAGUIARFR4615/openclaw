import cv2
import numpy as np
import matplotlib.pyplot as plt
import time  # 导入 time 模块用于计时

def find_image_sift(source_path, template_path, min_match_count=10):
    """
    使用 SIFT (Scale-Invariant Feature Transform) 特征点匹配算法。
    这种方法不依赖像素值的直接对比，而是基于图像的特征（角点、纹理），
    因此对缩放、旋转、亮度变化和压缩噪点有极强的鲁棒性。
    
    Args:
        source_path: 大图路径
        template_path: 子图路径
        min_match_count: 最少需要多少个特征点匹配成功才算找到 (默认 10)
    """
    
    # 1. 读取图片
    img1 = cv2.imread(template_path, cv2.IMREAD_GRAYSCALE) # 子图 (Query)
    img2 = cv2.imread(source_path, cv2.IMREAD_GRAYSCALE)   # 大图 (Train)

    if img1 is None or img2 is None:
        print("错误：无法读取图片。")
        return

    # 开始计时
    start_time = time.time()

    # 2. 初始化 SIFT 检测器
    try:
        sift = cv2.SIFT_create()
    except AttributeError:
        # 如果 OpenCV 版本较老，可能需要使用 ORB 作为备选 (虽然 SIFT 效果更好)
        print("警告: 当前 OpenCV 版本不包含 SIFT，尝试使用 ORB...")
        sift = cv2.ORB_create()

    # 3. 检测关键点并计算描述符
    print("正在计算图像特征点...")
    kp1, des1 = sift.detectAndCompute(img1, None)
    kp2, des2 = sift.detectAndCompute(img2, None)

    if des1 is None or des2 is None:
        print("错误：无法在图片中提取到足够的特征点。图片可能太小或太模糊。")
        return

    # 4. 特征匹配 (使用 FLANN 匹配器)
    # FLANN 是一个快速最近邻搜索库
    FLANN_INDEX_KDTREE = 1
    index_params = dict(algorithm=FLANN_INDEX_KDTREE, trees=5)
    search_params = dict(checks=50)
    
    # 如果是 SIFT 使用 FlannBasedMatcher，如果是 ORB 使用 BFMatcher
    if isinstance(sift, cv2.SIFT):
        flann = cv2.FlannBasedMatcher(index_params, search_params)
        matches = flann.knnMatch(des1, des2, k=2)
    else:
        # ORB 备选方案
        bf = cv2.BFMatcher(cv2.NORM_HAMMING, crossCheck=False)
        matches = bf.knnMatch(des1, des2, k=2)

    # 5. 筛选优质匹配点 (Lowe's ratio test)
    # 这一步过滤掉那些模棱两可的错误匹配
    good = []
    for m, n in matches:
        if m.distance < 0.7 * n.distance:
            good.append(m)

    # 结束计时
    end_time = time.time()
    elapsed_time = end_time - start_time

    print(f"找到的优质特征点匹配数: {len(good)}")
    print(f"匹配算法执行耗时: {elapsed_time:.4f} 秒")

    if len(good) >= min_match_count:
        print(f"匹配成功！(匹配点 > {min_match_count})")
        
        # 6. 计算单应性矩阵 (Homography)
        # 这能帮我们找到子图在大图中的具体位置、角度和透视变换
        src_pts = np.float32([kp1[m.queryIdx].pt for m in good]).reshape(-1, 1, 2)
        dst_pts = np.float32([kp2[m.trainIdx].pt for m in good]).reshape(-1, 1, 2)

        # RANSAC 算法能进一步剔除错误匹配点
        M, mask = cv2.findHomography(src_pts, dst_pts, cv2.RANSAC, 5.0)
        
        if M is not None:
            # 获取子图的尺寸
            h, w = img1.shape
            
            # 定义子图的四个角
            pts = np.float32([[0, 0], [0, h - 1], [w - 1, h - 1], [w - 1, 0]]).reshape(-1, 1, 2)
            
            # 计算这四个角在大图中的新位置
            dst = cv2.perspectiveTransform(pts, M)

            # 7. 绘图
            img2_color = cv2.imread(source_path) # 重新读取彩图用于显示
            
            # 画出多边形边框 (绿色, 线宽 3)
            # 注意：这里用 polylines 因为考虑到可能存在旋转或透视变换，矩形可能会变成四边形
            img2_with_box = cv2.polylines(img2_color, [np.int32(dst)], True, (0, 255, 0), 5, cv2.LINE_AA)
            
            # 画出特征点连线图 (可选，用于调试)
            draw_params = dict(matchColor=(0, 255, 0), singlePointColor=None, matchesMask=mask.ravel().tolist(), flags=2)
            img3 = cv2.drawMatches(img1, kp1, img2, kp2, good, None, **draw_params)

            # 显示结果
            plt.figure(figsize=(12, 6))
            plt.subplot(121), plt.imshow(cv2.cvtColor(img2_with_box, cv2.COLOR_BGR2RGB))
            plt.title(f'Detected Result ({elapsed_time:.2f}s)'), plt.axis('off')
            
            plt.subplot(122), plt.imshow(img3)
            plt.title('Feature Matches'), plt.axis('off')
            plt.show()
            
        else:
            print("计算位置失败 (Homography 计算未收敛)。")
            
    else:
        print(f"匹配失败 - 优质匹配点不足 ({len(good)}/{min_match_count})。")
        print("可能原因：图片差异过大，或者子图纹理太少（纯色块很难匹配）。")

if __name__ == "__main__":
    large_image = 'large_image.png'
    sub_image = 'sub_image.png'
    
    # 执行特征匹配
    find_image_sift(large_image, sub_image)