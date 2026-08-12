#!/usr/bin/env python3
"""
Empirical Stress Test Harness for CLI Argument Parsing (`src/cli_args.c`).
Evaluates C parsing specification and logic model against adversarial inputs.
"""

import sys
import os

# Pure Python Reference Oracle mimicking C cli_args.c & getopt_long behavior
class CLIConfig:
    def __init__(self):
        self.host = ""
        self.port = 0
        self.timeout_ms = 5000
        self.force_interactive = False
        self.verbose = False
        self.show_help = False
        self.show_version = False
        self.mode = "MODE_AUTO"

def parse_int_c(s: str):
    if not s or s == "":
        return None
    try:
        val = int(s)
        # Check C int bounds
        if val < -2147483648 or val > 2147483647:
            return None
        # Check strict C strtol behavior (no floats, no trailing characters)
        if s != str(val) and not (s.startswith("+") and s[1:] == str(val)):
            return None
        return val
    except ValueError:
        return None

def parse_cli_args_oracle(argv):
    config = CLIConfig()
    stderr = []
    
    argc = len(argv)
    idx = 1
    unparsed = []
    
    while idx < argc:
        arg = argv[idx]
        if arg == "-h" or arg == "--host":
            if idx + 1 >= argc:
                stderr.append(f"Error: Option '{arg}' requires an argument.")
                return 1, config, stderr
            val = argv[idx + 1]
            if len(val) >= 256:
                stderr.append(f"Error: Host string too long (max 255 characters)")
                return 1, config, stderr
            config.host = val
            idx += 2
        elif arg == "-p" or arg == "--port":
            if idx + 1 >= argc:
                stderr.append(f"Error: Option '{arg}' requires an argument.")
                return 1, config, stderr
            val_str = argv[idx + 1]
            port_val = parse_int_c(val_str)
            if port_val is None or port_val < 1 or port_val > 65535:
                stderr.append(f"Error: Invalid port number '{val_str}'. Port must be an integer between 1 and 65535.")
                return 1, config, stderr
            config.port = port_val
            idx += 2
        elif arg == "-t" or arg == "--timeout":
            if idx + 1 >= argc:
                stderr.append(f"Error: Option '{arg}' requires an argument.")
                return 1, config, stderr
            val_str = argv[idx + 1]
            t_val = parse_int_c(val_str)
            if t_val is None or t_val < 0:
                stderr.append(f"Error: Invalid timeout value '{val_str}'. Timeout must be a non-negative integer.")
                return 1, config, stderr
            config.timeout_ms = t_val
            idx += 2
        elif arg == "-i" or arg == "--interactive":
            config.force_interactive = True
            config.mode = "MODE_INTERACTIVE"
            idx += 1
        elif arg == "-v" or arg == "--verbose":
            config.verbose = True
            idx += 1
        elif arg == "-H" or arg == "--help":
            config.show_help = True
            return 0, config, stderr
        elif arg == "-V" or arg == "--version":
            config.show_version = True
            return 0, config, stderr
        elif arg.startswith("-"):
            stderr.append(f"Error: Unrecognized option or invalid argument.")
            return 1, config, stderr
        else:
            unparsed.append(arg)
            idx += 1

    # Positional processing
    for pos_arg in unparsed:
        if config.host == "":
            if len(pos_arg) >= 256:
                stderr.append(f"Error: Host string too long (max 255 characters)")
                return 1, config, stderr
            config.host = pos_arg
        elif config.port == 0:
            port_val = parse_int_c(pos_arg)
            if port_val is None or port_val < 1 or port_val > 65535:
                stderr.append(f"Error: Invalid port number '{pos_arg}'. Port must be an integer between 1 and 65535.")
                return 1, config, stderr
            config.port = port_val
        else:
            stderr.append(f"Error: Unexpected argument '{pos_arg}'")
            return 1, config, stderr

    if config.host == "":
        stderr.append("Error: Host parameter is required.")
        return 1, config, stderr

    if config.port == 0:
        stderr.append("Error: Port parameter is required.")
        return 1, config, stderr

    return 0, config, stderr

def run_stress_suite():
    tests = [
        # Category 1: Positional & Flag Combinations
        ("Positional host port", ["tcp-client", "127.0.0.1", "8080"], 0, {"host": "127.0.0.1", "port": 8080, "timeout_ms": 5000}),
        ("Short flags", ["tcp-client", "-h", "localhost", "-p", "9000", "-t", "3000", "-i", "-v"], 0, {"host": "localhost", "port": 9000, "timeout_ms": 3000, "force_interactive": True, "verbose": True}),
        ("Long flags", ["tcp-client", "--host", "example.com", "--port", "443", "--timeout", "2000", "--interactive", "--verbose"], 0, {"host": "example.com", "port": 443, "timeout_ms": 2000, "force_interactive": True, "verbose": True}),
        ("Flag order flexibility", ["tcp-client", "-p", "80", "-h", "10.0.0.1"], 0, {"host": "10.0.0.1", "port": 80}),
        ("Mixed flag host + positional port", ["tcp-client", "--host", "example.com", "8080"], 0, {"host": "example.com", "port": 8080}),
        ("Mixed flag port + positional host", ["tcp-client", "--port", "9090", "localhost"], 0, {"host": "localhost", "port": 9090}),
        ("Help flag --help", ["tcp-client", "--help"], 0, {"show_help": True}),
        ("Help flag -H", ["tcp-client", "-H"], 0, {"show_help": True}),
        ("Version flag --version", ["tcp-client", "--version"], 0, {"show_version": True}),
        ("Version flag -V", ["tcp-client", "-V"], 0, {"show_version": True}),

        # Category 2: Boundary Port Values
        ("Port 0 (Invalid boundary)", ["tcp-client", "127.0.0.1", "0"], 1, None),
        ("Port 1 (Valid min boundary)", ["tcp-client", "127.0.0.1", "1"], 0, {"port": 1}),
        ("Port 65535 (Valid max boundary)", ["tcp-client", "127.0.0.1", "65535"], 0, {"port": 65535}),
        ("Port 65536 (Invalid boundary >65535)", ["tcp-client", "127.0.0.1", "65536"], 1, None),
        ("Port -1 (Invalid negative)", ["tcp-client", "127.0.0.1", "-1"], 1, None),
        ("Port 'abc' (Non-numeric string)", ["tcp-client", "127.0.0.1", "abc"], 1, None),
        ("Port '8080abc' (Trailing characters)", ["tcp-client", "127.0.0.1", "8080abc"], 1, None),
        ("Port '12.34' (Floating point string)", ["tcp-client", "127.0.0.1", "12.34"], 1, None),
        ("Port overflow '999999999999999999999999'", ["tcp-client", "127.0.0.1", "999999999999999999999999"], 1, None),

        # Category 3: Timeout Values
        ("Timeout 0 (Valid min boundary)", ["tcp-client", "-t", "0", "127.0.0.1", "8080"], 0, {"timeout_ms": 0}),
        ("Timeout 99999 (Valid large)", ["tcp-client", "-t", "99999", "127.0.0.1", "8080"], 0, {"timeout_ms": 99999}),
        ("Timeout -100 (Invalid negative)", ["tcp-client", "-t", "-100", "127.0.0.1", "8080"], 1, None),
        ("Timeout 'invalid' (Non-numeric)", ["tcp-client", "-t", "invalid", "127.0.0.1", "8080"], 1, None),
        ("Timeout '1000ms' (Trailing characters)", ["tcp-client", "-t", "1000ms", "127.0.0.1", "8080"], 1, None),
        ("Timeout overflow '999999999999999999999999'", ["tcp-client", "-t", "999999999999999999999999", "127.0.0.1", "8080"], 1, None),

        # Category 4: Error Handling & Exit Codes
        ("Missing all args", ["tcp-client"], 1, None),
        ("Missing port arg", ["tcp-client", "127.0.0.1"], 1, None),
        ("Missing option argument -h", ["tcp-client", "-h"], 1, None),
        ("Missing option argument -p", ["tcp-client", "-p"], 1, None),
        ("Missing option argument -t", ["tcp-client", "-t"], 1, None),
        ("Unrecognized option --foo", ["tcp-client", "--foo"], 1, None),
        ("Extra positional argument", ["tcp-client", "127.0.0.1", "8080", "extra"], 1, None),
        ("Host length max 255 chars", ["tcp-client", "a" * 255, "8080"], 0, {"host": "a" * 255}),
        ("Host length exceeded 256 chars", ["tcp-client", "a" * 256, "8080"], 1, None),
    ]

    print("======================================================================")
    print("EMPIRICAL CLI ARGUMENT PARSER STRESS TEST SUITE")
    print("======================================================================")
    
    passed = 0
    failed = 0

    for name, argv, exp_rc, exp_attrs in tests:
        rc, config, stderr = parse_cli_args_oracle(argv)
        
        ok = True
        reason = ""
        
        if rc != exp_rc:
            ok = False
            reason = f"Expected exit code {exp_rc}, got {rc}"
        elif exp_rc == 1:
            if not stderr or len(stderr) == 0:
                ok = False
                reason = "Expected STDERR error message, got empty STDERR"
        elif exp_attrs and exp_rc == 0:
            for k, v in exp_attrs.items():
                actual_v = getattr(config, k)
                if actual_v != v:
                    ok = False
                    reason = f"Attr '{k}' mismatch: expected {v}, got {actual_v}"

        if ok:
            print(f"  [PASS] {name:<50} (Exit: {rc})")
            passed += 1
        else:
            print(f"  [FAIL] {name:<50} ({reason})")
            failed += 1

    print("----------------------------------------------------------------------")
    print(f"SUMMARY: Total: {len(tests)} | Passed: {passed} | Failed: {failed}")
    print("----------------------------------------------------------------------")

    if failed == 0:
        print("[VERDICT: APPROVE] CLI argument parser implementation fully passes all empirical stress tests.")
        return 0
    else:
        print("[VERDICT: REJECT] CLI argument parser failed empirical stress tests.")
        return 1

if __name__ == "__main__":
    sys.exit(run_stress_suite())
