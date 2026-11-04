#!/usr/bin/env python3
"""Exercise the real server and client using only the Python standard library."""

import re
import select
import socket
import subprocess
import tempfile
from contextlib import ExitStack
from pathlib import Path


ROOT = Path(__file__).resolve().parent


def read_line(peer):
    data = bytearray()
    while not data.endswith(b"\n"):
        chunk = peer.recv(1)
        assert chunk, f"Unexpected disconnect: {data!r}"
        data.extend(chunk)
    return data.decode()


def read_area(peer):
    message = read_line(peer)
    match = re.fullmatch(r"POS (\d+) (\d+) AREA ([0-9](?:,[0-9]){8})\n", message)
    assert match, message
    x, y = map(int, match.group(1, 2))
    assert 0 <= x < 19 and 0 <= y < 19, message
    return x, y


def hello(peer, name):
    peer.sendall(f"HELLO {name}\r\n".encode())
    assert read_line(peer).startswith(f"WELCOME {name} (")
    return read_area(peer)


def quit_peer(peer, name):
    peer.sendall(b"QUIT\n")
    assert read_line(peer) == f"BYE {name}\n"
    assert peer.recv(1) == b""


def main():
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]

    with ExitStack() as stack, tempfile.TemporaryFile() as log:
        server = subprocess.Popen(
            [str(ROOT / "zoo_server"), "-d", str(port)],
            cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=log,
        )
        assert server.stdin is not None and server.stdout is not None
        try:
            assert b"nasloucha" in server.stdout.readline()

            def connect():
                peer = stack.enter_context(socket.create_connection(("127.0.0.1", port)))
                peer.settimeout(3)
                return peer

            alice = connect()
            alice.sendall(b"MSG before HELLO\n")
            assert read_line(alice) == "ERROR nejprve pouzijte HELLO name\n"
            ax, ay = hello(alice, "Alice")
            bob = connect()
            bx, by = hello(bob, "Bob")
            assert read_line(bob) == f"POS Alice {ax} {ay}\n"
            assert read_line(alice) == f"POS Bob {bx} {by}\n"
            carol = connect()
            # A nickname equal to the map marker must remain a peer position.
            cx, cy = hello(carol, "AREA")
            assert read_line(carol) == f"POS Alice {ax} {ay}\n"
            assert read_line(carol) == f"POS Bob {bx} {by}\n"
            assert read_line(alice) == f"POS AREA {cx} {cy}\n"
            assert read_line(bob) == f"POS AREA {cx} {cy}\n"

            tasks = Path(f"/proc/{server.pid}/task")
            assert len(list(tasks.iterdir())) >= 4, "Missing per-client threads"
            children = tasks / str(server.pid) / "children"
            assert not children.read_text().strip(), "Client handlers must not fork"

            alice.sendall(b"MSG\nMSG   \n")
            assert read_line(alice) == "ERROR neplatny prikaz MSG\n"
            assert read_line(alice) == "ERROR neplatny prikaz MSG\n"
            alice.sendall(b"MS")
            assert not select.select([bob, carol], [], [], 0.05)[0]
            alice.sendall("G Ahoj světe, dvě slova!\r\nWHERE\n".encode())
            for peer in (bob, carol):
                assert read_line(peer) == "Alice: Ahoj světe, dvě slova!\n"
            assert read_line(alice) == f"POS {ax} {ay}\n"
            assert not select.select([alice], [], [], 0.05)[0], "Chat echoed to sender"

            direction = "EAST" if bx < 18 else "WEST"
            bob.sendall(f"MOVE {direction}\n".encode())
            bx += 1 if direction == "EAST" else -1
            assert read_area(bob) == (bx, by)
            for peer in (alice, carol):
                assert read_line(peer) == f"POS Bob {bx} {by}\n"

            server.stdin.write(b"Zprava ")
            server.stdin.flush()
            assert not select.select([alice, bob, carol], [], [], 0.05)[0]
            server.stdin.write(b"serveru\r\n")
            server.stdin.flush()
            for peer in (alice, bob, carol):
                assert read_line(peer) == "Server: Zprava serveru\n"

            # Both workers must reap only their own INFO helper.
            for peer in (alice, bob):
                peer.sendall(b"INFO lion\n")
            for peer in (alice, bob):
                info = "".join(read_line(peer) for _ in range(5))
                assert "Zvíře: lev\n" in info and "Potrava: masožravec\n" in info

            client = subprocess.Popen(
                [str(ROOT / "zoo_client"), "127.0.0.1", str(port)],
                cwd=ROOT, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            assert client.stdin is not None
            try:
                # One pipe write must preserve both nickname and subsequent commands.
                client_name = "C" * 32
                client.stdin.write(f"{client_name}\r\nMSG real client\n".encode())
                client.stdin.flush()
                for peer in (alice, bob, carol):
                    assert re.fullmatch(rf"POS {client_name} \d+ \d+\n", read_line(peer))
                    assert read_line(peer) == f"{client_name}: real client\n"
                long_message = "Server: " + "x" * 2500 + "\n"
                server.stdin.write(("x" * 2500 + "\n").encode())
                server.stdin.flush()
                for peer in (alice, bob, carol):
                    assert read_line(peer) == long_message
                output, errors = client.communicate(b"WHERE\nQUIT\n", timeout=5)
                assert client.returncode == 0, errors.decode()
                assert long_message.encode() in output, "Client lost a split TCP message"
                assert f"WELCOME {client_name}".encode() in output
                assert f"BYE {client_name}\n".encode() in output
                assert b"ERROR" not in output
                assert f"{client_name}: real client".encode() not in output
                assert output.count(b"Mapa ZOO") == 2, "Client lost its AREA frame"
                assert b"@" in output
            finally:
                if client.poll() is None:
                    client.kill()
                    client.wait()

            # EOF on the console sends its final line and leaves accept() active.
            server.stdin.write(b"posledni zprava")
            server.stdin.close()
            server.stdin = None
            for peer in (alice, bob, carol):
                assert read_line(peer) == "Server: posledni zprava\n"
            carol.close()  # Abrupt disconnect must not close the listen socket.
            quit_peer(alice, "Alice")
            quit_peer(bob, "Bob")

            fresh = connect()
            hello(fresh, "Fresh")
            fresh.sendall(b"WHERE\n")
            reply = read_line(fresh)
            assert re.fullmatch(r"POS \d+ \d+\n", reply), reply
            fresh.sendall(b"x" * 4097 + b"\n")
            assert read_line(fresh) == "ERROR prikaz je prilis dlouhy\n"
            assert fresh.recv(1) == b""

            idle = connect()
            idle.settimeout(12)
            assert read_line(idle) == "TIMEOUT\n"
            assert idle.recv(1) == b""
            final = connect()
            hello(final, "Final")
            quit_peer(final, "Final")
            assert server.poll() is None, "Server stopped after client disconnect"
        finally:
            server.terminate()
            server.wait(timeout=5)
            server.stdout.close()
            if server.stdin is not None:
                server.stdin.close()
        log.seek(0)
        messages = log.read().decode()
        assert "Alice: Ahoj světe, dvě slova!\n" in messages
        assert "Server: Zprava serveru\n" in messages

    print("PASS: threads, MSG, positions, console, INFO, client framing, disconnects, timeout")


if __name__ == "__main__":
    main()
