import socket
import struct
import time

HOST = '127.0.0.1'
PORT = 8080

def send_request(sock, args):
    # Encode args as custom binary protocol
    payload = struct.pack('<I', len(args))  # number of strings
    for arg in args:
        encoded = arg.encode()
        payload += struct.pack('<I', len(encoded)) + encoded
    sock.send(struct.pack('<I', len(payload)) + payload)

    # Read response header
    header = sock.recv(4)
    if not header:
        print("No response from server.")
        return
    (size,) = struct.unpack('<I', header)
    body = sock.recv(size)
    print("Response:", body)

with socket.create_connection((HOST, PORT)) as s:
    print("Connected to server")

    print("Setting key foo = bar")
    send_request(s, ['set', 'foo', 'bar'])

    print("Setting TTL to 2000ms")
    send_request(s, ['pexpire', 'foo', '2000'])

    print("Sleeping 1 second")
    time.sleep(1)

    print("PTTL (should be ~1000)")
    send_request(s, ['pttl', 'foo'])

    print("Sleeping 2 seconds")
    time.sleep(2)

    print("GET foo (should be nil)")
    send_request(s, ['get', 'foo'])

    print("PTTL after expiry (should be -2)")
    send_request(s, ['pttl', 'foo'])

    print("Done.")
