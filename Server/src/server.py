import socket
import threading
import time
import datetime

# Server port UDP
PORT = 8888
# Check alive UDP
HEART_BEAT = 100
# Number of sound sections to be played
SECTIONS = 1

SRV_SOCK = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
RUNNING = True

CLIENT_SOCKETS = []
PARTS = []
PARTS.append(open('8BA_Part1.wav', 'rb').read())
PARTS.append(open('8BA_Part2.wav', 'rb').read())
def listen(sock:socket):
    global RUNNING
    while RUNNING:
        try:
            message = sock.recvfrom(1024)
            #print(f'{message[1]}: {message[0]}')
            if message[0][:4] == b'INIT':
                print(f'Client Connection: {message[1][0]}:{message[1][1]}')
                CLIENT_SOCKETS.append(message[1])
                sock.sendto(b'OK', message[1])
            if message[0][:10] == b'DISCONNECT':
                print(f'Client Disconnect: {message[1][0]}:{message[1][1]}')
                CLIENT_SOCKETS.remove(message[1])
            if message[0][:4] == b'YEET':
                print(f'Master Initiate Download: {message[1][0]}:{message[1][1]}')
                for cli_sock in CLIENT_SOCKETS:
                    sock.sendto(b'DOWNLOAD', cli_sock)
            if message[0][:7] == b'EXECUTE':
                print(f'Master Initiate Execute: {message[1][0]}:{message[1][1]}')
                for cli_sock in CLIENT_SOCKETS:
                    sock.sendto(b'EXECUTE', cli_sock)
            if message[0][:3] == b'END':
                print(CLIENT_SOCKETS)
                RUNNING = False
                for cli_sock in CLIENT_SOCKETS:
                    sock.sendto(b'KILL', cli_sock)
                break
        except ConnectionResetError as e:
            print("Client disconnect: ", e)

def heartbeat(sock:socket):
    global RUNNING
    while RUNNING:
        for cli_sock in CLIENT_SOCKETS:
            sock.sendto(b'PING', cli_sock)
        print('HEART BEAT')
        time.sleep(HEART_BEAT/1000)

def download(dl_cli:socket, part_num):
    dl_cli.sendall(PARTS[part_num])

def download_listen():
    global RUNNING
    part_num = 0
    download_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    download_sock.bind(("0.0.0.0",PORT))
    download_sock.listen()
    while RUNNING:
        conn, addr = download_sock.accept()
        with conn:
            print(f'Sending part to: {addr}')
            print(f'Part {part_num} Size: {len(PARTS[part_num])}')
            print(f"TEST SIZE: {len(PARTS[part_num]).to_bytes(4, 'little')}")
            conn.sendall(len(PARTS[part_num]).to_bytes(4, 'little'))
            conn.sendall(PARTS[part_num])
            conn.close()
            #download_thread = threading.Thread(target=download, args=[conn, part_num])
            #download_thread.daemon = True
            #download_thread.start()
            part_num = (part_num+1)%len(PARTS)


def main():
    global RUNNING
    print(f'Binding server to port: {PORT}')
    SRV_SOCK.bind(("0.0.0.0",PORT))
    
    listen_thread = threading.Thread(target=listen, args=[SRV_SOCK])
    listen_thread.daemon = True
    listen_thread.start()

    download_listen_thread = threading.Thread(target=download_listen)
    download_listen_thread.daemon = True
    download_listen_thread.start()
    
    heartbeat_thread = threading.Thread(target=heartbeat, args=[SRV_SOCK])
    heartbeat_thread.daemon = True
    heartbeat_thread.start()
    
    listen_thread.join()
    print("CLOSED")
    exit()

if __name__ == "__main__":
    main()