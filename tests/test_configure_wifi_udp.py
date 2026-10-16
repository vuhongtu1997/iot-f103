import contextlib
import importlib.util
import io
import json
from pathlib import Path
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location(
    "wifi_udp", Path(__file__).resolve().parents[1] / "tools/configure_wifi_udp.py")
client = importlib.util.module_from_spec(spec)
spec.loader.exec_module(client)


class FakeSocket:
    def __init__(self, replies):
        self.replies = list(replies)
        self.sent = None

    def __enter__(self): return self
    def __exit__(self, *args): pass
    def setsockopt(self, *args): pass
    def settimeout(self, *args): pass

    def sendto(self, payload, address):
        self.sent = json.loads(payload)
        self.destination = address

    def recvfrom(self, size):
        if not self.replies:
            raise client.socket.timeout()
        value = self.replies.pop(0)
        if isinstance(value, bytes):
            return value, ("192.168.4.1", 8181)
        response = {"rqi": self.sent["rqi"], "data": value}
        return json.dumps(response).encode(), ("192.168.4.1", 8181)


class WifiUdpClient(unittest.TestCase):
    def run_client(self, argv, replies):
        sock = FakeSocket(replies)
        output = io.StringIO()
        with patch("sys.argv", ["wifi_udp", *argv]), \
             patch.object(client.socket, "socket", return_value=sock), \
             patch.object(client.getpass, "getpass", return_value="secret-password"), \
             contextlib.redirect_stdout(output):
            result = client.main()
        self.assertNotIn("secret-password", output.getvalue())
        return result, sock

    def test_discovery_ignores_malformed_and_unrelated_replies(self):
        result, sock = self.run_client(["discover"], [
            b"not json", b'{"rqi":"unrelated","data":{"code":0}}', {"code": 0}])
        self.assertEqual(result, 0)
        self.assertEqual(sock.sent["cmd"], "SCAN_HC")
        self.assertEqual(sock.destination, ("192.168.4.1", 8181))

    def test_setup_waits_for_connected(self):
        result, sock = self.run_client(["setup", "--ssid", "Test"], [
            {"code": 0, "status": "connecting"}, {"code": 0, "status": "connected"}])
        self.assertEqual(result, 0)
        self.assertEqual(sock.sent["data"], {"ssid": "Test", "pass": "secret-password"})

    def test_connecting_is_not_success(self):
        result, _ = self.run_client(["setup", "--ssid", "Test"], [
            {"code": 0, "status": "connecting"}])
        self.assertEqual(result, 1)

    def test_connection_failure(self):
        result, _ = self.run_client(["setup", "--ssid", "Test"], [
            {"code": 0, "status": "connecting"}, {"code": -1, "status": "connection_failed"}])
        self.assertEqual(result, 1)

    def test_open_network(self):
        result, sock = self.run_client(["setup", "--ssid", "Test", "--open"], [
            {"code": 0, "status": "connected"}])
        self.assertEqual(result, 0)
        self.assertEqual(sock.sent["data"]["pass"], "")


if __name__ == "__main__":
    unittest.main()
