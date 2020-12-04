import socket,subprocess,threading
from subprocess import DEVNULL
import queue
import pathlib

class TestPeer():

    MSG_LEN = 2048

    def __init__(self,tcp_port : int,udp_port : int,on_tcp_socket = None):
        self.udp_port = udp_port
        self.tcp_port = tcp_port
        self.udp_queue = queue.Queue()
        self.on_tcp_socket = on_tcp_socket

    def start(self):
        s = socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        s.bind(('',self.udp_port))
        self.udp_socket = s

        s = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        s.bind(('',self.tcp_port))
        s.listen(1)
        self.tcp_socket = s
        threading.Thread(target=self.udp_receive_loop).start()
        threading.Thread(target=self.tcp_listen_loop).start()

    def udp_receive_loop(self):
        while True:
            data,addr = self.udp_socket.recvfrom(TestPeer.MSG_LEN)
            self.udp_queue.put(data)

    def tcp_listen_loop(self):
        while True:
            s,_ = self.tcp_socket.accept()
            if self.on_tcp_socket is not None:
                threading.Thread(target=lambda: self.on_tcp_socket(s)).start()
                self.on_tcp_socket(s)
            else:
                s.close()

class Proxy():
    def __init__(self,local_port,remote_port,debug_port):
        self.local_port = local_port
        self.remote_port = remote_port
        self.debug_port = debug_port

        rp = pathlib.Path(__file__).parent.parent
        oxo = rp / 'oxo'
        if not oxo.exists():
            oxo = rp / 'build' / 'oxo'
        if not oxo.exists():
            raise "can not find executable 'oxo'"
        self.exec_path = oxo.resolve()

    def start(self):
        cmd = '{} -l {} -r {} -d {}'.format(
            self.exec_path,self.local_port,self.remote_port,self.debug_port).split(' ')
        self.process = subprocess.Popen(cmd,stdout=DEVNULL,stderr=DEVNULL)

def main():
    def tcp_handler(s):
        while True:
            data = s.recv(1024)
            if data == b'':
                s.close()
                break

    peer = TestPeer(2222,5555)
    peer.on_tcp_socket = tcp_handler
    peer.start()
    Proxy(1111,2222,5555).start()

    while True:
        print(peer.udp_queue.get())

if __name__ == '__main__':
    main()
