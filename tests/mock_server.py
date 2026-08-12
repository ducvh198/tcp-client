#!/usr/bin/env python3
"""
TCP Mock Server for TCP Client CLI E2E Testing.
Supports configurable modes: echo, delayed, disconnect, multiline, http.
Uses Python 3 standard library only.
"""

import argparse
import socket
import sys
import time
import threading
import select


def log_msg(quiet: bool, msg: str):
    """Print message to stderr if not quiet."""
    if not quiet:
        sys.stderr.write(f"[mock_server] {msg}\n")
        sys.stderr.flush()


def handle_client_echo(conn: socket.socket, addr, quiet: bool):
    """Echo all received bytes back to client until client disconnects or closes."""
    log_msg(quiet, f"Connected (echo) from {addr}")
    try:
        while True:
            data = conn.recv(4096)
            if not data:
                break
            conn.sendall(data)
    except (ConnectionResetError, BrokenPipeError):
        log_msg(quiet, "Client disconnected abruptly")
    finally:
        conn.close()
        log_msg(quiet, f"Closed connection from {addr}")


def handle_client_delayed(conn: socket.socket, addr, delay_ms: int, quiet: bool):
    """Delay response by delay_ms before echoing data back or closing."""
    log_msg(quiet, f"Connected (delayed {delay_ms}ms) from {addr}")
    try:
        # Read available data first if any
        conn.settimeout(delay_ms / 1000.0 + 1.0)
        try:
            data = conn.recv(4096)
        except socket.timeout:
            data = b""

        # Perform configured delay
        time.sleep(delay_ms / 1000.0)

        if data:
            conn.sendall(data)
    except (ConnectionResetError, BrokenPipeError):
        log_msg(quiet, "Client disconnected abruptly during delay")
    finally:
        conn.close()
        log_msg(quiet, f"Closed delayed connection from {addr}")


def handle_client_disconnect(conn: socket.socket, addr, disconnect_after: int, quiet: bool):
    """Disconnect immediately or after receiving N bytes."""
    log_msg(quiet, f"Connected (disconnect after {disconnect_after} bytes) from {addr}")
    try:
        if disconnect_after > 0:
            received = 0
            while received < disconnect_after:
                chunk = conn.recv(min(4096, disconnect_after - received))
                if not chunk:
                    break
                received += len(chunk)
                log_msg(quiet, f"Received {received}/{disconnect_after} bytes before disconnect")
        # Abruptly close socket
        log_msg(quiet, f"Abruptly closing socket for {addr}")
    except Exception as e:
        log_msg(quiet, f"Error in disconnect mode: {e}")
    finally:
        try:
            # Set SO_LINGER to 0 for hard RST disconnect if supported
            conn.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, b'\x01\x00\x00\x00\x00\x00\x00\x00')
        except Exception:
            pass
        conn.close()


def handle_client_multiline(conn: socket.socket, addr, quiet: bool):
    """Interactive line-by-line mode: responds ACK: <line> until exit/quit command."""
    log_msg(quiet, f"Connected (multiline/interactive) from {addr}")
    try:
        conn_file = conn.makefile('r', encoding='utf-8', errors='replace')
        while True:
            line = conn_file.readline()
            if not line:
                break
            stripped = line.strip()
            log_msg(quiet, f"Received line: '{stripped}'")
            if stripped.lower() in ('exit', 'quit'):
                conn.sendall(b"Goodbye!\n")
                break
            response = f"ACK: {stripped}\n"
            conn.sendall(response.encode('utf-8'))
    except (ConnectionResetError, BrokenPipeError):
        log_msg(quiet, "Client disconnected abruptly")
    finally:
        conn.close()
        log_msg(quiet, f"Closed multiline connection from {addr}")


def handle_client_http(conn: socket.socket, addr, quiet: bool):
    """Sends HTTP/1.1 200 OK response upon receiving request."""
    log_msg(quiet, f"Connected (http) from {addr}")
    try:
        conn.settimeout(2.0)
        try:
            _req = conn.recv(4096)
        except socket.timeout:
            pass

        body = "Hello, World!\n"
        headers = (
            "HTTP/1.1 200 OK\r\n"
            "Server: TCP-Client-Mock-Server/1.0\r\n"
            "Content-Type: text/plain\r\n"
            f"Content-Length: {len(body)}\r\n"
            "Connection: close\r\n"
            "\r\n"
        )
        response = headers + body
        conn.sendall(response.encode('utf-8'))
    except (ConnectionResetError, BrokenPipeError):
        log_msg(quiet, "Client disconnected abruptly in HTTP mode")
    finally:
        conn.close()
        log_msg(quiet, f"Closed HTTP connection from {addr}")


def handle_client_segmented(conn: socket.socket, addr, quiet: bool):
    """Simulates TCP Segmentation by splitting a 272-byte payload into 255-byte and 17-byte chunks with delay."""
    log_msg(quiet, f"Connected (segmented) from {addr}")
    try:
        conn.settimeout(2.0)
        try:
            _req = conn.recv(4096)
        except socket.timeout:
            pass

        # Build 272-byte packet: 2-byte header (270 bytes payload) + A1 (resp) + 00 (err) + TR-31 payload + KCV
        payload_len = 270
        header = bytes([ (payload_len >> 8) & 0xFF, payload_len & 0xFF ]) # 2 bytes
        resp_code = b"A1" # 2 bytes
        err_code = b"00" # 2 bytes
        # TR-31 payload (260 bytes: 'S' + length + key block) + KCV (6 bytes: "123456")
        tr31_data = (b"S0254" + b"K" * 249) # 254 bytes
        kcv = b"KCV123" # 6 bytes
        full_packet = header + resp_code + err_code + tr31_data + kcv # Total: 2 + 2 + 2 + 254 + 6 = 266 -> let's pad payload to 270 bytes
        pad = b"P" * (272 - len(full_packet))
        full_packet = header + resp_code + err_code + tr31_data + pad + kcv # Exactly 272 bytes

        # Segment 1: first 255 bytes
        seg1 = full_packet[:255]
        # Segment 2: remaining 17 bytes (272 - 255 = 17 bytes)
        seg2 = full_packet[255:]

        log_msg(quiet, f"Sending Segment 1 ({len(seg1)} bytes)...")
        conn.sendall(seg1)
        time.sleep(0.1) # 100ms TCP segmentation delay
        log_msg(quiet, f"Sending Segment 2 ({len(seg2)} bytes)...")
        conn.sendall(seg2)
    except (ConnectionResetError, BrokenPipeError):
        log_msg(quiet, "Client disconnected abruptly in segmented mode")
    finally:
        conn.close()
        log_msg(quiet, f"Closed segmented connection from {addr}")


def run_mock_server(host: str, port: int, mode: str, delay_ms: int, disconnect_after: int,
                    quiet: bool, port_file: str = None, once: bool = False):
    """Main server loop binding host/port and servicing connections based on mode."""
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    srv.bind((host, port))
    srv.listen(5)

    allocated_port = srv.getsockname()[1]
    
    # Print allocated port for script parsing
    sys.stdout.write(f"PORT: {allocated_port}\n")
    sys.stdout.flush()

    if port_file:
        try:
            with open(port_file, 'w', encoding='utf-8') as f:
                f.write(f"{allocated_port}\n")
        except Exception as e:
            log_msg(quiet, f"Error writing port to file {port_file}: {e}")

    log_msg(quiet, f"Listening on {host}:{allocated_port} (mode: {mode})")

    handler_map = {
        'echo': lambda conn, addr: handle_client_echo(conn, addr, quiet),
        'delayed': lambda conn, addr: handle_client_delayed(conn, addr, delay_ms, quiet),
        'disconnect': lambda conn, addr: handle_client_disconnect(conn, addr, disconnect_after, quiet),
        'multiline': lambda conn, addr: handle_client_multiline(conn, addr, quiet),
        'http': lambda conn, addr: handle_client_http(conn, addr, quiet),
        'segmented': lambda conn, addr: handle_client_segmented(conn, addr, quiet),
    }

    handler = handler_map.get(mode, handler_map['echo'])

    srv.settimeout(1.0)
    running = True

    try:
        while running:
            try:
                conn, addr = srv.accept()
            except socket.timeout:
                continue
            except OSError:
                break

            # Handle client connection
            t = threading.Thread(target=handler, args=(conn, addr), daemon=True)
            t.start()

            if once:
                t.join()
                break

    except KeyboardInterrupt:
        log_msg(quiet, "Server shutting down via KeyboardInterrupt")
    finally:
        srv.close()
        log_msg(quiet, "Server stopped.")


def main():
    parser = argparse.ArgumentParser(description="Mock TCP Server for TCP Client CLI testing")
    parser.add_argument("--host", default="127.0.0.1", help="Host IP to bind (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=0, help="Port to listen on (0 for dynamic free port)")
    parser.add_argument("--mode", choices=["echo", "delayed", "disconnect", "multiline", "http", "segmented"],
                        default="echo", help="Server operating mode (default: echo)")
    parser.add_argument("--delay-ms", type=int, default=2000, help="Delay in milliseconds for delayed mode")
    parser.add_argument("--disconnect-after", type=int, default=0,
                        help="Bytes received before disconnect in disconnect mode (0 for immediate)")
    parser.add_argument("--quiet", action="store_true", help="Suppress debug logging output")
    parser.add_argument("--port-file", default=None, help="File path to write allocated port number to")
    parser.add_argument("--once", action="store_true", help="Exit after handling a single connection")

    args = parser.parse_args()

    run_mock_server(
        host=args.host,
        port=args.port,
        mode=args.mode,
        delay_ms=args.delay_ms,
        disconnect_after=args.disconnect_after,
        quiet=args.quiet,
        port_file=args.port_file,
        once=args.once
    )


if __name__ == "__main__":
    main()
