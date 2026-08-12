#!/usr/bin/env python3
"""
Tier 5 Adversarial Edge Case Test Harness for TCP Client CLI.
Focuses on interactive prompts, real-time I/O, quit/exit variants, STDIN EOF, timeouts, and signal handling.
"""

import os
import sys
import time
import socket
import signal
import subprocess
import hashlib
from typing import Tuple

BINARY_PATH = os.path.abspath("./tcp-client")
MOCK_SERVER_SCRIPT = os.path.join(os.path.dirname(__file__), "mock_server.py")


def start_mock_server(mode: str = "echo", delay_ms: int = 2000, disconnect_after: int = 0) -> Tuple[subprocess.Popen, int]:
    cmd = [
        sys.executable, MOCK_SERVER_SCRIPT,
        "--host", "127.0.0.1",
        "--port", "0",
        "--mode", mode,
        "--delay-ms", str(delay_ms),
        "--disconnect-after", str(disconnect_after),
        "--quiet"
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
    port = None
    start_time = time.time()
    while time.time() - start_time < 5.0:
        line = proc.stdout.readline()
        if line.startswith("PORT:"):
            port = int(line.split(":")[1].strip())
            break
        if proc.poll() is not None:
            break
    if port is None:
        stderr_out = proc.stderr.read()
        proc.kill()
        raise RuntimeError(f"Failed to start mock server: {stderr_out}")
    return proc, port

def stop_mock_server(proc: subprocess.Popen):
    if proc:
        proc.terminate()
        try:
            proc.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            proc.kill()

def run_client(args, input_data: bytes = None, timeout: float = 5.0) -> Tuple[int, str, str]:
    cmd = [BINARY_PATH] + args
    try:
        proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE if input_data is not None else None,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=False
        )
        stdout_bytes, stderr_bytes = proc.communicate(input=input_data, timeout=timeout)
        return proc.returncode, stdout_bytes.decode('utf-8', errors='replace'), stderr_bytes.decode('utf-8', errors='replace')
    except subprocess.TimeoutExpired:
        proc.kill()
        return (-2, "", "Timeout expired")
    except Exception as e:
        return (-3, "", str(e))

def run_adversarial_suite():
    tests = []
    
    # 1. Exit / Quit variants in Interactive mode
    def test_exit_cmd_lowercase():
        srv, port = start_mock_server("multiline")
        try:
            code, out, err = run_client(["127.0.0.1", str(port), "-i"], input_data=b"hello\nexit\n")
            assert code == 0, f"Expected code 0, got {code}"
            assert "ACK: hello" in out, f"Expected 'ACK: hello', got {out}"
            assert "Goodbye!" in out, f"Expected 'Goodbye!', got {out}"
            assert "> " in out, f"Expected prompt '> ', got {out}"
        finally:
            stop_mock_server(srv)
    tests.append(("ADV-01", "Interactive exit command (lowercase)", test_exit_cmd_lowercase))

    def test_exit_cmd_uppercase():
        srv, port = start_mock_server("multiline")
        try:
            code, out, err = run_client(["127.0.0.1", str(port), "-i"], input_data=b"hello\nQUIT\n")
            assert code == 0, f"Expected code 0, got {code}"
            assert "Goodbye!" in out, f"Expected 'Goodbye!', got {out}"
        finally:
            stop_mock_server(srv)
    tests.append(("ADV-02", "Interactive QUIT command (uppercase)", test_exit_cmd_uppercase))

    def test_exit_cmd_padded_spaces():
        srv, port = start_mock_server("multiline")
        try:
            code, out, err = run_client(["127.0.0.1", str(port), "-i"], input_data=b"hello\n  ExIt  \n")
            assert code == 0, f"Expected code 0, got {code}"
            assert "Goodbye!" in out, f"Expected 'Goodbye!', got {out}"
        finally:
            stop_mock_server(srv)
    tests.append(("ADV-03", "Interactive exit command with spaces & mixed case", test_exit_cmd_padded_spaces))

    def test_multi_turn_interactive():
        srv, port = start_mock_server("multiline")
        try:
            code, out, err = run_client(["127.0.0.1", str(port), "-i"], input_data=b"msg1\nmsg2\nmsg3\nquit\n")
            assert code == 0, f"Expected code 0, got {code}"
            assert "ACK: msg1" in out
            assert "ACK: msg2" in out
            assert "ACK: msg3" in out
            assert "Goodbye!" in out
        finally:
            stop_mock_server(srv)
    tests.append(("ADV-04", "Multi-turn interactive conversation", test_multi_turn_interactive))

    # 2. STDIN EOF Handling
    def test_stdin_eof_interactive():
        srv, port = start_mock_server("echo")
        try:
            # EOF without exit command
            code, out, err = run_client(["127.0.0.1", str(port), "-i"], input_data=b"hello stream\n")
            assert code == 0, f"Expected code 0 on STDIN EOF, got {code}"
            assert "hello stream" in out
        finally:
            stop_mock_server(srv)
    tests.append(("ADV-05", "STDIN EOF in interactive mode without explicit exit", test_stdin_eof_interactive))

    def test_empty_stdin_interactive():
        srv, port = start_mock_server("echo")
        try:
            code, out, err = run_client(["127.0.0.1", str(port), "-i"], input_data=b"")
            assert code == 0, f"Expected code 0 on empty STDIN, got {code}"
        finally:
            stop_mock_server(srv)
    tests.append(("ADV-06", "Empty STDIN in interactive mode", test_empty_stdin_interactive))

    def test_partial_line_eof():
        srv, port = start_mock_server("echo")
        try:
            code, out, err = run_client(["127.0.0.1", str(port), "-i"], input_data=b"no newline EOF")
            assert code == 0, f"Expected code 0 on partial line EOF, got {code}"
            assert "no newline EOF" in out
        finally:
            stop_mock_server(srv)
    tests.append(("ADV-07", "Partial line before STDIN EOF", test_partial_line_eof))

    # 3. Timeout handling
    def test_instant_timeout():
        srv, port = start_mock_server("delayed", delay_ms=3000)
        try:
            code, out, err = run_client(["127.0.0.1", str(port), "-t", "1"], input_data=b"fast\n")
            assert code == 4, f"Expected code 4 for 1ms timeout, got {code}"
        finally:
            stop_mock_server(srv)
    tests.append(("ADV-08", "Instant connection/read timeout (1ms)", test_instant_timeout))

    def test_oneshot_read_timeout():
        srv, port = start_mock_server("delayed", delay_ms=3000)
        try:
            code, out, err = run_client(["127.0.0.1", str(port), "-t", "500"], input_data=b"test oneshot timeout\n")
            assert code == 4, f"Expected code 4 for oneshot read timeout, got {code}"
        finally:
            stop_mock_server(srv)
    tests.append(("ADV-09", "One-shot read timeout (500ms)", test_oneshot_read_timeout))

    def test_nonroutable_ip_timeout():
        code, out, err = run_client(["10.255.255.1", "8080", "-t", "300"])
        assert code == 4, f"Expected code 4 for non-routable IP timeout, got {code}"
    tests.append(("ADV-10", "Non-routable IP connection timeout", test_nonroutable_ip_timeout))

    # 4. Signal & Error handling
    def test_immediate_disconnect():
        srv, port = start_mock_server("disconnect", disconnect_after=0)
        try:
            code, out, err = run_client(["127.0.0.1", str(port)], input_data=b"Immediate disconnect\n")
            assert code == 5, f"Expected exit code 5 for disconnect, got {code}"
        finally:
            stop_mock_server(srv)
    tests.append(("ADV-11", "Immediate server disconnect (0 bytes)", test_immediate_disconnect))

    def test_sigpipe_suppression():
        srv, port = start_mock_server("disconnect", disconnect_after=5)
        try:
            # Pipelined data larger than 5 bytes to trigger write error on closed socket
            code, out, err = run_client(["127.0.0.1", str(port)], input_data=b"Long message causing SIGPIPE attempt\n")
            # Must exit with code 5 (Network socket I/O error), NOT crash via SIGPIPE (which produces code 141 / negative)
            assert code == 5, f"Expected exit code 5 (graceful error), got {code}"
        finally:
            stop_mock_server(srv)
    tests.append(("ADV-12", "SIGPIPE signal suppression on broken pipe", test_sigpipe_suppression))

    # 5. High-Throughput & Long Input Line Streaming
    def test_high_throughput_streaming():
        srv, port = start_mock_server("echo")
        try:
            chunk = b"Z" * (8 * 1024 * 1024)
            expected_md5 = hashlib.md5(chunk).hexdigest()
            code, out, err = run_client(["127.0.0.1", str(port)], input_data=chunk, timeout=15.0)
            assert code == 0, f"Expected exit code 0, got {code}"
            received_md5 = hashlib.md5(out.encode('latin1', errors='replace')).hexdigest()
            assert received_md5 == expected_md5, f"8MB payload checksum mismatch"
        finally:
            stop_mock_server(srv)
    tests.append(("ADV-13", "High-Throughput 8MB Stream Payload", test_high_throughput_streaming))

    def test_interactive_long_line():
        srv, port = start_mock_server("echo")
        try:
            payload = b"A" * 5000 + b"\nexit\n"
            code, out, err = run_client(["127.0.0.1", str(port), "-i"], input_data=payload, timeout=5.0)
            assert code == 0, f"Expected exit code 0, got {code}"
            a_count = out.count("A")
            assert a_count == 5000, f"Expected 5000 'A's in interactive mode response, got {a_count}"
        finally:
            stop_mock_server(srv)
    tests.append(("ADV-14", "Interactive long line (>4096 bytes)", test_interactive_long_line))

    print("======================================================================")
    print("TIER 5 ADVERSARIAL EDGE CASE SUITE")
    print("======================================================================")

    passed = 0
    failed = 0
    for test_id, name, fn in tests:
        try:
            fn()
            print(f"  [PASS] [{test_id}] {name}")
            passed += 1
        except AssertionError as e:
            print(f"  [FAIL] [{test_id}] {name}: {e}")
            failed += 1
        except Exception as e:
            print(f"  [ERROR] [{test_id}] {name}: {e}")
            failed += 1

    print("----------------------------------------------------------------------")
    print(f"Adversarial Suite Summary: {passed}/{len(tests)} Passed ({failed} Failed)")
    print("----------------------------------------------------------------------")
    return failed == 0

if __name__ == "__main__":
    success = run_adversarial_suite()
    sys.exit(0 if success else 1)
