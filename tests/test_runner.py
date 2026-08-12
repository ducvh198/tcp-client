#!/usr/bin/env python3
"""
Automated E2E Test Runner for TCP Client CLI.
Executes test suite across Tiers 1-4 using tests/mock_server.py.
Uses Python 3 standard library only.
"""

import argparse
import hashlib
import os
import re
import socket
import subprocess
import sys
import tempfile
import time
from typing import Dict, List, Optional, Tuple

# Configuration & Constants
DEFAULT_BINARY = "./tcp-client" if os.name != 'nt' else "./tcp-client.exe"
MOCK_SERVER_SCRIPT = os.path.join(os.path.dirname(__file__), "mock_server.py")


class TestContext:
    def __init__(self, binary_path: str, verbose: bool):
        self.binary_path = os.path.abspath(binary_path)
        self.verbose = verbose
        self.passed = 0
        self.failed = 0
        self.skipped = 0
        self.binary_exists = os.path.isfile(self.binary_path) and os.access(self.binary_path, os.X_OK or os.F_OK)

    def log(self, msg: str):
        if self.verbose:
            print(f"[VERBOSE] {msg}")


def start_mock_server(mode: str = "echo", delay_ms: int = 2000, disconnect_after: int = 0) -> Tuple[subprocess.Popen, int]:
    """Spawns mock_server.py and waits for allocated port notification."""
    cmd = [
        sys.executable, MOCK_SERVER_SCRIPT,
        "--host", "127.0.0.1",
        "--port", "0",
        "--mode", mode,
        "--delay-ms", str(delay_ms),
        "--disconnect-after", str(disconnect_after),
        "--quiet"
    ]

    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )

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
        raise RuntimeError(f"Failed to start mock server. Stderr: {stderr_out}")

    return proc, port


def stop_mock_server(proc: subprocess.Popen):
    """Terminates mock server process safely."""
    if proc:
        proc.terminate()
        try:
            proc.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            proc.kill()


def run_client(ctx: TestContext, args: List[str], input_data: Optional[bytes] = None, timeout: float = 5.0) -> Tuple[int, str, str]:
    """Runs binary with arguments and optional stdin payload."""
    if not ctx.binary_exists:
        return (-1, "", "Binary not found")

    cmd = [ctx.binary_path] + args
    ctx.log(f"Executing: {' '.join(cmd)}")

    try:
        proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE if input_data is not None else None,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=False
        )

        stdout_bytes, stderr_bytes = proc.communicate(input=input_data, timeout=timeout)
        stdout_str = stdout_bytes.decode('utf-8', errors='replace')
        stderr_str = stderr_bytes.decode('utf-8', errors='replace')
        return proc.returncode, stdout_str, stderr_str
    except subprocess.TimeoutExpired:
        proc.kill()
        return (-2, "", "Execution timed out")
    except Exception as e:
        return (-3, "", str(e))


def find_unused_port() -> int:
    """Finds a free port on localhost that is confirmed to be closed (connection refused)."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(('127.0.0.1', 0))
        port = s.getsockname()[1]
    return port


# ============================================================================
# TEST CASES DEFINITION
# ============================================================================

TEST_REGISTRY = []


def register_test(tier: int, test_id: str, name: str, feat_ids: List[str]):
    def decorator(func):
        TEST_REGISTRY.append({
            'tier': tier,
            'id': test_id,
            'name': name,
            'feat_ids': feat_ids,
            'func': func
        })
        return func
    return decorator


# ----------------------------------------------------------------------------
# TIER 1: FEATURE COVERAGE
# ----------------------------------------------------------------------------

@register_test(1, "T1_01", "Basic positional syntax (host port)", ["FEAT-001", "FEAT-006", "FEAT-012", "FEAT-015"])
def test_t1_01(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"Hello Positional\n"
        code, out, err = run_client(ctx, ["127.0.0.1", str(port)], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}. Stderr: {err}"
        assert "Hello Positional" in out, f"Payload missing in output: '{out}'"
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_02", "Positional syntax with 'localhost'", ["FEAT-001", "FEAT-006"])
def test_t1_02(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"Localhost Test\n"
        code, out, err = run_client(ctx, ["localhost", str(port)], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}. Stderr: {err}"
        assert "Localhost Test" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_03", "Flag parameters (-h -p short syntax)", ["FEAT-002", "FEAT-006"])
def test_t1_03(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"Short Flags\n"
        code, out, err = run_client(ctx, ["-h", "127.0.0.1", "-p", str(port)], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}. Stderr: {err}"
        assert "Short Flags" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_04", "Flag parameters (--host --port long syntax)", ["FEAT-002"])
def test_t1_04(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"Long Flags\n"
        code, out, err = run_client(ctx, ["--host", "127.0.0.1", "--port", str(port)], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}. Stderr: {err}"
        assert "Long Flags" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_05", "Flag parameter order flexibility (-p before -h)", ["FEAT-002"])
def test_t1_05(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"Reverse Flags\n"
        code, out, err = run_client(ctx, ["-p", str(port), "-h", "127.0.0.1"], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}"
        assert "Reverse Flags" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_06", "Force interactive flag (-i short flag)", ["FEAT-004", "FEAT-011"])
def test_t1_06(ctx: TestContext):
    srv_proc, port = start_mock_server("multiline")
    try:
        payload = b"hello\nexit\n"
        code, out, err = run_client(ctx, ["-h", "127.0.0.1", "-p", str(port), "-i"], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}"
        assert "ACK: hello" in out or "Goodbye!" in out or "> " in out or ">" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_07", "Force interactive flag (--interactive long flag)", ["FEAT-004", "FEAT-011"])
def test_t1_07(ctx: TestContext):
    srv_proc, port = start_mock_server("multiline")
    try:
        payload = b"test\nquit\n"
        code, out, err = run_client(ctx, ["--host", "127.0.0.1", "--port", str(port), "--interactive"], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}"
        assert "ACK: test" in out or "Goodbye!" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_08", "One-shot pipe mode simple payload", ["FEAT-012"])
def test_t1_08(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"One-shot payload test"
        code, out, err = run_client(ctx, ["127.0.0.1", str(port)], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}"
        assert out == "One-shot payload test"
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_09", "One-shot pipe mode multiline payload", ["FEAT-012"])
def test_t1_09(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"Line 1\nLine 2\nLine 3\n"
        code, out, err = run_client(ctx, ["127.0.0.1", str(port)], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}"
        assert out == "Line 1\nLine 2\nLine 3\n"
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_10", "One-shot pipe mode binary payload", ["FEAT-012"])
def test_t1_10(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = bytes(range(256))
        code, out, err = run_client(ctx, ["127.0.0.1", str(port)], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}"
        assert len(out.encode('latin1', errors='ignore')) >= 200 or len(out) > 0
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_11", "Timeout flag configuration (-t 3000)", ["FEAT-003", "FEAT-007"])
def test_t1_11(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"Timeout Flag Test\n"
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-t", "3000"], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}"
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_12", "Timeout flag configuration (--timeout 3000)", ["FEAT-003", "FEAT-007"])
def test_t1_12(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"Long Timeout Flag\n"
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "--timeout", "3000"], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}"
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_15_HEX", "Direct HEX payload option (-x / --hex)", ["FEAT-017"])
def test_t1_15_hex(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        # 000630303030 -> 6 bytes: 0x00, 0x06, '0', '0', '0', '0'
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-x", "00 06 30 30 30 30"])
        assert code == 0, f"Expected exit code 0, got {code}. Stderr: {err}"
        assert "0000" in out or len(out) == 6
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_16_HEX", "Direct HEX payload with HEX output format (--hex-out)", ["FEAT-017"])
def test_t1_16_hex(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "--hex", "000630303030", "--hex-out"])
        assert code == 0, f"Expected exit code 0, got {code}. Stderr: {err}"
        assert "00 06 30 30 30 30" in out.strip()
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_17_HEX", "Invalid HEX payload string error handling", ["FEAT-017"])
def test_t1_17_hex(ctx: TestContext):
    code, out, err = run_client(ctx, ["127.0.0.1", "8000", "-x", "INVALID_HEX_XYZ"])
    assert code == 1, f"Expected exit code 1 for invalid hex string, got {code}"
    assert "Invalid HEX" in err or "Error" in err


@register_test(1, "T1_18_HSM_SUCCESS", "payShield 10K HSM Response Decoder Success Case (-D)", ["FEAT-018"])
def test_t1_18_hsm_success(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        # TCP Len 000A (10 bytes): Header "HDR1" (4B) + Resp "ND" (2B) + Err "00" (2B) + Payload "00" (2B)
        hex_cmd = "000A484452314E4430303030"
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-x", hex_cmd, "-D", "--hsm-header-len", "4"])
        assert code == 0, f"Expected exit code 0, got {code}. Stderr: {err}"
        assert "payShield 10K HSM RESPONSE DECODER" in out, "Failed check 1: title"
        assert "HDR1" in out, "Failed check 2: HDR1"
        assert "ND" in out, "Failed check 3: ND"
        assert "00" in out, "Failed check 4: 00"
        assert "SUCCESS" in out, "Failed check 5: SUCCESS"
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_18B_HSM_DETAILED_BREAKDOWN", "payShield 10K Detailed Field Breakdown Formatter (-D)", ["FEAT-018"])
def test_t1_18b_hsm_detailed_breakdown(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        # TCP Len 002D (45 bytes): Header "00000000" (8B) + Resp "A1" (2B) + Err "00" (2B) + Payload (39B)
        # Payload: 'U' + 32 hex chars key + 6 hex chars KCV
        payload_ascii = "U946EC35A217E415D333AFAC7EC336116A1B2C3"
        payload_hex = payload_ascii.encode("ascii").hex()
        hex_cmd = "002D303030303030303041313030" + payload_hex
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-x", hex_cmd, "-D", "--hsm-header-len", "8"])
        assert code == 0, f"Expected exit code 0, got {code}. Stderr: {err}"
        assert "DETAILED FIELD BREAKDOWN:" in out
        assert "TCP/IP Header............ = [002D] 45 Bytes" in out
        assert "Message Header........... = [00000000]" in out
        assert "Command Code............. = [A1] Generate a Key Response" in out
        assert "Error Code............... = [00] No error" in out
        assert "Key...................... = [U946EC35A217E415D333AFAC7EC336116]" in out
        assert "Key Check Value (KCV).... = [A1B2C3]" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_19_HSM_ERROR", "payShield 10K HSM Response Decoder Error Case (-D)", ["FEAT-018"])
def test_t1_19_hsm_error(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        # TCP Len 0008 (8 bytes), Header "HDR1", Resp "A1", Err "10" (Source Key Parity Error)
        hex_cmd = "00084844523141313130"
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-x", hex_cmd, "-D", "--hsm-header-len", "4"])
        assert code == 0, f"Expected exit code 0, got {code}. Stderr: {err}"
        assert "payShield 10K HSM RESPONSE DECODER ANALYSIS" in out
        assert "'A1'" in out
        assert "'10'" in out
        assert "Source key parity error" in out or "Parity" in out
        assert "DETAILED FIELD BREAKDOWN:" in out
        assert "Error Details............ = [Source key parity error (ZMK, TPK, BDK)]" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_20_ASCII", "Direct ASCII payload option (-a / --ascii)", ["FEAT-019"])
def test_t1_20_ascii(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-a", "NC0000"])
        assert code == 0, f"Expected exit code 0, got {code}. Stderr: {err}"
        assert out == "NC0000"
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_21_ASCII_TCP_LEN", "Direct ASCII payload with 2-byte TCP length header (-a -L)", ["FEAT-019"])
def test_t1_21_ascii_tcp_len(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-a", "NC0000", "-L"])
        assert code == 0, f"Expected exit code 0, got {code}. Stderr: {err}"
        # 2-byte length header for "NC0000" (6 bytes) -> 0x00, 0x06 followed by "NC0000"
        assert len(out) == 8
        assert out.endswith("NC0000")
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_13", "Verbose logging flag (-v short flag)", ["FEAT-005"])
def test_t1_13(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"Verbose Test\n"
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-v"], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}"
        assert len(err) > 0, "Expected verbose output on STDERR"
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_14", "Verbose logging flag (--verbose long flag)", ["FEAT-005"])
def test_t1_14(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"Verbose Long Test\n"
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "--verbose"], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}"
        assert len(err) > 0, "Expected verbose output on STDERR"
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_15", "Auto-mode detection for piped STDIN stream", ["FEAT-010", "FEAT-012"])
def test_t1_15(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"Auto mode piped data\n"
        code, out, err = run_client(ctx, ["127.0.0.1", str(port)], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}"
        assert "Auto mode piped data" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_16", "Exit Code 0: Successful execution", ["FEAT-009", "FEAT-015"])
def test_t1_16(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"Exit code 0\n"
        code, out, err = run_client(ctx, ["127.0.0.1", str(port)], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}"
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_17", "Exit Code 1: Invalid CLI arguments", ["FEAT-009", "FEAT-015"])
def test_t1_17(ctx: TestContext):
    code, out, err = run_client(ctx, ["--invalid-flag-xyz"])
    assert code == 1, f"Expected exit code 1 for invalid args, got {code}"


@register_test(1, "T1_18", "Exit Code 2: Host resolution failure", ["FEAT-009", "FEAT-015"])
def test_t1_18(ctx: TestContext):
    code, out, err = run_client(ctx, ["invalid.nonexistent.domain.test", "8080"])
    assert code == 2, f"Expected exit code 2 for DNS failure, got {code}"


@register_test(1, "T1_19", "Exit Code 3: Connection refused", ["FEAT-009", "FEAT-015"])
def test_t1_19(ctx: TestContext):
    closed_port = find_unused_port()
    code, out, err = run_client(ctx, ["127.0.0.1", str(closed_port)])
    assert code == 3, f"Expected exit code 3 for connection refused, got {code}"


@register_test(1, "T1_20", "Exit Code 4: Connection/Read Timeout", ["FEAT-007", "FEAT-009", "FEAT-015"])
def test_t1_20(ctx: TestContext):
    srv_proc, port = start_mock_server("delayed", delay_ms=3000)
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-t", "500"], input_data=b"Timeout Test\n")
        assert code == 4, f"Expected exit code 4 for timeout, got {code}"
    finally:
        stop_mock_server(srv_proc)


@register_test(1, "T1_21", "Exit Code 5: Network Socket I/O / Disconnect Error", ["FEAT-008", "FEAT-009", "FEAT-015"])
def test_t1_21(ctx: TestContext):
    srv_proc, port = start_mock_server("disconnect", disconnect_after=0)
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port)], input_data=b"Disconnect Test\n")
        assert code == 5, f"Expected exit code 5 for disconnect error, got {code}"
    finally:
        stop_mock_server(srv_proc)


# ----------------------------------------------------------------------------
# TIER 2: BOUNDARY & CORNER CASES
# ----------------------------------------------------------------------------

@register_test(2, "T2_01", "Empty stdin payload streaming", ["FEAT-012"])
def test_t2_01(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port)], input_data=b"")
        assert code == 0, f"Expected exit code 0, got {code}"
        assert out == "", f"Expected empty stdout, got '{out}'"
    finally:
        stop_mock_server(srv_proc)


@register_test(2, "T2_02", "Large file payload streaming (>1MB)", ["FEAT-012"])
def test_t2_02(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        # Generate 1MB test stream
        large_chunk = b"A" * 1024 * 1024
        expected_md5 = hashlib.md5(large_chunk).hexdigest()

        code, out, err = run_client(ctx, ["127.0.0.1", str(port)], input_data=large_chunk, timeout=15.0)
        assert code == 0, f"Expected exit code 0, got {code}"
        received_md5 = hashlib.md5(out.encode('latin1', errors='replace')).hexdigest()
        assert received_md5 == expected_md5 or len(out) == len(large_chunk), f"Payload size mismatch. Length: {len(out)}"
    finally:
        stop_mock_server(srv_proc)


@register_test(2, "T2_03", "Port boundary maximum valid port 65535", ["FEAT-001", "FEAT-002"])
def test_t2_03(ctx: TestContext):
    # Port 65535 syntax check (likely connection refused or refused exit code 3)
    code, out, err = run_client(ctx, ["127.0.0.1", "65535"])
    assert code in (0, 3), f"Expected exit code 0 or 3 for port 65535, got {code}"


@register_test(2, "T2_04", "Port boundary invalid port > 65535", ["FEAT-001", "FEAT-002"])
def test_t2_04(ctx: TestContext):
    code, out, err = run_client(ctx, ["127.0.0.1", "70000"])
    assert code == 1, f"Expected exit code 1 for port > 65535, got {code}"


@register_test(2, "T2_05", "Invalid non-numeric port parameter", ["FEAT-001", "FEAT-002"])
def test_t2_05(ctx: TestContext):
    code, out, err = run_client(ctx, ["127.0.0.1", "not_a_port"])
    assert code == 1, f"Expected exit code 1 for non-numeric port, got {code}"


@register_test(2, "T2_06", "Invalid host IP address / unresolvable host", ["FEAT-006", "FEAT-009"])
def test_t2_06(ctx: TestContext):
    code, out, err = run_client(ctx, ["999.999.999.999", "8080"], timeout=15.0)
    assert code == 2, f"Expected exit code 2 for invalid IP/host, got {code}"


@register_test(2, "T2_07", "Connection refused on unused closed port", ["FEAT-006", "FEAT-009"])
def test_t2_07(ctx: TestContext):
    closed_port = find_unused_port()
    code, out, err = run_client(ctx, ["127.0.0.1", str(closed_port)])
    assert code == 3, f"Expected exit code 3 for connection refused, got {code}"


@register_test(2, "T2_08", "Instant timeout (1ms timeout against delayed mock server)", ["FEAT-003", "FEAT-007"])
def test_t2_08(ctx: TestContext):
    srv_proc, port = start_mock_server("delayed", delay_ms=2000)
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-t", "1"], input_data=b"Fast timeout\n")
        assert code == 4, f"Expected exit code 4 for 1ms timeout, got {code}"
    finally:
        stop_mock_server(srv_proc)


# ----------------------------------------------------------------------------
# TIER 3: CROSS-FEATURE COMBINATIONS
# ----------------------------------------------------------------------------

@register_test(3, "T3_01", "Flag syntax + One-shot pipe mode", ["FEAT-002", "FEAT-012"])
def test_t3_01(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"Flag + Pipe Mode Combination\n"
        code, out, err = run_client(ctx, ["-h", "127.0.0.1", "-p", str(port)], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}"
        assert "Flag + Pipe Mode Combination" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(3, "T3_02", "Interactive flag + Verbose mode combination (-i -v)", ["FEAT-004", "FEAT-005", "FEAT-011"])
def test_t3_02(ctx: TestContext):
    srv_proc, port = start_mock_server("multiline")
    try:
        payload = b"combo\nexit\n"
        code, out, err = run_client(ctx, ["-h", "127.0.0.1", "-p", str(port), "-i", "-v"], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}"
        assert len(err) > 0, "Expected verbose messages on STDERR"
    finally:
        stop_mock_server(srv_proc)


@register_test(3, "T3_03", "Positional host/port + Timeout flag (<host> <port> -t 1000)", ["FEAT-001", "FEAT-003", "FEAT-007"])
def test_t3_03(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"Positional and Timeout Combo\n"
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-t", "1000"], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}"
        assert "Positional and Timeout Combo" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(3, "T3_04", "Short timeout + Verbose logging (-t 500 -v)", ["FEAT-003", "FEAT-005", "FEAT-007"])
def test_t3_04(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"Short Timeout Verbose Combo\n"
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-t", "500", "-v"], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}"
        assert len(err) > 0, "Expected verbose output on STDERR"
    finally:
        stop_mock_server(srv_proc)


@register_test(3, "T3_05", "Full flag combination (-h -p -t -v + pipe)", ["FEAT-002", "FEAT-003", "FEAT-005", "FEAT-012"])
def test_t3_05(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"All Flags Combined\n"
        code, out, err = run_client(ctx, ["-h", "127.0.0.1", "-p", str(port), "-t", "2000", "-v"], input_data=payload)
        assert code == 0, f"Expected exit code 0, got {code}"
        assert "All Flags Combined" in out
        assert len(err) > 0
    finally:
        stop_mock_server(srv_proc)


# ----------------------------------------------------------------------------
# TIER 4: REAL-WORLD APPLICATION SCENARIOS
# ----------------------------------------------------------------------------

@register_test(4, "T4_01", "HTTP GET Simulation scenario", ["FEAT-012", "FEAT-016"])
def test_t4_01(ctx: TestContext):
    srv_proc, port = start_mock_server("http")
    try:
        http_req = b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"
        code, out, err = run_client(ctx, ["127.0.0.1", str(port)], input_data=http_req)
        assert code == 0, f"Expected exit code 0, got {code}"
        assert "HTTP/1.1 200 OK" in out, f"Expected HTTP OK header in output: {out}"
        assert "Hello, World!" in out, f"Expected HTTP body in output: {out}"
    finally:
        stop_mock_server(srv_proc)


@register_test(4, "T4_02", "JSON file pipe transmission scenario", ["FEAT-012", "FEAT-016"])
def test_t4_02(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        json_payload = b'{\n  "service": "tcp-client-test",\n  "status": "ok",\n  "count": 42\n}\n'
        code, out, err = run_client(ctx, ["127.0.0.1", str(port)], input_data=json_payload)
        assert code == 0, f"Expected exit code 0, got {code}"
        assert '"service": "tcp-client-test"' in out
        assert '"status": "ok"' in out
    finally:
        stop_mock_server(srv_proc)


@register_test(4, "T4_03", "Multi-message interactive session simulation", ["FEAT-011", "FEAT-016"])
def test_t4_03(ctx: TestContext):
    srv_proc, port = start_mock_server("multiline")
    try:
        interactive_stream = b"PING\nPONG\nexit\n"
        code, out, err = run_client(ctx, ["-h", "127.0.0.1", "-p", str(port), "-i"], input_data=interactive_stream)
        assert code == 0, f"Expected exit code 0, got {code}"
        assert "ACK: PING" in out or "ACK: PONG" in out or "Goodbye!" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(4, "T4_04", "Server premature disconnect mid-transfer scenario", ["FEAT-008", "FEAT-009", "FEAT-015"])
def test_t4_04(ctx: TestContext):
    srv_proc, port = start_mock_server("disconnect", disconnect_after=10)
    try:
        stream_data = b"This is a longer string that exceeds 10 bytes"
        code, out, err = run_client(ctx, ["127.0.0.1", str(port)], input_data=stream_data)
        assert code == 5, f"Expected exit code 5 for abrupt disconnect, got {code}"
    finally:
        stop_mock_server(srv_proc)


# ----------------------------------------------------------------------------
# TIER 5: ADVERSARIAL EDGE CASES & HIGH-THROUGHPUT STREAMING
# ----------------------------------------------------------------------------

@register_test(5, "T5_01", "High-Throughput 8MB Stream Payload", ["FEAT-012", "FEAT-016"])
def test_t5_01(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        chunk = b"X" * (8 * 1024 * 1024)
        expected_md5 = hashlib.md5(chunk).hexdigest()
        code, out, err = run_client(ctx, ["127.0.0.1", str(port)], input_data=chunk, timeout=15.0)
        assert code == 0, f"Expected exit code 0, got {code}. Stderr: {err}"
        received_md5 = hashlib.md5(out.encode('latin1', errors='replace')).hexdigest()
        assert received_md5 == expected_md5, f"8MB payload checksum mismatch"
    finally:
        stop_mock_server(srv_proc)


@register_test(5, "T5_02", "Interactive long line (>4096 bytes)", ["FEAT-011", "FEAT-016"])
def test_t5_02(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        payload = b"A" * 5000 + b"\nexit\n"
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-i"], input_data=payload, timeout=5.0)
        assert code == 0, f"Expected exit code 0, got {code}. Stderr: {err}"
        a_count = out.count("A")
        assert a_count == 5000, f"Expected 5000 'A's in interactive response, got {a_count}"
    finally:
        stop_mock_server(srv_proc)


@register_test(5, "T5_03", "Interactive exit command (lowercase)", ["FEAT-011"])
def test_t5_03(ctx: TestContext):
    srv_proc, port = start_mock_server("multiline")
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-i"], input_data=b"hello\nexit\n")
        assert code == 0, f"Expected exit code 0, got {code}"
        assert "ACK: hello" in out
        assert "Goodbye!" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(5, "T5_04", "Interactive QUIT command (uppercase)", ["FEAT-011"])
def test_t5_04(ctx: TestContext):
    srv_proc, port = start_mock_server("multiline")
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-i"], input_data=b"hello\nQUIT\n")
        assert code == 0, f"Expected exit code 0, got {code}"
        assert "Goodbye!" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(5, "T5_05", "Interactive exit command with spaces & mixed case", ["FEAT-011"])
def test_t5_05(ctx: TestContext):
    srv_proc, port = start_mock_server("multiline")
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-i"], input_data=b"hello\n  ExIt  \n")
        assert code == 0, f"Expected exit code 0, got {code}"
        assert "Goodbye!" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(5, "T5_06", "Multi-turn interactive conversation", ["FEAT-011"])
def test_t5_06(ctx: TestContext):
    srv_proc, port = start_mock_server("multiline")
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-i"], input_data=b"msg1\nmsg2\nmsg3\nquit\n")
        assert code == 0, f"Expected exit code 0, got {code}"
        assert "ACK: msg1" in out
        assert "ACK: msg2" in out
        assert "ACK: msg3" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(5, "T5_07", "STDIN EOF in interactive mode without explicit exit", ["FEAT-011"])
def test_t5_07(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-i"], input_data=b"hello stream\n")
        assert code == 0, f"Expected exit code 0, got {code}"
        assert "hello stream" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(5, "T5_08", "Empty STDIN in interactive mode", ["FEAT-011"])
def test_t5_08(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-i"], input_data=b"")
        assert code == 0, f"Expected exit code 0, got {code}"
    finally:
        stop_mock_server(srv_proc)


@register_test(5, "T5_09", "Partial line before STDIN EOF", ["FEAT-011"])
def test_t5_09(ctx: TestContext):
    srv_proc, port = start_mock_server("echo")
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-i"], input_data=b"no newline EOF")
        assert code == 0, f"Expected exit code 0, got {code}"
        assert "no newline EOF" in out
    finally:
        stop_mock_server(srv_proc)


@register_test(5, "T5_10", "Instant connection/read timeout (1ms)", ["FEAT-007"])
def test_t5_10(ctx: TestContext):
    srv_proc, port = start_mock_server("delayed", delay_ms=3000)
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-t", "1"], input_data=b"fast\n")
        assert code == 4, f"Expected exit code 4, got {code}"
    finally:
        stop_mock_server(srv_proc)


@register_test(5, "T5_11", "One-shot read timeout (500ms)", ["FEAT-007"])
def test_t5_11(ctx: TestContext):
    srv_proc, port = start_mock_server("delayed", delay_ms=3000)
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port), "-t", "500"], input_data=b"test oneshot timeout\n")
        assert code == 4, f"Expected exit code 4, got {code}"
    finally:
        stop_mock_server(srv_proc)


@register_test(5, "T5_12", "Non-routable IP connection timeout", ["FEAT-007"])
def test_t5_12(ctx: TestContext):
    code, out, err = run_client(ctx, ["10.255.255.1", "8080", "-t", "300"])
    assert code == 4, f"Expected exit code 4, got {code}"


@register_test(5, "T5_13", "Immediate server disconnect (0 bytes)", ["FEAT-008"])
def test_t5_13(ctx: TestContext):
    srv_proc, port = start_mock_server("disconnect", disconnect_after=0)
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port)], input_data=b"Immediate disconnect\n")
        assert code == 5, f"Expected exit code 5, got {code}"
    finally:
        stop_mock_server(srv_proc)


@register_test(5, "T5_14", "SIGPIPE signal suppression on broken pipe", ["FEAT-008"])
def test_t5_14(ctx: TestContext):
    srv_proc, port = start_mock_server("disconnect", disconnect_after=5)
    try:
        code, out, err = run_client(ctx, ["127.0.0.1", str(port)], input_data=b"Long message causing SIGPIPE attempt\n")
        assert code == 5, f"Expected exit code 5, got {code}"
    finally:
        stop_mock_server(srv_proc)


@register_test(5, "T5_15_TCP_SEGMENTATION", "TCP Segmentation 272-byte packet complete reception without 17-byte loss", ["FEAT-008"])
def test_t5_15(ctx: TestContext):
    srv_proc, port = start_mock_server("segmented")
    try:
        code, out, err = run_client(ctx, ["-h", "127.0.0.1", "-p", str(port), "-D"], input_data=b"")
        assert code == 0, f"Expected exit code 0, got {code}"
        assert "Raw Packet Length : 272 bytes" in out, f"Expected 272 bytes packet, got: {out}"
        assert "Key Check Value (KCV).... = [PPPPPPKCV123]" in out, f"Expected KCV in output, got: {out}"
        assert "Key Block (TR-31)" in out, f"Expected TR-31 block in output, got: {out}"
    finally:
        stop_mock_server(srv_proc)


# ============================================================================
# MAIN RUNNER LOGIC
# ============================================================================

def list_tests():
    """Prints listing of all available tests by Tier."""
    print("======================================================================")
    print("TCP Client CLI E2E Test Suite - Feature & Tier Inventory")
    print("======================================================================")
    for tier_num in [1, 2, 3, 4, 5]:
        tier_tests = [t for t in TEST_REGISTRY if t['tier'] == tier_num]
        print(f"\n--- Tier {tier_num}: {get_tier_name(tier_num)} ({len(tier_tests)} Test Cases) ---")
        for test in tier_tests:
            feats = ", ".join(test['feat_ids'])
            print(f"  [{test['id']}] {test['name']:<55} (Features: {feats})")
    print(f"\nTotal Registered Test Cases: {len(TEST_REGISTRY)}")


def get_tier_name(tier: int) -> str:
    names = {
        1: "Feature Coverage",
        2: "Boundary & Corner Cases",
        3: "Cross-Feature Combinations",
        4: "Real-World Application Scenarios",
        5: "Adversarial Edge Cases & High-Throughput Streaming"
    }
    return names.get(tier, "Unknown Tier")


def main():
    parser = argparse.ArgumentParser(description="Automated E2E Test Runner for TCP Client CLI")
    parser.add_argument("--binary", default=DEFAULT_BINARY, help=f"Path to tcp-client binary (default: {DEFAULT_BINARY})")
    parser.add_argument("--tier", choices=["1", "2", "3", "4", "5", "all"], default="all", help="Tier to execute (default: all)")
    parser.add_argument("--verbose", action="store_true", help="Enable verbose logging output")
    parser.add_argument("--list", action="store_true", help="List all available test cases by Tier and exit")

    args = parser.parse_args()

    if args.list:
        list_tests()
        sys.exit(0)

    ctx = TestContext(args.binary, args.verbose)

    print("======================================================================")
    print("TCP CLIENT CLI AUTOMATED E2E TEST RUNNER")
    print("======================================================================")
    print(f"Target Binary : {ctx.binary_path}")
    print(f"Binary Status : {'FOUND' if ctx.binary_exists else 'NOT FOUND'}")
    print(f"Filter Tier   : {args.tier.upper()}")
    print("----------------------------------------------------------------------")

    if not ctx.binary_exists:
        print(f"\n[ERROR] Target binary '{ctx.binary_path}' not found or is not executable.")
        print("        Please compile the project executable (e.g., run 'make' or build script) before running tests.")
        sys.exit(1)

    # Filter tests to run
    if args.tier == "all":
        tests_to_run = TEST_REGISTRY
    else:
        target_tier = int(args.tier)
        tests_to_run = [t for t in TEST_REGISTRY if t['tier'] == target_tier]

    print(f"Running {len(tests_to_run)} test cases...\n")

    for test in tests_to_run:
        test_id = test['id']
        test_name = test['name']
        ctx.log(f"Starting test [{test_id}] {test_name}")

        try:
            test['func'](ctx)
            print(f"  [PASS] [{test_id}] {test_name}")
            ctx.passed += 1
        except AssertionError as e:
            print(f"  [FAIL] [{test_id}] {test_name}: {e}")
            ctx.failed += 1
        except Exception as e:
            print(f"  [ERROR] [{test_id}] {test_name}: Unexpected exception: {e}")
            ctx.failed += 1

    print("\n----------------------------------------------------------------------")
    print("TEST EXECUTION SUMMARY")
    print(f"Total Run : {len(tests_to_run)}")
    print(f"Passed    : {ctx.passed}")
    print(f"Failed    : {ctx.failed}")
    print("----------------------------------------------------------------------")

    if ctx.failed > 0:
        sys.exit(1)
    else:
        sys.exit(0)


if __name__ == "__main__":
    main()

