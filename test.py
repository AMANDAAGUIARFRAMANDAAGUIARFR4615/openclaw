import socket
import struct
import binascii
import os

def get_stun_response(server_ip='127.0.0.1', server_port=3478):
    print(f"[-] 正在尝试连接 STUN 服务器: {server_ip}:{server_port}...")

    # 创建 UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(3.0)

    # STUN Binding Request Header (RFC 5389)
    # Type: 0x0001 (Binding Request)
    # Length: 0x0000
    # Magic Cookie: 0x2112A442
    # Transaction ID: 12 random bytes
    message_type = 0x0001
    message_length = 0x0000
    magic_cookie = 0x2112A442
    trans_id = os.urandom(12)

    # 打包数据
    packet = struct.pack('>HHI', message_type, message_length, magic_cookie) + trans_id

    try:
        sock.sendto(packet, (server_ip, server_port))
        data, addr = sock.recvfrom(2048)
        
        print(f"[+] 成功收到响应! 来自: {addr}")
        
        # 解析响应头
        resp_type, resp_len, resp_cookie = struct.unpack('>HHI', data[:8])
        
        if resp_type == 0x0101: # Binding Success Response
            print("[+] 响应类型: Binding Success (0x0101)")
            # 这里可以进一步解析 XOR-MAPPED-ADDRESS 属性来获取 IP
            # 为了保持代码简单，只要收到 0x0101 就证明 Coturn 活着
            return True
        else:
            print(f"[!] 收到意外的响应类型: {hex(resp_type)}")
            return False

    except socket.timeout:
        print("[!] 请求超时。服务器未响应或防火墙拦截了 UDP 3478 端口。")
        return False
    except Exception as e:
        print(f"[!] 发生错误: {e}")
        return False
    finally:
        sock.close()

if __name__ == '__main__':
    # 如果你在本机运行 docker，使用 127.0.0.1
    # 如果是远程服务器，请替换为服务器 IP
    get_stun_response('150.109.57.128', 3478)