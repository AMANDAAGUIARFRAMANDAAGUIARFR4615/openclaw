import sys
import socket
import struct
import random
import time
from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QHBoxLayout, QLabel, QLineEdit, QPushButton, 
                             QTextBrowser, QMessageBox, QGroupBox)
from PyQt6.QtCore import QThread, pyqtSignal, Qt

# --- 配置 ---
STUN_SERVER_IP = '150.109.57.128'
STUN_SERVER_PORT = 3478

# --- STUN 协议逻辑 (工具函数) ---
def get_public_ip(sock):
    """通过 STUN 获取公网地址 (阻塞式，建议在监听开启前调用)"""
    sock.settimeout(2.0)
    
    # 构建 Binding Request
    message_type = 0x0001
    message_length = 0x0000
    magic_cookie = 0x2112A442
    trans_id = struct.pack('12B', *[random.randint(0, 255) for _ in range(12)])
    packet = struct.pack('>HHI', message_type, message_length, magic_cookie) + trans_id

    try:
        sock.sendto(packet, (STUN_SERVER_IP, STUN_SERVER_PORT))
        data, _ = sock.recvfrom(2048)
    except socket.timeout:
        raise Exception("连接 STUN 服务器超时")

    # 解析响应
    idx = 20
    length = len(data)
    
    while idx < length:
        attr_type, attr_len = struct.unpack('>HH', data[idx:idx+4])
        # 0x0020: XOR-MAPPED-ADDRESS
        if attr_type == 0x0020:
            port_raw = struct.unpack('>H', data[idx+6:idx+8])[0]
            ip_raw = struct.unpack('>I', data[idx+8:idx+12])[0]
            
            public_port = port_raw ^ (magic_cookie >> 16)
            public_ip_int = ip_raw ^ magic_cookie
            public_ip = socket.inet_ntoa(struct.pack('>I', public_ip_int))
            return public_ip, public_port
        
        # 0x0001: MAPPED-ADDRESS (兼容旧版)
        elif attr_type == 0x0001:
            port_raw = struct.unpack('>H', data[idx+6:idx+8])[0]
            ip_raw = struct.unpack('>I', data[idx+8:idx+12])[0]
            public_ip = socket.inet_ntoa(struct.pack('>I', ip_raw))
            return public_ip, port_raw
            
        idx += 4 + attr_len
        
    raise Exception("未解析到公网地址")

# --- 后台监听线程 ---
class ListenerThread(QThread):
    new_message = pyqtSignal(str, str) # message, ip:port

    def __init__(self, sock):
        super().__init__()
        self.sock = sock
        self.running = True

    def run(self):
        print("[Thread] 开始监听端口...")
        while self.running:
            try:
                # 阻塞接收，直到有数据或 socket 关闭
                data, addr = self.sock.recvfrom(2048)
                text = data.decode('utf-8', errors='ignore')
                
                # 简单过滤掉非打印字符（过滤掉可能的 STUN 包残留）
                if text and text.isprintable():
                    sender = f"{addr[0]}:{addr[1]}"
                    self.new_message.emit(text, sender)
            except OSError:
                # Socket 关闭时会抛出异常，退出循环
                break
            except Exception as e:
                print(f"Receive Error: {e}")

    def stop(self):
        self.running = False

# --- 主窗口 ---
class P2PChatWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("UDP P2P 穿透聊天 (Qt版)")
        self.resize(500, 600)
        
        # 网络相关变量
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(('0.0.0.0', 0)) # 绑定随机端口
        self.peer_addr = None # (ip, port)
        self.listener = None

        self.init_ui()
        
    def init_ui(self):
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        layout = QVBoxLayout()

        # 1. 顶部：我的信息区域
        group_me = QGroupBox("我的信息")
        layout_me = QVBoxLayout()
        self.lbl_status = QLabel("状态: 未连接 STUN")
        self.lbl_my_addr = QLabel("公网地址: ???")
        self.lbl_my_addr.setStyleSheet("font-weight: bold; font-size: 14px; color: blue;")
        self.btn_get_ip = QPushButton("1. 获取我的公网 IP (STUN)")
        self.btn_get_ip.clicked.connect(self.on_get_ip)
        
        layout_me.addWidget(self.lbl_status)
        layout_me.addWidget(self.lbl_my_addr)
        layout_me.addWidget(self.btn_get_ip)
        group_me.setLayout(layout_me)
        layout.addWidget(group_me)

        # 2. 中部：对方信息区域
        group_peer = QGroupBox("连接对方")
        layout_peer = QHBoxLayout()
        self.input_peer_ip = QLineEdit()
        self.input_peer_ip.setPlaceholderText("输入对方地址，例如 123.1.1.1:8888")
        self.btn_connect = QPushButton("2. 连接/打洞")
        self.btn_connect.clicked.connect(self.on_connect_peer)
        
        layout_peer.addWidget(self.input_peer_ip)
        layout_peer.addWidget(self.btn_connect)
        group_peer.setLayout(layout_peer)
        layout.addWidget(group_peer)

        # 3. 聊天记录显示
        self.chat_display = QTextBrowser()
        layout.addWidget(self.chat_display)

        # 4. 底部：发送区域
        layout_send = QHBoxLayout()
        self.input_msg = QLineEdit()
        self.input_msg.setPlaceholderText("输入消息...")
        self.input_msg.returnPressed.connect(self.on_send_msg)
        self.btn_send = QPushButton("发送")
        self.btn_send.clicked.connect(self.on_send_msg)
        
        layout_send.addWidget(self.input_msg)
        layout_send.addWidget(self.btn_send)
        layout.addLayout(layout_send)

        main_widget.setLayout(layout)

    def log(self, text, color="black"):
        """往聊天框添加系统日志"""
        self.chat_display.append(f'<span style="color:{color}">{text}</span>')

    def on_get_ip(self):
        """步骤1：获取本机 IP"""
        self.btn_get_ip.setEnabled(False)
        self.log("正在连接 STUN 服务器...", "gray")
        QApplication.processEvents() # 刷新界面

        try:
            my_ip, my_port = get_public_ip(self.sock)
            addr_str = f"{my_ip}:{my_port}"
            self.lbl_my_addr.setText(f"公网地址: {addr_str}")
            self.lbl_my_addr.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse) # 允许复制
            self.lbl_status.setText("状态: STUN 获取成功")
            self.log(f"成功获取地址: {addr_str} (请复制发给对方)", "green")
            
            # 开启监听线程
            self.start_listener()
            
        except Exception as e:
            self.log(f"STUN 失败: {e}", "red")
            self.btn_get_ip.setEnabled(True)

    def start_listener(self):
        if self.listener is not None:
            return
        
        # 设置 socket 为阻塞模式供线程使用 (STUN时设了timeout，这里要改回来或保留视情况而定)
        # 最好设为 None (完全阻塞) 或一个较长的 timeout
        self.sock.settimeout(None)
        
        self.listener = ListenerThread(self.sock)
        self.listener.new_message.connect(self.on_recv_msg)
        self.listener.start()

    def on_connect_peer(self):
        """步骤2：设置对方 IP 并尝试打洞"""
        text = self.input_peer_ip.text().strip()
        if not text:
            QMessageBox.warning(self, "错误", "请输入对方的 IP:Port")
            return
        
        try:
            ip, port = text.split(':')
            self.peer_addr = (ip, int(port))
            self.log(f"已设置目标: {self.peer_addr}", "blue")
            
            # 打洞逻辑：发送几个空包或握手包
            self.log("正在尝试打洞 (发送握手包)...", "orange")
            for i in range(5):
                msg = f"HANDSHAKE_{i}"
                self.sock.sendto(msg.encode(), self.peer_addr)
                # 稍微 sleep 一下避免发太快，注意这里是在主线程，太久会卡
                # 但发5个包很快，不需要 QThread
                QApplication.processEvents() 
                time.sleep(0.1) 
            
            self.log("打洞包发送完毕，请尝试发送消息。", "blue")

        except ValueError:
            QMessageBox.warning(self, "错误", "格式错误，应为 IP:Port")

    def on_send_msg(self):
        if not self.peer_addr:
            QMessageBox.warning(self, "错误", "请先连接对方 (步骤2)")
            return
        
        msg = self.input_msg.text().strip()
        if not msg:
            return

        try:
            self.sock.sendto(msg.encode('utf-8'), self.peer_addr)
            self.chat_display.append(f'<span style="color:blue">我: {msg}</span>')
            self.input_msg.clear()
        except Exception as e:
            self.log(f"发送失败: {e}", "red")

    def on_recv_msg(self, text, sender):
        # 如果收到的是握手包，不显示在聊天气泡里，只在日志提示
        if "HANDSHAKE" in text:
            # 只有第一次收到握手包时才提示连接成功
            self.log(f"收到来自 {sender} 的打洞握手包，通道可能已建立！", "green")
            return

        self.chat_display.append(f'<span style="color:red">[{sender}]: {text}</span>')

    def closeEvent(self, event):
        """关闭窗口时清理资源"""
        if self.listener:
            self.listener.stop()
        self.sock.close()
        event.accept()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = P2PChatWindow()
    window.show()
    sys.exit(app.exec())