#!/usr/bin/env python3
"""Configure the ESP32-C3 master over the existing UDP RPC envelope."""
import argparse
import getpass
import json
import socket
import time
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("discover", "scan", "setup", "cloud"))
    parser.add_argument("--host", default="192.168.4.1")
    parser.add_argument("--port", type=int, default=8181)
    parser.add_argument("--ssid")
    parser.add_argument("--cloud-host")
    parser.add_argument("--cloud-port", type=int, default=1883)
    parser.add_argument("--user", help="Gateway device_uid")
    parser.add_argument("--open", action="store_true", help="Use an open Wi-Fi network")
    args = parser.parse_args()
    data = {}
    command = {"discover": "SCAN_HC", "scan": "HC_SCAN_WIFI", "setup": "setupHC", "cloud": "setupCloud"}[args.action]
    if args.action == "setup":
        if not args.ssid:
            parser.error("setup requires --ssid")
        data = {"ssid": args.ssid, "pass": "" if args.open else getpass.getpass("Wi-Fi password: ")}
        if not 1 <= len(args.ssid.encode()) <= 32 or "\0" in args.ssid:
            parser.error("SSID must contain 1-32 bytes and no NUL")
        password = data["pass"]
        if "\0" in password or (password and not 8 <= len(password.encode()) <= 63):
            parser.error("Wi-Fi password must contain 8-63 bytes, or use --open")
    if args.action == "cloud":
        if not args.cloud_host or not args.user:
            parser.error("cloud requires --cloud-host and --user")
        token = getpass.getpass("Ingest token: ")
        if (not 1 <= len(args.cloud_host.encode()) <= 253 or
                not 1 <= len(args.user.encode()) <= 128 or
                not 1 <= len(token.encode()) <= 255 or
                not 1 <= args.cloud_port <= 65535 or
                any("\0" in value for value in (args.cloud_host, args.user, token)) or
                any(char in args.user for char in "/+#")):
            parser.error("Invalid cloud host, user, token or port")
        data = {"host": args.cloud_host, "user": args.user, "pass": token, "port": args.cloud_port}
    request_id = str(uuid.uuid4())
    packet = json.dumps({"cmd": command, "rqi": request_id, "data": data}).encode()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.sendto(packet, (args.host, args.port))
        deadline = time.monotonic() + (25 if args.action == "setup" else 20)
        while time.monotonic() < deadline:
            sock.settimeout(max(0.01, deadline - time.monotonic()))
            try:
                payload, peer = sock.recvfrom(65535)
            except socket.timeout:
                break
            try:
                response = json.loads(payload)
            except (ValueError, UnicodeDecodeError):
                continue
            if not isinstance(response, dict) or response.get("rqi") != request_id:
                continue
            print(f"{peer[0]}:{peer[1]} {json.dumps(response, ensure_ascii=False)}")
            result = response.get("data", {})
            if args.action != "setup":
                if args.host == "255.255.255.255":
                    continue
                return 0 if result.get("code") == 0 else 1
            if result.get("status") != "connecting":
                return 0 if result.get("status") == "connected" and result.get("code") == 0 else 1
        print("No final reply received. UDP packets may be lost; reconnect and discover before retrying setup.")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
