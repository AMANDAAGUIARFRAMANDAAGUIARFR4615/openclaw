import socket
import struct
import threading
import time
import sys
import random

# 配置你的 Coturn 服务器地址
STUN_SERVER_IP = '150.109.57.128'
STUN_SERVER_PORT = 3478

def get_ip_info(sock):
    """
    向 STUN 服务器发送 Binding Request，并解析 XOR-MAPPED-ADDRESS
    获取本机在公网的 IP 和端口
    """
    sock.settimeout(2.0)
    print(f"[-] 正在向 STUN 服务器 {STUN_SERVER_IP}:{STUN_SERVER_PORT} 获取公网地址...")

    # 构建 STUN Binding Request
    message_type = 0x0001
    message_length = 0x0000
    magic_cookie = 0x2112A442
    trans_id =  struct.pack('12B', *[random.randint(0, 255) for _ in range(12)])
    packet = struct.pack('>HHI', message_type, message_length, magic_cookie) + trans_id

    try:
        sock.sendto(packet, (STUN_SERVER_IP, STUN_SERVER_PORT))
        data, addr = sock.recvfrom(2048)
    except socket.timeout:
        print("[!] 连接 STUN 服务器超时，请检查网络或防火墙 (UDP 3478)")
        sys.exit(1)

    # 解析响应，寻找 XOR-MAPPED-ADDRESS (Attribute 0x0020)
    # 头部占 20 字节
    idx = 20
    length = len(data)
    
    public_ip = None
    public_port = None

    while idx < length:
        attr_type, attr_len = struct.unpack('>HH', data[idx:idx+4])
        # 0x0020 是 XOR-MAPPED-ADDRESS
        # 0x0001 是 MAPPED-ADDRESS (旧标准，Coturn 也可能返回这个)
        if attr_type == 0x0020:
            # 解析 XOR 映射地址
            # 这里的端口和IP都需要与 magic_cookie 进行异或运算
            family = struct.unpack('B', data[idx+5:idx+6])[0]
            port_raw = struct.unpack('>H', data[idx+6:idx+8])[0]
            ip_raw = struct.unpack('>I', data[idx+8:idx+12])[0]
            
            public_port = port_raw ^ (magic_cookie >> 16)
            public_ip_int = ip_raw ^ magic_cookie
            public_ip = socket.inet_ntoa(struct.pack('>I', public_ip_int))
            break
        elif attr_type == 0x0001:
             # 解析普通映射地址 (无需异或)
            port_raw = struct.unpack('>H', data[idx+6:idx+8])[0]
            ip_raw = struct.unpack('>I', data[idx+8:idx+12])[0]
            public_port = port_raw
            public_ip = socket.inet_ntoa(struct.pack('>I', ip_raw))
            break
            
        idx += 4 + attr_len
    
    if public_ip and public_port:
        return public_ip, public_port
    else:
        print("[!] 未能从 STUN 响应中解析出地址")
        sys.exit(1)

def listen_msg(sock):
    """监听线程：接收消息"""
    while True:
        try:
            data, addr = sock.recvfrom(1024)
            text = data.decode('utf-8', errors='ignore')
            # 过滤掉 STUN 包或无关包，只显示文本
            if len(text) > 0 and text.isprintable():
                print(f"\r[来自 {addr[0]}:{addr[1]}] {text}\n> ", end="")
        except Exception:
            pass

def main():
    # 1. 创建 UDP Socket
    # 绑定到 0.0.0.0 和随机端口
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(('0.0.0.0', 0)) 

    # 2. 获取公网地址 (这一步至关重要，NAT 会建立映射)
    my_ip, my_port = get_ip_info(sock)
    print(f"\n[OK] 你的公网地址是: {my_ip}:{my_port}")
    print("请把上面这行地址复制给对方。\n")

    # 3. 交换信息 (手动信令)
    peer_str = input("输入对方的公网地址 (格式 IP:Port): ").strip()
    try:
        peer_ip, peer_port = peer_str.split(':')
        peer_port = int(peer_port)
    except ValueError:
        print("格式错误")
        return

    print(f"\n[-] 准备连接对方 {peer_ip}:{peer_port} ...")
    
    # 启动监听线程
    threading.Thread(target=listen_msg, args=(sock,), daemon=True).start()

    # 4. 打洞阶段 (Hole Punching)
    # 双方必须疯狂互发数据包，只要有一方的数据包挤进了对方刚开的口子，连接就建立了
    print("[-] 正在打洞 (发送握手包)...")
    for i in range(5):
        msg = f"punch_packet_{i}"
        sock.sendto(msg.encode(), (peer_ip, peer_port))
        time.sleep(0.5)
    
    print("[+] 打洞尝试结束，现在可以直接输入消息按回车发送。")
    print("> ", end="")

    # 5. 聊天主循环
    while True:
        msg = input()
        if msg:
            sock.sendto(msg.encode('utf-8'), (peer_ip, peer_port))
            print("> ", end="")

if __name__ == '__main__':
    main()