import cv2
import numpy as np
import time
import os

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

def find_image_sift_affine(source_path, template_path, min_match_count=4, max_process_size=960):
    """
    优化版 SIFT 匹配 (仿射变换版)：
    解决红框扭曲问题，强制保持矩形形状。
    """
    
    # 1. 读取原始图片
    img_template_orig = cv2.imread(template_path)
    img_source_orig = cv2.imread(source_path)

    if img_template_orig is None or img_source_orig is None:
        print("错误：无法读取图片，请检查路径。")
        return

    # 转灰度
    img1_gray = cv2.cvtColor(img_template_orig, cv2.COLOR_BGR2GRAY)
    img2_gray = cv2.cvtColor(img_source_orig, cv2.COLOR_BGR2GRAY)

    # 针对极小图的优化：强行放大
    if min(img1_gray.shape) < 100:
        print("提示：模板图过小，正在进行放大预处理...")
        img1_gray = cv2.resize(img1_gray, None, fx=2, fy=2, interpolation=cv2.INTER_LINEAR)

    print(f"原图尺寸: {img2_gray.shape[::-1]}, 模板尺寸: {img1_gray.shape[::-1]}")

    start_time = time.time()

    # 2. 降采样大图
    img2_small, scale_ratio = resize_image_with_ratio(img2_gray, max_dimension=max_process_size)
    print(f"处理尺寸: {img2_small.shape[::-1]} (缩放比例: {scale_ratio:.4f})")

    # 3. 初始化 SIFT
    # 增加 edgeThreshold 以适应文字/UI
    sift = cv2.SIFT_create(nfeatures=2000, edgeThreshold=20)

    # 4. 检测并计算
    kp1, des1 = sift.detectAndCompute(img1_gray, None)
    kp2, des2 = sift.detectAndCompute(img2_small, None)

    if des1 is None or des2 is None:
        print("无法提取特征点。")
        return

    # 5. FLANN 匹配
    index_params = dict(algorithm=1, trees=5)
    search_params = dict(checks=50) # 稍微增加 checks 提高精度
    
    flann = cv2.FlannBasedMatcher(index_params, search_params)
    matches = flann.knnMatch(des1, des2, k=2)

    # 6. 筛选优质匹配
    good = []
    for m, n in matches:
        if m.distance < 0.75 * n.distance:
            good.append(m)

    match_time = time.time() - start_time
    print(f"算法耗时: {match_time:.4f} 秒, 匹配点数: {len(good)}")

    # 7. 计算位置 (改用仿射变换)
    if len(good) >= min_match_count:
        src_pts = np.float32([kp1[m.queryIdx].pt for m in good]).reshape(-1, 1, 2)
        dst_pts = np.float32([kp2[m.trainIdx].pt for m in good]).reshape(-1, 1, 2)

        # 【核心修改点】
        # 使用 estimateAffinePartial2D 代替 findHomography
        # 作用：限制变换只能是 "旋转 + 缩放 + 平移"，绝对禁止 3D 透视扭曲
        M, inliers = cv2.estimateAffinePartial2D(src_pts, dst_pts)

        if M is not None:
            # 获取模板图尺寸
            h, w = img1_gray.shape
            # 定义四个角点
            pts = np.float32([[0, 0], [0, h - 1], [w - 1, h - 1], [w - 1, 0]]).reshape(-1, 1, 2)
            
            # 【核心修改点】
            # 仿射矩阵是 2x3 的，必须使用 cv2.transform (perspectiveTransform 只能用于 3x3)
            dst_in_small = cv2.transform(pts, M)
            
            # 坐标映射回原图
            dst_in_original = dst_in_small / scale_ratio
            
            # 8. 绘图
            img_result = img_source_orig.copy()
            cv2.polylines(img_result, [np.int32(dst_in_original)], True, (0, 0, 255), 5, cv2.LINE_AA)
            
            # 显示结果
            display_h = 800
            display_scale = display_h / img_result.shape[0]
            display_w = int(img_result.shape[1] * display_scale)
            img_display = cv2.resize(img_result, (display_w, display_h))
            
            cv2.imshow("Affine Corrected Result", img_display)
            print(f"成功定位！中心点坐标约: {np.mean(dst_in_original, axis=0).flatten()}")
            
            cv2.waitKey(0)
            cv2.destroyAllWindows()
            return True
        else:
            print("计算仿射矩阵失败 (匹配点分布可能过于集中)。")
            return False
    else:
        print(f"匹配失败 (只有 {len(good)}/{min_match_count} 个匹配点)。")
        return False

if __name__ == "__main__":
    # 测试文件路径
    large_image = 'large_image.png' 
    sub_image = 'sub_image4.png'
    
    # 自动生成测试图片 (如果不存在)
    if not os.path.exists(large_image):
        print("生成测试图片中...")
        img_l = np.zeros((1000, 1000, 3), dtype=np.uint8)
        # 绘制背景噪点
        for i in range(200):
            cv2.circle(img_l, (np.random.randint(0,1000), np.random.randint(0,1000)), np.random.randint(5,20), (200,200,200), -1)
        
        # 绘制一个矩形目标
        cv2.rectangle(img_l, (400, 400), (600, 600), (0, 255, 0), -1)
        cv2.putText(img_l, "Target", (420, 500), cv2.FONT_HERSHEY_SIMPLEX, 1.5, (0,0,0), 3)
        
        # 截取模板
        img_s = img_l[400:600, 400:600].copy()
        
        cv2.imwrite(large_image, img_l)
        cv2.imwrite(sub_image, img_s)

    find_image_sift_affine(large_image, sub_image)