#!/usr/bin/env python3
"""
Integration test harness for Task 2:
- Tab completion callback (interactive_completion_callback)
- Persistent history path resolution (get_interactive_history_path)
- Linenoise edge case hardening (ESC during completion, Win extended keys, defensive EOF)
"""

import os
import sys
import shutil
import tempfile
import subprocess
import unittest

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

EXPECTED_COMPLETIONS = [
    "exit", "quit", "help", "clear", "status", "history",
    "NC0000", "00 06 30 30 30 30", "BA", "BB", "CA", "CB", "M0", "M2"
]

def resolve_history_path_mock(env_dict, platform="win32"):
    """Reference implementation of history path resolution logic."""
    if platform == "win32":
        userprofile = env_dict.get("USERPROFILE")
        if userprofile:
            return f"{userprofile}\\.tcp_client_history"
        homedrive = env_dict.get("HOMEDRIVE")
        homepath = env_dict.get("HOMEPATH")
        if homedrive and homepath:
            return f"{homedrive}{homepath}\\.tcp_client_history"
        return "./.tcp_client_history"
    else:
        home = env_dict.get("HOME")
        if home:
            return f"{home}/.tcp_client_history"
        return "./.tcp_client_history"

def match_completions_mock(buf):
    """Reference implementation of prefix matching."""
    if not buf:
        return list(EXPECTED_COMPLETIONS)
    return [cmd for cmd in EXPECTED_COMPLETIONS if cmd.startswith(buf)]


class TestInteractiveLogic(unittest.TestCase):

    def test_completion_prefix_matching(self):
        """Verify command completions match expected prefixes."""
        # Empty buffer returns all commands
        self.assertEqual(match_completions_mock(""), EXPECTED_COMPLETIONS)
        self.assertEqual(match_completions_mock(None), EXPECTED_COMPLETIONS)

        # Built-in commands
        self.assertEqual(match_completions_mock("ex"), ["exit"])
        self.assertEqual(match_completions_mock("qui"), ["quit"])
        self.assertEqual(match_completions_mock("h"), ["help", "history"])
        self.assertEqual(match_completions_mock("he"), ["help"])
        self.assertEqual(match_completions_mock("hi"), ["history"])
        self.assertEqual(match_completions_mock("cl"), ["clear"])
        self.assertEqual(match_completions_mock("st"), ["status"])

        # HSM & Payment commands
        self.assertEqual(match_completions_mock("NC"), ["NC0000"])
        self.assertEqual(match_completions_mock("00"), ["00 06 30 30 30 30"])
        self.assertEqual(match_completions_mock("B"), ["BA", "BB"])
        self.assertEqual(match_completions_mock("BA"), ["BA"])
        self.assertEqual(match_completions_mock("BB"), ["BB"])
        self.assertEqual(match_completions_mock("C"), ["CA", "CB"])
        self.assertEqual(match_completions_mock("CA"), ["CA"])
        self.assertEqual(match_completions_mock("CB"), ["CB"])
        self.assertEqual(match_completions_mock("M"), ["M0", "M2"])
        self.assertEqual(match_completions_mock("M0"), ["M0"])
        self.assertEqual(match_completions_mock("M2"), ["M2"])

        # Non-matching prefixes
        self.assertEqual(match_completions_mock("xyz"), [])
        self.assertEqual(match_completions_mock("123"), [])
        self.assertEqual(match_completions_mock("exit_extra"), [])

    def test_history_path_resolution_windows(self):
        """Verify Windows history path resolution rules."""
        # Case 1: USERPROFILE is present
        env1 = {"USERPROFILE": "C:\\Users\\alice"}
        self.assertEqual(
            resolve_history_path_mock(env1, platform="win32"),
            "C:\\Users\\alice\\.tcp_client_history"
        )

        # Case 2: USERPROFILE is empty, HOMEDRIVE and HOMEPATH present
        env2 = {"USERPROFILE": "", "HOMEDRIVE": "D:", "HOMEPATH": "\\Users\\bob"}
        self.assertEqual(
            resolve_history_path_mock(env2, platform="win32"),
            "D:\\Users\\bob\\.tcp_client_history"
        )

        # Case 3: None present -> fallback
        env3 = {"USERPROFILE": "", "HOMEDRIVE": "", "HOMEPATH": ""}
        self.assertEqual(
            resolve_history_path_mock(env3, platform="win32"),
            "./.tcp_client_history"
        )

    def test_history_path_resolution_posix(self):
        """Verify POSIX history path resolution rules."""
        # Case 1: HOME is present
        env1 = {"HOME": "/home/alice"}
        self.assertEqual(
            resolve_history_path_mock(env1, platform="linux"),
            "/home/alice/.tcp_client_history"
        )

        # Case 2: HOME is empty or missing -> fallback
        env2 = {"HOME": ""}
        self.assertEqual(
            resolve_history_path_mock(env2, platform="linux"),
            "./.tcp_client_history"
        )
        env3 = {}
        self.assertEqual(
            resolve_history_path_mock(env3, platform="linux"),
            "./.tcp_client_history"
        )

    def test_c_implementation(self):
        """Compile and execute C unit tests for mode_interactive and linenoise."""
        c_test_src = r"""
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "mode_interactive.h"
#include "linenoise.h"
#include "compat.h"

/* Stubs for run_interactive_mode dependencies during unit test */
bool signal_handler_is_interrupted(void) { return false; }
int get_last_socket_error(void) { return 0; }
bool is_socket_wouldblock(int err) { (void)err; return false; }
ssize_t socket_read(int fd, void *buf, size_t count, int timeout_ms) {
    (void)fd; (void)buf; (void)count; (void)timeout_ms;
    return 0;
}
ssize_t socket_write_all(int fd, const void *buf, size_t count, int timeout_ms) {
    (void)fd; (void)buf; (void)count; (void)timeout_ms;
    return (ssize_t)count;
}

static void test_history_path(void) {
    char path[512];
    char *res = get_interactive_history_path(path, sizeof(path));
    assert(res == path);
    assert(strlen(path) > 0);
    assert(strstr(path, ".tcp_client_history") != NULL);
    assert(get_interactive_history_path(NULL, 100) == NULL);
    assert(get_interactive_history_path(path, 0) == NULL);
}

static void test_completions(void) {
    linenoiseCompletions lc = {0, NULL};

    /* NULL buf treated as empty */
    interactive_completion_callback(NULL, &lc);
    assert(lc.len == 14);
    assert(strcmp(lc.cvec[0], "exit") == 0);
    assert(strcmp(lc.cvec[1], "quit") == 0);
    assert(strcmp(lc.cvec[2], "help") == 0);
    assert(strcmp(lc.cvec[3], "clear") == 0);
    assert(strcmp(lc.cvec[4], "status") == 0);
    assert(strcmp(lc.cvec[5], "history") == 0);
    assert(strcmp(lc.cvec[6], "NC0000") == 0);
    assert(strcmp(lc.cvec[7], "00 06 30 30 30 30") == 0);
    assert(strcmp(lc.cvec[8], "BA") == 0);
    assert(strcmp(lc.cvec[9], "BB") == 0);
    assert(strcmp(lc.cvec[10], "CA") == 0);
    assert(strcmp(lc.cvec[11], "CB") == 0);
    assert(strcmp(lc.cvec[12], "M0") == 0);
    assert(strcmp(lc.cvec[13], "M2") == 0);
    linenoiseFreeCompletions(&lc);

    /* "ex" -> "exit" */
    interactive_completion_callback("ex", &lc);
    assert(lc.len == 1);
    assert(strcmp(lc.cvec[0], "exit") == 0);
    linenoiseFreeCompletions(&lc);

    /* "NC" -> "NC0000" */
    interactive_completion_callback("NC", &lc);
    assert(lc.len == 1);
    assert(strcmp(lc.cvec[0], "NC0000") == 0);
    linenoiseFreeCompletions(&lc);

    /* "00" -> "00 06 30 30 30 30" */
    interactive_completion_callback("00", &lc);
    assert(lc.len == 1);
    assert(strcmp(lc.cvec[0], "00 06 30 30 30 30") == 0);
    linenoiseFreeCompletions(&lc);

    /* "B" -> "BA", "BB" */
    interactive_completion_callback("B", &lc);
    assert(lc.len == 2);
    assert(strcmp(lc.cvec[0], "BA") == 0);
    assert(strcmp(lc.cvec[1], "BB") == 0);
    linenoiseFreeCompletions(&lc);

    /* "M" -> "M0", "M2" */
    interactive_completion_callback("M", &lc);
    assert(lc.len == 2);
    assert(strcmp(lc.cvec[0], "M0") == 0);
    assert(strcmp(lc.cvec[1], "M2") == 0);
    linenoiseFreeCompletions(&lc);

    /* Non-existent prefix */
    interactive_completion_callback("nonexistent", &lc);
    assert(lc.len == 0);
    linenoiseFreeCompletions(&lc);

    /* NULL lc safe */
    interactive_completion_callback("ex", NULL);
}

static void test_linenoise_hardening(void) {
    char buf[64];
    linenoiseState l;

    /* 1. Defensive EOF: c == -1 must return -1 */
    linenoiseEditStart(&l, buf, sizeof(buf), "> ");
    assert(linenoiseEditFeed(&l, -1) == -1);
    linenoiseEditStop(&l);

    /* 2. ESC during completion mode cancels completion and sets esc_state */
    linenoiseSetCompletionCallback(interactive_completion_callback);
    linenoiseEditStart(&l, buf, sizeof(buf), "> ");
    assert(linenoiseEditFeed(&l, 'e') == 0);
    assert(linenoiseEditFeed(&l, 'x') == 0);
    /* Tab into completion mode */
    assert(linenoiseEditFeed(&l, '\t') == 0);
    assert(l.in_completion == 1);
    assert(strcmp(l.buf, "exit") == 0);

    /* Feed ESC (27) while in completion mode */
    assert(linenoiseEditFeed(&l, 27) == 0);
    assert(l.in_completion == 0);

    /* Now feed '[' then 'A' (Up arrow) - must NOT insert literal '[' or 'A' */
    assert(linenoiseEditFeed(&l, '[') == 0);
    assert(linenoiseEditFeed(&l, 'A') == 0);
    assert(strchr(l.buf, '[') == NULL);
    linenoiseEditStop(&l);

    /* 3. Windows extended key prefix (0 or 0xE0) during completion mode */
    linenoiseEditStart(&l, buf, sizeof(buf), "> ");
    assert(linenoiseEditFeed(&l, 'e') == 0);
    assert(linenoiseEditFeed(&l, 'x') == 0);
    assert(linenoiseEditFeed(&l, '\t') == 0);
    assert(l.in_completion == 1);

    /* Feed 0xE0 */
    assert(linenoiseEditFeed(&l, (int)(unsigned char)0xE0) == 0);
    assert(l.in_completion == 0);
    linenoiseEditStop(&l);

    linenoiseEditStart(&l, buf, sizeof(buf), "> ");
    assert(linenoiseEditFeed(&l, 'e') == 0);
    assert(linenoiseEditFeed(&l, 'x') == 0);
    assert(linenoiseEditFeed(&l, '\t') == 0);
    assert(l.in_completion == 1);

    /* Feed 0 */
    assert(linenoiseEditFeed(&l, 0) == 0);
    assert(l.in_completion == 0);
    linenoiseEditStop(&l);
}

int main(void) {
    test_history_path();
    test_completions();
    test_linenoise_hardening();
    printf("C UNIT TESTS PASS\n");
    return 0;
}
"""
        tmp_dir = tempfile.mkdtemp()
        try:
            c_file = os.path.join(tmp_dir, "test_interactive_c.c")
            exe_file = os.path.join(tmp_dir, "test_interactive_c.exe")
            with open(c_file, "w", encoding="utf-8") as f:
                f.write(c_test_src)

            # Determine compiler command
            mode_interactive_c = os.path.join(REPO_ROOT, "src", "mode_interactive.c")
            linenoise_c = os.path.join(REPO_ROOT, "src", "linenoise.c")
            include_dir = os.path.join(REPO_ROOT, "src")

            if shutil.which("gcc"):
                compile_cmd = [
                    "gcc", "-I" + include_dir,
                    c_file, mode_interactive_c, linenoise_c,
                    "-o", exe_file
                ]
                if sys.platform == "win32":
                    compile_cmd.append("-lws2_32")
                res = subprocess.run(compile_cmd, capture_output=True, text=True)
                self.assertEqual(res.returncode, 0, f"Compilation failed: {res.stderr}")
                run_res = subprocess.run([exe_file], capture_output=True, text=True)
                self.assertEqual(run_res.returncode, 0, f"Execution failed: {run_res.stderr}")
                self.assertIn("C UNIT TESTS PASS", run_res.stdout)
            elif shutil.which("wsl"):
                # Use WSL cross-compiler or gcc
                # Convert paths to WSL paths
                wsl_repo = REPO_ROOT.replace("\\", "/").replace("d:", "/mnt/d").replace("D:", "/mnt/d").replace("c:", "/mnt/c").replace("C:", "/mnt/c")
                wsl_tmp = tmp_dir.replace("\\", "/").replace("d:", "/mnt/d").replace("D:", "/mnt/d").replace("c:", "/mnt/c").replace("C:", "/mnt/c")
                wsl_c = f"{wsl_tmp}/test_interactive_c.c"
                wsl_exe = f"{wsl_tmp}/test_interactive_c.exe"
                wsl_mode = f"{wsl_repo}/src/mode_interactive.c"
                wsl_line = f"{wsl_repo}/src/linenoise.c"
                wsl_inc = f"{wsl_repo}/src"

                compile_cmd = [
                    "wsl", "x86_64-w64-mingw32-gcc", "-I" + wsl_inc,
                    wsl_c, wsl_mode, wsl_line, "-lws2_32",
                    "-o", wsl_exe
                ]
                res = subprocess.run(compile_cmd, capture_output=True, text=True)
                self.assertEqual(res.returncode, 0, f"WSL compilation failed: {res.stderr}")

                # Execute compiled binary natively on Windows
                run_res = subprocess.run([exe_file], capture_output=True, text=True)
                self.assertEqual(run_res.returncode, 0, f"Execution failed: {run_res.stderr}")
                self.assertIn("C UNIT TESTS PASS", run_res.stdout)
            else:
                self.skipTest("No GCC or WSL found to compile C unit tests")
        finally:
            shutil.rmtree(tmp_dir, ignore_errors=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
