import cv2
import numpy as np
import time

def resize_image_with_ratio(image, max_dimension=1000):
    """
    辅助函数：将图片等比例缩放，使其长边不超过 max_dimension。
    返回: (缩放后的图片, 缩放比例)
    """
    h, w = image.shape[:2]
    
    # 如果图片本身就很小，不需要缩放
    if max(h, w) <= max_dimension:
        return image, 1.0
    
    # 计算缩放比例
    scale = max_dimension / max(h, w)
    new_w = int(w * scale)
    new_h = int(h * scale)
    
    # 快速插值缩放
    resized_img = cv2.resize(image, (new_w, new_h), interpolation=cv2.INTER_AREA)
    return resized_img, scale

def find_image_sift_optimized(source_path, template_path, min_match_count=10, max_process_size=960):
    """
    优化版 SIFT 匹配：
    1. 使用降采样加速特征提取
    2. 限制特征点数量
    3. 坐标映射回原图
    """
    
    # 1. 读取原始图片 (保持原图用于最终展示)
    img_template_orig = cv2.imread(template_path)
    img_source_orig = cv2.imread(source_path)

    if img_template_orig is None or img_source_orig is None:
        print("错误：无法读取图片，请检查路径。")
        return

    # 转灰度
    img1_gray = cv2.cvtColor(img_template_orig, cv2.COLOR_BGR2GRAY)
    img2_gray = cv2.cvtColor(img_source_orig, cv2.COLOR_BGR2GRAY)

    print(f"原图尺寸: {img2_gray.shape[::-1]}, 模板尺寸: {img1_gray.shape[::-1]}")

    # 开始计时
    start_time = time.time()

    # 2. 【核心优化】降采样处理
    # 大图通常很大，缩小处理能极大提升速度
    # 模板图如果也很巨大，也可以缩放，通常模板较小，这里主要缩放大图
    img2_small, scale_ratio = resize_image_with_ratio(img2_gray, max_dimension=max_process_size)
    
    # 模板图如果过大也建议缩小，否则特征尺度差异过大影响匹配
    # 这里为了逻辑简单，假设模板图尺寸适中，或者通过 max_process_size 控制
    # 实际工程中，建议也对 img1 进行适当缩放以匹配 img2_small 的分辨率层级
    # 但为了保证模板细节，这里只缩放大图作为搜索背景
    
    print(f"处理尺寸: {img2_small.shape[::-1]} (缩放比例: {scale_ratio:.4f})")

    # 3. 初始化 SIFT (限制特征点数量)
    # nfeatures=2000: 仅保留最强的2000个特征点，防止过多噪点拖慢速度
    sift = cv2.SIFT_create(nfeatures=2000)

    # 4. 检测并计算 (在小图上进行)
    kp1, des1 = sift.detectAndCompute(img1_gray, None)
    kp2, des2 = sift.detectAndCompute(img2_small, None)

    if des1 is None or des2 is None:
        print("无法提取特征点。")
        return

    # 5. FLANN 匹配
    FLANN_INDEX_KDTREE = 1
    index_params = dict(algorithm=FLANN_INDEX_KDTREE, trees=5)
    # checks=30: 减少搜索次数，略微牺牲精度换取速度 (原默认50)
    search_params = dict(checks=30) 
    
    flann = cv2.FlannBasedMatcher(index_params, search_params)
    matches = flann.knnMatch(des1, des2, k=2)

    # 6. 筛选优质匹配 (Ratio Test)
    good = []
    for m, n in matches:
        if m.distance < 0.75 * n.distance: # 0.7 -> 0.75 放宽一点条件，增加通过率
            good.append(m)

    match_time = time.time() - start_time
    print(f"算法耗时: {match_time:.4f} 秒, 匹配点数: {len(good)}")

    # 7. 计算位置并还原坐标
    if len(good) >= min_match_count:
        src_pts = np.float32([kp1[m.queryIdx].pt for m in good]).reshape(-1, 1, 2)
        dst_pts = np.float32([kp2[m.trainIdx].pt for m in good]).reshape(-1, 1, 2)

        # 计算单应性矩阵 (基于小图坐标)
        M, mask = cv2.findHomography(src_pts, dst_pts, cv2.RANSAC, 5.0)

        if M is not None:
            # 获取模板图尺寸
            h, w = img1_gray.shape
            pts = np.float32([[0, 0], [0, h - 1], [w - 1, h - 1], [w - 1, 0]]).reshape(-1, 1, 2)
            
            # 变换坐标 (得到在小图上的位置)
            dst_in_small = cv2.perspectiveTransform(pts, M)
            
            # 【核心优化】将坐标映射回原图尺寸
            # 坐标 / 缩放比例
            dst_in_original = dst_in_small / scale_ratio
            
            # 8. 绘图 (在原图上绘制)
            # 绘制边框
            img_result = cv2.polylines(img_source_orig, [np.int32(dst_in_original)], True, (0, 0, 255), 5, cv2.LINE_AA)
            
            # 显示结果
            # 为了方便在屏幕显示，最终结果再缩放一下用于 imshow，否则 4K 图屏幕放不下
            display_h = 800
            display_scale = display_h / img_result.shape[0]
            display_w = int(img_result.shape[1] * display_scale)
            img_display = cv2.resize(img_result, (display_w, display_h))
            
            cv2.imshow("Optimized Result", img_display)
            
            # 同时也显示匹配连线 (可选，用于调试，需要映射关键点，较麻烦，这里略去以保持代码整洁)
            print(f"成功定位！中心点坐标约: {np.mean(dst_in_original, axis=0).flatten()}")
            
            cv2.waitKey(0)
            cv2.destroyAllWindows()
            return True
        else:
            print("计算 Homography 失败。")
            return False
    else:
        print(f"匹配失败 (只有 {len(good)}/{min_match_count} 个匹配点)。")
        return False

if __name__ == "__main__":
    # 请替换为实际图片路径
    # 建议使用高分辨率图片进行测试，效果差异明显
    large_image = 'large_image.png' 
    sub_image = 'sub_image.png'
    
    # 如果没有图片，生成假图片用于测试代码运行
    import os
    if not os.path.exists(large_image):
        print("未检测到测试图片，正在生成随机测试图...")
        # 生成一个黑底图片
        img_l = np.zeros((2000, 2000, 3), dtype=np.uint8)
        # 画一些随机内容作为特征
        for i in range(100):
            cv2.circle(img_l, (np.random.randint(0,2000), np.random.randint(0,2000)), np.random.randint(10,50), (200,200,200), -1)
        # 从中切一块作为子图
        img_s = img_l[500:800, 500:800].copy()
        # 稍微旋转或缩放子图模拟真实场景
        M = cv2.getRotationMatrix2D((150, 150), 10, 1.0)
        img_s = cv2.warpAffine(img_s, M, (300, 300))
        
        cv2.imwrite(large_image, img_l)
        cv2.imwrite(sub_image, img_s)
        print("测试图片已生成。")

    find_image_sift_optimized(large_image, sub_image)