import concurrent.futures
import os
import pathlib
import select
import signal
import socket
import subprocess
import threading
import time
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
JIDLA = {
    "Smazeny syr", "Kureci rizek s hranolky", "Spagety Bolognese",
    "Zeleninovy salat", "Hamburger", "Hot dog", "Palacinky", "Ovocny pohar",
}


def read_line(connection):
    line = bytearray()
    while True:
        character = connection.recv(1)
        if not character:
            return bytes(line)
        line += character
        if character == b"\n":
            return bytes(line)


class RestaurantTests(unittest.TestCase):
    def setUp(self):
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            self.port = reservation.getsockname()[1]
        self.server = self.start_server(self.port)

    def start_server(self, port):
        server = subprocess.Popen(
            [str(ROOT / "server"), str(port), "-d"],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
        self.addCleanup(self.stop_server, server)
        self.assertTrue(select.select([server.stdout], [], [], 3)[0])
        self.assertIn(b"nasloucha", server.stdout.readline())
        return server

    def stop_server(self, server):
        if server.poll() is None:
            server.terminate()
        try:
            server.wait(timeout=3)
        except subprocess.TimeoutExpired:
            os.killpg(server.pid, signal.SIGKILL)
            server.wait(timeout=3)
            raise
        finally:
            server.stdout.close()

    def connect(self, role=None, port=None):
        connection = socket.create_connection(
            ("127.0.0.1", self.port if port is None else port), 3
        )
        self.addCleanup(connection.close)
        self.assertEqual(read_line(connection), b"Role?\n")
        if role is not None:
            connection.sendall(role.encode() + b"\n")
        return connection

    def children(self):
        path = pathlib.Path(
            f"/proc/{self.server.pid}/task/{self.server.pid}/children"
        )
        return [int(pid) for pid in path.read_text().split()]

    def wait_until(self, predicate):
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            if predicate():
                return
            time.sleep(0.02)
        self.fail("Timed out waiting for server processes")

    def ipc_paths(self):
        prefix = f"osy_restaurant_{os.getuid()}_{self.port}"
        return [pathlib.Path(f"/dev/shm/{prefix}_buffer")] + [
            pathlib.Path(f"/dev/shm/sem.{prefix}_{name}")
            for name in ("empty", "mutex", "full")
        ]

    def assert_waiting(self, connection, timeout=0.15):
        connection.settimeout(timeout)
        try:
            with self.assertRaises(socket.timeout):
                connection.recv(1)
        finally:
            connection.settimeout(3)

    def test_prompt_and_invalid_role(self):
        connection = self.connect()
        connection.sendall(b"spatna-role\n")
        self.assertTrue(read_line(connection).startswith(b"ERROR"))
        self.assertEqual(connection.recv(1), b"")

    def test_fifo_owned_strings_fragmented_and_combined_lines(self):
        cook = self.connect()
        cook.sendall(b"ku")
        cook.sendall(b"char\r\nSmazeny ")
        cook.sendall(b"syr\r\nHot dog\nPalacinky\nQUIT\n")
        for _ in range(3):
            self.assertEqual(read_line(cook), b"OK\n")
        self.assertEqual(cook.recv(1), b"")

        visitor = self.connect("navstevnik")
        for dish in (b"Smazeny syr", b"Hot dog", b"Palacinky"):
            self.assertEqual(read_line(visitor), dish + b"\n")
            visitor.sendall(b"OK\r\n")

    def test_full_counter_blocks_until_one_meal_is_taken(self):
        cook = self.connect("kuchar")
        cook.sendall(b"".join(f"jidlo {i}\n".encode() for i in range(6)))
        for _ in range(5):
            self.assertEqual(read_line(cook), b"OK\n")
        self.assert_waiting(cook)

        visitor = self.connect("navstevnik")
        self.assertEqual(read_line(visitor), b"jidlo 0\n")
        self.assertEqual(read_line(cook), b"OK\n")
        cook.sendall(b"QUIT\n")
        self.assertEqual(cook.recv(1), b"")
        for i in range(1, 6):
            visitor.sendall(b"OK\n")
            self.assertEqual(read_line(visitor), f"jidlo {i}\n".encode())
        visitor.sendall(b"OK\n")

    def test_empty_counter_waits_without_idle_timeout(self):
        visitor = self.connect("navstevnik")
        self.assert_waiting(visitor, 3.2)
        cook = self.connect("kuchar")
        cook.sendall(b"Hamburger\nQUIT\n")
        self.assertEqual(read_line(cook), b"OK\n")
        self.assertEqual(read_line(visitor), b"Hamburger\n")
        visitor.sendall(b"OK\n")

    def test_visitor_acknowledges_before_receiving_next_meal(self):
        cook = self.connect("kuchar")
        cook.sendall(b"Hot dog\nPalacinky\nQUIT\n")
        self.assertEqual(read_line(cook), b"OK\n")
        self.assertEqual(read_line(cook), b"OK\n")
        visitor = self.connect("navstevnik")
        self.assertEqual(read_line(visitor), b"Hot dog\n")
        self.assert_waiting(visitor)
        visitor.sendall(b"OK\n")
        self.assertEqual(read_line(visitor), b"Palacinky\n")
        visitor.sendall(b"OK\n")

    def test_line_length_limit(self):
        cook = self.connect("kuchar")
        cook.sendall(b"x" * 256 + b"\nQUIT\n")
        self.assertEqual(read_line(cook), b"OK\n")
        self.assertEqual(cook.recv(1), b"")
        visitor = self.connect("navstevnik")
        self.assertEqual(read_line(visitor), b"x" * 256 + b"\n")
        visitor.sendall(b"OK\n")
        invalid = self.connect("kuchar")
        invalid.sendall(b"x" * 257 + b"\n")
        self.assertTrue(read_line(invalid).startswith(b"ERROR"))
        self.assertEqual(invalid.recv(1), b"")

    def test_tcp_line_length_limit(self):
        invalid = self.connect("kuchar")
        invalid.sendall(b"x" * 4097)
        self.assertEqual(invalid.recv(1), b"")

    def test_embedded_null_is_rejected(self):
        invalid = self.connect("kuchar")
        invalid.sendall(b"Hot\0dog\n")
        self.assertTrue(read_line(invalid).startswith(b"ERROR"))
        self.assertEqual(invalid.recv(1), b"")

    def test_named_ipc_and_one_process_per_connection(self):
        connections = [self.connect() for _ in range(3)]
        self.wait_until(lambda: len(self.children()) == 3)
        self.assertTrue(all(path.exists() for path in self.ipc_paths()))
        for child in self.children():
            maps = pathlib.Path(f"/proc/{child}/maps").read_text()
            self.assertIn(str(self.ipc_paths()[0]), maps)
            self.assertEqual(len(list(pathlib.Path(f"/proc/{child}/task").iterdir())), 1)
        for connection in connections:
            connection.close()
        self.wait_until(lambda: not self.children())

    def test_shutdown_reaps_blocked_workers_and_removes_ipc(self):
        cook = self.connect("kuchar")
        cook.sendall(b"".join(f"jidlo {i}\n".encode() for i in range(6)))
        for _ in range(5):
            self.assertEqual(read_line(cook), b"OK\n")
        self.assert_waiting(cook)
        self.connect()
        self.wait_until(lambda: len(self.children()) == 2)
        children = self.children()

        self.stop_server(self.server)
        self.assertEqual(self.server.returncode, 0)
        self.assertEqual(cook.recv(1), b"")
        self.assertTrue(all(not pathlib.Path(f"/proc/{pid}").exists()
                            for pid in children))
        self.assertTrue(all(not path.exists() for path in self.ipc_paths()))

        self.server = self.start_server(self.port)
        visitor = self.connect("navstevnik")
        self.assert_waiting(visitor)
        new_cook = self.connect("kuchar")
        new_cook.sendall(b"Hot dog\nQUIT\n")
        self.assertEqual(read_line(new_cook), b"OK\n")
        self.assertEqual(read_line(visitor), b"Hot dog\n")
        visitor.sendall(b"OK\n")

    def test_restart_after_crash_resets_stale_full_queue(self):
        cook = self.connect("kuchar")
        cook.sendall(b"stare jidlo\n" * 5 + b"QUIT\n")
        for _ in range(5):
            self.assertEqual(read_line(cook), b"OK\n")
        self.assertEqual(cook.recv(1), b"")
        self.wait_until(lambda: not self.children())

        self.server.kill()
        self.stop_server(self.server)
        self.assertTrue(all(path.exists() for path in self.ipc_paths()))

        self.server = self.start_server(self.port)
        visitor = self.connect("navstevnik")
        self.assert_waiting(visitor)
        new_cook = self.connect("kuchar")
        new_cook.sendall(b"nove jidlo\nQUIT\n")
        self.assertEqual(read_line(new_cook), b"OK\n")
        self.assertEqual(read_line(visitor), b"nove jidlo\n")
        visitor.sendall(b"OK\n")

    def test_duplicate_server_does_not_reset_running_queue(self):
        cook = self.connect("kuchar")
        cook.sendall(b"Palacinky\nQUIT\n")
        self.assertEqual(read_line(cook), b"OK\n")
        duplicate = subprocess.run(
            [str(ROOT / "server"), str(self.port)],
            capture_output=True, text=True, timeout=3,
        )
        self.assertNotEqual(duplicate.returncode, 0)
        self.assertIn("bind/listen", duplicate.stderr)
        visitor = self.connect("navstevnik")
        self.assertEqual(read_line(visitor), b"Palacinky\n")
        visitor.sendall(b"OK\n")

    def test_servers_on_different_ports_have_separate_queues(self):
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            other_port = reservation.getsockname()[1]
        self.start_server(other_port)
        for port, dish in ((self.port, b"Hot dog"), (other_port, b"Palacinky")):
            cook = self.connect("kuchar", port)
            cook.sendall(dish + b"\nQUIT\n")
            self.assertEqual(read_line(cook), b"OK\n")
        for port, dish in ((self.port, b"Hot dog"), (other_port, b"Palacinky")):
            visitor = self.connect("navstevnik", port)
            self.assertEqual(read_line(visitor), dish + b"\n")
            visitor.sendall(b"OK\n")

    def test_multiple_cooks_and_visitors_share_the_queue(self):
        visitors = [self.connect("navstevnik") for _ in range(3)]
        cooks = [self.connect("kuchar") for _ in range(4)]
        received = []
        lock = threading.Lock()
        complete = threading.Event()
        expected = {f"kuchar {cook}, jidlo {i}\n".encode()
                    for cook in range(4) for i in range(20)}

        def receive(connection):
            while not complete.is_set():
                try:
                    dish = read_line(connection)
                except socket.timeout:
                    if complete.is_set():
                        return
                    raise
                if not dish:
                    return
                connection.sendall(b"OK\n")
                with lock:
                    received.append(dish)
                    if len(received) == len(expected):
                        complete.set()

        def produce(number, connection):
            for i in range(20):
                connection.sendall(f"kuchar {number}, jidlo {i}\n".encode())
                self.assertEqual(read_line(connection), b"OK\n")
            connection.sendall(b"QUIT\n")
            self.assertEqual(connection.recv(1), b"")

        with concurrent.futures.ThreadPoolExecutor(max_workers=7) as pool:
            readers = [pool.submit(receive, visitor) for visitor in visitors]
            writers = [pool.submit(produce, i, cook)
                       for i, cook in enumerate(cooks)]
            for writer in writers:
                writer.result(timeout=10)
            self.assertTrue(complete.wait(5))
            for visitor in visitors:
                visitor.shutdown(socket.SHUT_RDWR)
            for reader in readers:
                reader.result(timeout=5)
        self.assertEqual(len(received), len(expected))
        self.assertEqual(set(received), expected)

    def test_real_cook_client_sends_meals_and_quit(self):
        client = subprocess.run(
            [str(ROOT / "client"), "localhost", str(self.port)],
            input="kuchar\n5\n", capture_output=True, text=True, timeout=5,
        )
        self.assertEqual(client.returncode, 0, client.stderr)
        self.assertEqual(client.stderr, "")
        visitor = self.connect("navstevnik")
        for _ in range(5):
            self.assertIn(read_line(visitor).decode().strip(), JIDLA)
            visitor.sendall(b"OK\n")


class ClientProtocolTests(unittest.TestCase):
    def test_cook_waits_for_each_ack_and_quit_has_no_null_byte(self):
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen()
            listener.settimeout(3)
            with subprocess.Popen(
                [str(ROOT / "client"), "127.0.0.1",
                 str(listener.getsockname()[1])],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, text=True,
            ) as client:
                client.stdin.write("kuchar\n2\n")
                client.stdin.flush()
                connection, _ = listener.accept()
                with connection:
                    connection.settimeout(3)
                    connection.sendall(b"Ro")
                    connection.sendall(b"le?\n")
                    self.assertEqual(read_line(connection), b"kuchar\n")
                    for _ in range(2):
                        self.assertIn(read_line(connection).decode().strip(), JIDLA)
                        connection.settimeout(0.15)
                        with self.assertRaises(socket.timeout):
                            connection.recv(1)
                        connection.settimeout(3)
                        connection.sendall(b"O")
                        connection.sendall(b"K\n")
                    self.assertEqual(read_line(connection), b"QUIT\n")
                    self.assertEqual(connection.recv(1), b"")
                _, errors = client.communicate(timeout=3)
                self.assertEqual(client.returncode, 0, errors)

    def test_visitor_client_prints_and_acknowledges_every_meal(self):
        with socket.socket() as listener:
            listener.bind(("127.0.0.1", 0))
            listener.listen()
            listener.settimeout(3)
            with subprocess.Popen(
                [str(ROOT / "client"), "127.0.0.1",
                 str(listener.getsockname()[1])],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, text=True,
            ) as client:
                client.stdin.write("navstevnik\n")
                client.stdin.flush()
                connection, _ = listener.accept()
                with connection:
                    connection.settimeout(3)
                    connection.sendall(b"Role?\n")
                    self.assertEqual(read_line(connection), b"navstevnik\n")
                    connection.sendall(b"Smazeny ")
                    connection.sendall(b"syr\nHot dog\r\n")
                    self.assertEqual(read_line(connection), b"OK\n")
                    self.assertEqual(read_line(connection), b"OK\n")
                output, errors = client.communicate(timeout=3)
                self.assertEqual(client.returncode, 0, errors)
                self.assertEqual(output.splitlines(), ["Role?", "Smazeny syr", "Hot dog"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
