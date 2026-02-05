import os

def search_specific_exe_recursive():
    # 设定要查找的特定文件名
    TARGET_EXE_NAME = "RemotePro.exe"
    
    cwd = os.getcwd()
    print(f"正在递归搜索路径: {cwd}")
    print(f"目标文件: {TARGET_EXE_NAME}\n")
    print(f"{'='*60}")

    # 定义我们要搜索的十六进制特征码
    targets = [
        # --- 最常见的情况 ---
        ("Int32 (标准 86400000)",          "00 5C 26 05"),
        ("Int32 (优化版 <= 86399999)",     "FF 5B 26 05"), # 重点：针对 < 变 <= 的情况

        # --- 64位整数 (qint64 / long long) ---
        # 注意：如果是64位程序，高位通常是0，但也可能被指令分割
        ("Int64 (标准 86400000)",          "00 5C 26 05 00 00 00 00"),
        ("Int64 (优化版 <= 86399999)",     "FF 5B 26 05 00 00 00 00"),
        
        ("Double (双精度 8.64e7)",     "00 00 00 00 70 99 94 41"),
        ("Float (单精度 8.64e7)",      "80 CB A4 4C"),
        ("Int32 (原始值 86400)",       "00 51 01 00") 
    ]

    found_file_count = 0
    found_pattern_count = 0

    # 递归遍历目录
    for root, dirs, files in os.walk(cwd):
        for filename in files:
            # 只匹配名字叫 RemotePro.exe 的文件 (忽略大小写)
            if filename.lower() == TARGET_EXE_NAME.lower():
                found_file_count += 1
                filepath = os.path.join(root, filename)
                print(f"正在分析文件: {filepath}")
                
                try:
                    with open(filepath, "rb") as f:
                        content = f.read()
                    
                    file_has_match = False
                    
                    # 循环查找所有特征码
                    for label, hex_str in targets:
                        pattern = bytes.fromhex(hex_str.replace(" ", ""))
                        
                        offset = 0
                        while True:
                            index = content.find(pattern, offset)
                            if index == -1:
                                break
                            
                            found_pattern_count += 1
                            file_has_match = True
                            print(f"  [√] 找到特征码！")
                            print(f"      类型: {label}")
                            print(f"      地址(Offset): {hex(index)}")
                            
                            offset = index + 1
                    
                    if not file_has_match:
                        print("  [X] 文件找到了，但内部未发现目标数值。")
                    
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
