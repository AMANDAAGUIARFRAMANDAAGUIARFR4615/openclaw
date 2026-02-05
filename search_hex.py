import os

def search_specific_exe_recursive():
    # --- 配置区域 ---
    
    # 设定要查找的特定文件名 (忽略大小写)
    TARGET_EXE_NAME = "RemotePro.exe"
    
    # 1. 定义十六进制特征码
    # 格式: (标签, 十六进制字符串)
    hex_definitions = [
        ("Int32 (标准 86400000)",         "00 5C 26 05"),
        ("Int32 (优化版 <= 86399999)",    "FF 5B 26 05"), 
        ("Int64 (标准 86400000)",         "00 5C 26 05 00 00 00 00"),
        ("Double (双精度 8.64e7)",        "00 00 00 00 70 99 94 41"),
        ("Float (单精度 8.64e7)",         "80 CB A4 4C"),
        ("Int32 (原始值 86400)",       "00 51 01 00") 
    ]

    # 2. 定义字符串特征码 (新增功能)
    # 格式: (标签, 要搜索的文本内容, 编码格式)
    # 提示: Windows 程序内部常用 'utf-16-le' (宽字符) 或 'ascii'/'utf-8'
    str_definitions = [
        ("示例: 过期 (ASCII)",    "expireAt", "utf-8"),
        ("示例: 过期 (宽字符)",   "expireAt", "utf-16-le"),
        ("示例: 错误提示",            "Error 404",            "utf-8"),
        ("示例: localhost",           "127.0.0.1",            "ascii")
    ]

    # --- 初始化：将所有特征码统一转换为 bytes ---
    
    targets = [] # 最终存储 (标签, bytes对象)

    # 处理十六进制
    for label, hex_str in hex_definitions:
        try:
            pattern_bytes = bytes.fromhex(hex_str.replace(" ", ""))
            targets.append((label, pattern_bytes))
        except ValueError:
            print(f"[配置错误] 十六进制格式有误: {label}")

    # 处理字符串
    for label, text, encoding in str_definitions:
        try:
            pattern_bytes = text.encode(encoding)
            # 为了方便区分，我们在标签里加上编码提示
            full_label = f"{label} [{encoding}]"
            targets.append((full_label, pattern_bytes))
        except LookupError:
            print(f"[配置错误] 不支持的编码格式: {encoding} ({label})")

    # --- 开始搜索流程 ---

    cwd = os.getcwd()
    print(f"正在递归搜索路径: {cwd}")
    print(f"目标文件: {TARGET_EXE_NAME}")
    print(f"加载特征码: {len(targets)} 个 (包含Hex和字符串)")
    print(f"{'='*60}")

    found_file_count = 0
    found_pattern_count = 0

    # 递归遍历目录
    for root, dirs, files in os.walk(cwd):
        for filename in files:
            # 只匹配目标文件名 (忽略大小写)
            if filename.lower() == TARGET_EXE_NAME.lower():
                found_file_count += 1
                filepath = os.path.join(root, filename)
                print(f"正在分析文件: {filepath}")
                
                try:
                    with open(filepath, "rb") as f:
                        content = f.read()
                    
                    file_has_match = False
                    
                    # 循环查找所有特征码 (targets 列表现在包含了 hex 和 string 转换后的 bytes)
                    for label, pattern in targets:
                        
                        offset = 0
                        while True:
                            index = content.find(pattern, offset)
                            if index == -1:
                                break
                            
                            found_pattern_count += 1
                            file_has_match = True
                            
                            # 打印结果
                            print(f"  [√] 找到特征码！")
                            print(f"      类型: {label}")
                            # 显示找到的内容的十六进制预览（如果是字符串，这能帮你确认是否找对）
                            preview_hex = pattern.hex(' ').upper()
                            if len(preview_hex) > 20: preview_hex = preview_hex[:20] + "..."
                            print(f"      内容: {preview_hex}")
                            print(f"      地址(Offset): {hex(index)}")
                            
                            offset = index + 1
                    
                    if not file_has_match:
                        print("  [X] 文件找到了，但内部未发现任何目标特征。")
                    
                    print("-" * 40)

                except Exception as e:
                    print(f"  [!] 无法读取文件: {e}")

    print(f"{'='*60}")
    if found_file_count == 0:
        print(f"未找到名为 {TARGET_EXE_NAME} 的文件。")
    else:
        print(f"搜索结束。找到 {found_file_count} 个文件，共 {found_pattern_count} 处匹配。")

if __name__ == "__main__":
    search_specific_exe_recursive()