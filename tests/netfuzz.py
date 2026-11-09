"""Black-box UDP fuzzer for omohaaded: malformed connectionless and sequenced packets.

Usage: python3 tests/netfuzz.py <port> <seconds> [seed]
"""
import os, random, socket, sys, time

port = int(sys.argv[1]); seconds = float(sys.argv[2]); seed = int(sys.argv[3]) if len(sys.argv) > 3 else 1
rnd = random.Random(seed)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setblocking(False)
cmds = [b"getstatus", b"getinfo", b"getchallenge", b"connect", b"rcon", b"ipAuthorize", b"disconnect",
        b"getKey", b"authorizeThis", b"\x02getstatus", b"\x02connect", b"print", b"echo", b"infoResponse"]

def junk(n):
    alphabet = b'\\"%;\n\r\x00\xff' + bytes(range(32, 127))
    return bytes(rnd.choice(alphabet) for _ in range(n))

def userinfo():
    pairs = []
    for _ in range(rnd.randint(0, 40)):
        pairs.append(b"\\" + junk(rnd.randint(0, 40)) + b"\\" + junk(rnd.randint(0, 300)))
    return b"".join(pairs)

def packet():
    kind = rnd.random()
    if kind < 0.55:
        cmd = rnd.choice(cmds)
        args = rnd.choice([b"", b" " + junk(rnd.randint(1, 60)), b' "' + userinfo() + b'"',
                           b" " + junk(rnd.randint(500, 1600)), b" %s%s%n%x" * rnd.randint(1, 30)])
        return b"\xff\xff\xff\xff" + cmd + args
    if kind < 0.85:  # sequenced packet with garbage payload and random qport
        return os.urandom(4) + os.urandom(2) + os.urandom(rnd.randint(0, 1400))
    return os.urandom(rnd.randint(0, 2000))

sent, end = 0, time.time() + seconds
while time.time() < end:
    try:
        sock.sendto(packet(), ("127.0.0.1", port))
        sent += 1
    except (BlockingIOError, OSError):
        time.sleep(0.001)
    try:
        while sock.recv(65536):
            pass
    except (BlockingIOError, OSError):
        pass
    if sent % 200 == 0:
        time.sleep(0.005)
print(f"sent {sent} packets")
