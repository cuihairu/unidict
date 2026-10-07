#!/usr/bin/env python3
"""C++ 版 relay（unidict-relay）契约符合性跑批。

自由端口起 unidict-relay 子进程，等监听就绪后把 dev 契约符合性套件
（dev/test_relay_protocol.py 全量用例）经 UNIDICT_RELAY_EXTERNAL_BASE
打到该进程——两种实现跑同一套断言（PROTOCOL.md §4 验收口径）。

用法：python3 run_conformance.py /path/to/unidict-relay
"""
import os
import socket
import subprocess
import sys
import threading
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "dev"))

import test_relay_protocol  # noqa: E402


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def main():
    if len(sys.argv) != 2:
        print("用法: run_conformance.py <unidict-relay 路径>", file=sys.stderr)
        return 2
    binary = sys.argv[1]
    port = free_port()
    proc = subprocess.Popen(
        [binary, "--host", "127.0.0.1", "--port", str(port)],
        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)

    # 等监听行（stderr），15s 不就绪视为启动失败
    ready = threading.Event()

    def wait_listening():
        for line in proc.stderr:
            if b"listening on" in line:
                break
        ready.set()

    threading.Thread(target=wait_listening, daemon=True).start()
    if not ready.wait(15):
        proc.kill()
        print("unidict-relay 15s 内未就绪", file=sys.stderr)
        return 2

    os.environ["UNIDICT_RELAY_EXTERNAL_BASE"] = f"http://127.0.0.1:{port}"
    try:
        suite = unittest.defaultTestLoader.loadTestsFromModule(test_relay_protocol)
        result = unittest.TextTestRunner(verbosity=2).run(suite)
        return 0 if result.wasSuccessful() else 1
    finally:
        os.environ.pop("UNIDICT_RELAY_EXTERNAL_BASE", None)
        proc.terminate()
        try:
            proc.wait(5)
        except subprocess.TimeoutExpired:
            proc.kill()


if __name__ == "__main__":
    sys.exit(main())
