# PLAN — Multi-Process Test Harness

Status: **DRAFT — awaiting owner approval**

## 1. Goal

Replace the single-process, task-chain-driven test suite with a multi-process harness:
the runner spawns one child `EngineTest` process per testlet, enforces a timeout
(16 s default, per-testlet overridable), kills hung children, and aggregates results.
The engine gains process management as an OSAL capability (`OSProcess`); the test
harness is its first consumer.

## 2. Confirmed Decisions

| # | Decision | Choice |
| --- | --- | --- |
| D1 | Isolation granularity | Per testlet (~429 short-lived processes) |
| D2 | Spawn mechanism | Re-exec the same binary with an argv selector |
| D3 | Runner location | Runner mode inside `EngineTest` (no new target) |
| D4 | Platforms | Linux + macOS + Windows, all verified |
| D5 | Old single-process mode | Deleted — multi-process only |
| D6 | Timeout | 16 s default, overridable per testlet |
| D7 | Child output | Pipe-first: streams drained by the runner (`File` mode kept as an option) |

**Risk (from D4):** no Windows host is available in this session (journal precedent).
`WindowsProcess.cpp` will be written to the same contract and compile-verified via
the feature-macro proof where possible; a real Windows run by the owner closes it.
Recorded as an open gate, not silently passed.

## 3. Architecture

```
EngineTest (no flag)  =  RUNNER
  PrepareTestlets()            (engine-free: labels + counts)
  for each testlet N:
      spawn child: EngineTest --testlet N (stdout/stderr as pipes)
      loop: drain child pipes -> WaitForProcess(50 ms slice)
      deadline (16 s default, per-testlet override) exceeded:
          kill child, final drain, record "TIMEOUT"
  aggregate exit codes -> report (pass=/fail=/invalid= format preserved)

EngineTest --testlet N  =  CHILD
  Engine::Initialize
  schedule [RunTestlet N, ReportAndShutDown] on base stream (2-task chain)
  Engine::Run
  exit 0 / 1
```

## 4. Work Items

### 4.1 OSProcess (new OSAL capability)
- `Engine/OSAL/OSProcess.h` — free functions in `OS` namespace, following the
  `OSThread` pattern; `OSProcessTest` collection declared in the header.
- `Engine/OSAL/OSProcess.cpp` — common code + `OSProcessTest::Prepare`.
- `LinuxProcess.cpp` (posix_spawn family), `OSXProcess.cpp`, `WindowsProcess.cpp`
  (`CreateProcess`/`WaitForSingleObject`/`TerminateProcess`/`GetExitCodeProcess`).
- API surface (exact signatures fixed during implementation, contract in docs):
  spawn (path, args, per-stream redirection: inherit / file / pipe), a
  slice-wait returning state + exit code, kill, handle close (Windows HANDLE
  hygiene).
- **API design (pipe-first, proposal):**

  Types:

  | Type | Contract |
  | --- | --- |
  | `EProcessStream` | `StdOut` / `StdErr` — selects which child stream an operation addresses. |
  | `EStreamMode` | `Inherit` / `File` / `Pipe` — per-stream redirection mode inside `ProcessSpawnDesc`. |
  | `ProcessSpawnDesc` | `executablePath`, `arguments`, per-stream `EStreamMode`, file path for `File` mode. |
  | `ProcessPipe` | One pipe: non-blocking `Read(buffer, maxBytes)` answering `>0` bytes read, `0` nothing available, `-1` EOF (child closed its end); `CloseReadEnd()`. |
  | `ProcessHandle` | Native handle (`pid_t` / `HANDLE`) plus the stdout/stderr read-end `ProcessPipe`s (valid only in `Pipe` mode); `IsValid()`, `GetStdoutPipe()`, `GetStderrPipe()`. |
  | `EProcessWaitResult` | `Exited` / `TimedOut` / `Failed`. |

  Functions:

  | Function | Contract |
  | --- | --- |
  | `ProcessHandle SpawnProcess(const ProcessSpawnDesc& desc) noexcept` | Spawns a child per `desc`; in `Pipe` mode the child receives the write ends and **the parent's write-end copies are closed inside the spawn** — left open, EOF never arrives. In `File` mode the stream is redirected to the given path. Returns an invalid handle on failure. |
  | `EProcessWaitResult WaitForProcess(ProcessHandle& handle, uint32_t timeoutMilliSecs, int& exitCode) noexcept` | A **time slice, not a blocking wait**: waits up to `timeoutMilliSecs`, then answers `Exited` (exit code written), `TimedOut` (child still running), or `Failed`. The slice exists so the caller can drain the pipes between waits. |
  | `void KillProcess(ProcessHandle& handle) noexcept` | Force-terminates (`SIGKILL` / `TerminateProcess`). Must be followed by a reaping `WaitForProcess` (zombie) and a final drain. |
  | `void CloseProcessHandle(ProcessHandle& handle) noexcept` | Closes the pipes and the native handle (`CloseHandle` on Windows; on POSIX reaping is `WaitForProcess`'s job and this releases the rest). |

  The drain loop is the reason `WaitForProcess` is a slice: a pipe's kernel
  buffer (~64 KB on POSIX) fills if nobody reads, the child's writes block,
  and the runner then kills a child that was only waiting for the runner — a
  false timeout. The runner's loop: drain pipes → `WaitForProcess(50 ms)` →
  repeat until `Exited`; deadline reached → `KillProcess` → final drain.

  Examples (illustrative; exact signatures fixed during implementation):

  The full runner cycle — spawn with pipes, drain-while-wait, kill on
  deadline, close:

  ```cpp
  OS::ProcessSpawnDesc desc;
  desc.executablePath = engineTestPath;
  desc.arguments = {"EngineTest", "--testlet", "37"};
  desc.stdoutMode = OS::EStreamMode::Pipe;
  desc.stderrMode = OS::EStreamMode::Pipe;

  OS::ProcessHandle child = OS::SpawnProcess(desc);
  if (!child.IsValid())
  {
  	RecordSpawnFailure(testletIndex);
  	return;
  }

  int exitCode = 0;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(16);
  OS::EProcessWaitResult result = OS::EProcessWaitResult::Failed;

  while (true)
  {
  	AppendToLog(testletIndex, DrainToString(child.GetStdoutPipe()), DrainToString(child.GetStderrPipe()));

  	result = OS::WaitForProcess(child, 50, exitCode);
  	if (result != OS::EProcessWaitResult::TimedOut)
  	{
  		break;
  	}

  	if (std::chrono::steady_clock::now() >= deadline)
  	{
  		OS::KillProcess(child);                    // SIGKILL / TerminateProcess
  		OS::WaitForProcess(child, 1000, exitCode); // reap the zombie
  		AppendToLog(testletIndex, DrainToString(child.GetStdoutPipe()), DrainToString(child.GetStderrPipe()));
  		break;
  	}
  }

  OS::CloseProcessHandle(child);
  ```

  The drain helper — `ProcessPipe::Read`'s three-way answer in use:

  ```cpp
  std::string DrainToString(OS::ProcessPipe& pipe)
  {
  	std::string out;
  	char buffer[4096];
  	while (true)
  	{
  		const int bytes = pipe.Read(buffer, sizeof(buffer));
  		if (bytes <= 0)
  		{
  			break; // 0 = nothing available, -1 = EOF
  		}
  		out.append(buffer, static_cast<std::size_t>(bytes));
  	}
  	return out;
  }
  ```

  File mode — the same spawn, output redirected to a per-testlet log:

  ```cpp
  OS::ProcessSpawnDesc desc;
  desc.executablePath = engineTestPath;
  desc.arguments = {"EngineTest", "--testlet", "37"};
  desc.stdoutMode = OS::EStreamMode::File;
  desc.stderrMode = OS::EStreamMode::File;
  desc.stdoutFilePath = "build/Applications/EngineTest/Dev/testlet-logs/037.log";
  desc.stderrFilePath = "build/Applications/EngineTest/Dev/testlet-logs/037.log";
  ```

  One handle type, no separate id type: on POSIX `pid_t` is both the id and
  the handle, and on Windows the `HANDLE` obtained at spawn answers every
  call this API makes (the numeric id remains an implementation detail,
  retrievable via `GetProcessId` if a diagnostic ever needs it). The runner
  never consumes a raw id, so the API does not expose one.
- Docs: one page per function, class page for the test collection, design
  document `docs/design/OSProcess_Design.html` (platform mapping, handle
  lifecycle, zombie/HANDLE-leak avoidance, timeout semantics).

### 4.2 EngineTest modes
- `TestMain.cpp`: argv dispatch — no flag → runner; `--testlet N` → child.
- `UnitTestCollection.cpp`: delete the whole-suite task chain
  (`PostSuiteTask`, `RunTestletTask`, `ReportAndShutDownTask`,
  `ScheduleSuiteOnBaseStream`); add the minimal 2-task child schedule.
- `TestEnv`: delete `testletRunCounts`, `NoteTestletRepeated`,
  `suiteDrivenByShutdownPump`, `NoteSuiteDrivenByShutdownPump` (chain-only
  machinery). Runner aggregates from child exit codes.

### 4.3 Gate script
- `check.sh` test layer: invoke the runner (no flag), keep the
  `pass=/fail=/invalid=` parse, drop the now-redundant 300 s outer timeout
  (the runner enforces 16 s per testlet itself).

### 4.4 Docs & standards
- Every new entry: `/// API reference` pointer + page, signatures verbatim.
- New code written comment-free and layout-clean from the start.

## 5. Verification

| Check | Proof |
| --- | --- |
| Build | `./build.sh Applications/EngineTest -dev -debug -release -test` — 3 configs clean |
| Suite | Runner mode in Dev, Debug, Release: 66 collections / 429 testlets, exit 0 |
| Timeout kill (red-first) | Inject a sleeping testlet → runner kills it at the deadline, reports TIMEOUT, continues; exit 1 |
| Crash isolation (red-first) | Inject an aborting testlet → suite continues, child crash recorded; exit 1 |
| Coverage gates | `docs_coverage.py`, `docs_methods.py`, `htmlcheck.py`, `comments.py`, `layout.py`, `blank_lines.py`, `includes.py` over touched files |
| Windows | Owner-hosted run pending (risk, see D4) |

## 6. Commits

1. `feat(osal)` — OSProcess API + platform impls + docs + design document
2. `feat(test)` — runner/child modes, task-chain deletion, TestEnv trim
3. `chore(scripts)` — check.sh test layer on runner mode
4. `docs(test)` — harness design record, JOURNAL entry

## 7. Open Items for the Owner

- Per-testlet timeout override syntax: proposal `--timeout <seconds>` for the
  default and `--timeout-for <testletIndex>=<seconds>` (repeatable) — confirm or amend.
- Slow testlets to seed with overrides (TC13/TC17 were load-sensitive).

## 8. Appendix — Process APIs by Platform (Reference)

Reference material for implementing `OSProcess`. The mapping table is the
working contract; the per-platform tables are the raw material behind it.

### 8.1 Contract → platform mapping

| `OSProcess` call | Linux / macOS (POSIX) | Windows (Win32) |
| --- | --- | --- |
| `SpawnProcess` | `posix_spawn` with `posix_spawn_file_actions_adddup2`/`addopen` for redirection | `CreateProcessW` with `STARTF_USESTDHANDLES` in `STARTUPINFOW` |
| `WaitForProcess` (bounded) | `waitpid(pid, &status, WNOHANG)` in a deadline loop (blocking `waitpid` cannot honor a timeout) | `WaitForSingleObject(hProcess, ms)` — one call honors the timeout |
| exit code extraction | `WIFEXITED`/`WEXITSTATUS`, `WIFSIGNALED`/`WTERMSIG` on the status | `GetExitCodeProcess` |
| `TimedOut` detection | `waitpid` returns `0` with `WNOHANG` | `WaitForSingleObject` returns `WAIT_TIMEOUT` |
| `KillProcess` | `kill(pid, SIGKILL)` | `TerminateProcess(hProcess, exitCode)` |
| `CloseProcessHandle` | no-op — reaping is `waitpid`'s job | `CloseHandle(hProcess)` |
| pipe creation (`Pipe` mode) | `pipe(fds)` | `CreatePipe` |
| non-blocking pipe read | `fcntl(fd, F_SETFL, O_NONBLOCK)` then `read` (`EAGAIN` = nothing available) | `PeekNamedPipe` (available bytes) then `ReadFile` |
| EOF detection | `read()` returns `0` | `ReadFile` fails with `ERROR_BROKEN_PIPE` |
| parent write-end cleanup | `close(writeFd)` immediately after spawn | parent's write-handle copy `CloseHandle`d after spawn |

### 8.2 POSIX (Linux / macOS)

**Functions**

| Function | Role |
| --- | --- |
| `fork()` | Duplicates the calling process; returns twice (0 in child, pid in parent). |
| `execve` / `execvp` / family | Replaces the process image; returns only on failure. |
| `posix_spawn(pid, path, actions, attr, argv, envp)` | fork+exec combined; the preferred spawn on macOS, glibc ≥ 2.24 on Linux. |
| `posix_spawn_file_actions_init/addopen/adddup2/addclose/destroy` | Declares the child-side fd operations (stdout/stderr redirection). |
| `posix_spawnattr_init/destroy` | Spawn attributes (signal mask, sched, flags). |
| `pipe(int fds[2])` | Anonymous pipe; `fds[0]` read end, `fds[1]` write end — the child inherits the write end. |
| `fcntl(fd, F_SETFL, O_NONBLOCK)` | Makes a pipe fd non-blocking so `read` answers instead of hanging. |
| `waitpid(pid, &status, options)` | Waits and reaps; `WNOHANG` makes it non-blocking (returns 0 while running). |
| `waitid` / `wait` | Alternative wait forms. |
| `kill(pid, sig)` | Sends a signal; `SIGKILL` cannot be caught or ignored. |
| `getpid` / `getppid` | Self / parent id. |
| `_exit()` | Child-side exit that skips atexit handlers and stdio flush. |
| `popen(command, mode)` / `pclose` | Convenience wrapper: pipe + fork + exec of a **shell** (`sh -c`), returning a stdio `FILE*`. Returns no pid — no kill, no timeout, no stderr separation — which is why this harness does not use it; its internals are the same pipe/fork/exec/wait sequence `posix_spawn` composes. |

**Types and objects**: `pid_t` (process id and the only handle), `sigset_t`,
`posix_spawn_file_actions_t`, `posix_spawnattr_t`, wait status `int` decoded by
the `W*` macros, pipe file-descriptor pairs.

**Caveats**

- Between `fork()` and `exec()` only async-signal-safe calls are legal — a
  multi-threaded parent makes anything else undefined; this is why the plan
  spawns from an engine-free runner and prefers `posix_spawn`.
- A killed child is a zombie until reaped by `waitpid` — `KillProcess` must
  always be followed by a `WaitForProcess` that reaps, even on the timeout path.
- `SIGKILL`-terminated children surface as `WIFSIGNALED`, not as an exit code.

**Example — spawn with a piped stdout, drain, wait, kill (C, POSIX):**

A complete, compiled-and-run version of this example lives at
`Examples/OSProcessPipeExample/posix_pipe_spawn.c` (standalone, engine-free;
built and run via `run.sh` on this host: normal-exit and deadline-kill runs
both verified), and the Windows twin at
`Examples/OSProcessPipeExample/win32_pipe_spawn.cpp` with its `run.bat`
(compile-run pending a Windows host).

```c
int fds[2];
pipe(fds);

posix_spawn_file_actions_t actions;
posix_spawn_file_actions_init(&actions);
posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO); // child stdout -> pipe
posix_spawn_file_actions_addclose(&actions, fds[0]);               // child needs no read end

posix_spawnattr_t attr;
posix_spawnattr_init(&attr);

pid_t pid;
const char* argv[] = {"EngineTest", "--testlet", "37", NULL};
posix_spawn(&pid, "/path/EngineTest", &actions, &attr, (char* const*)argv, environ);
posix_spawn_file_actions_destroy(&actions);
posix_spawnattr_destroy(&attr);

close(fds[1]);                       // parent's write end closed NOW, or EOF never comes
fcntl(fds[0], F_SETFL, O_NONBLOCK);  // non-blocking drain

char buffer[4096];
const ssize_t bytes = read(fds[0], buffer, sizeof(buffer)); // >0 data, -1+EAGAIN none, 0 EOF

int status = 0;
const pid_t done = waitpid(pid, &status, WNOHANG); // 0 = still running
if (done == pid && WIFEXITED(status))
{
	const int exitCode = WEXITSTATUS(status);
}

kill(pid, SIGKILL);        // on deadline
waitpid(pid, &status, 0);  // reap the zombie
```

### 8.3 Windows (Win32)

**Functions**

| Function | Role |
| --- | --- |
| `CreateProcessW` | Creates a process and its primary thread; fills `PROCESS_INFORMATION` (`hProcess`, `hThread`, `dwProcessId`). |
| `WaitForSingleObject(hProcess, ms)` | Bounded wait; returns `WAIT_OBJECT_0` / `WAIT_TIMEOUT` / `WAIT_FAILED`. |
| `GetExitCodeProcess(hProcess, &code)` | Reads the exit code; `STILL_ACTIVE` (259) means not yet exited. |
| `TerminateProcess(hProcess, code)` | Force-terminates; asynchronous — the caller still waits and closes. |
| `OpenProcess` | Opens a handle from a numeric id (not needed by this API; id stays internal). |
| `GetProcessId(hProcess)` | Numeric id from a handle, if a diagnostic ever needs it. |
| `CloseHandle` | Releases the process (and thread) handles. |
| `SetHandleInformation` / `SECURITY_ATTRIBUTES` | Makes the redirection file handles inheritable. |
| `CreatePipe` | Anonymous pipe; returns a read and a write `HANDLE` — the child inherits the write end. |
| `PeekNamedPipe` / `ReadFile` | Non-blocking drain: peek the available bytes, then read them; `ERROR_BROKEN_PIPE` means EOF. |
| `ExitProcess` | Self-side exit. |
| `_popen` / `_pclose` | CRT twin of `popen` — runs the command through `cmd.exe /c`; same limitations (no handle, no kill, no timeout, no stderr separation). |

**Types and objects**: `HANDLE` (per-object kernel handle; the process handle
is what this API stores), `DWORD` (id and exit code), `STARTUPINFOW`,
`PROCESS_INFORMATION`, `SECURITY_ATTRIBUTES`, pipe `HANDLE` pairs.

**Caveats**

- `STILL_ACTIVE` (259) is a valid exit code a child could legitimately return,
  so "has it exited?" must come from the wait result, never from the exit code.
- `TerminateProcess` kills the process but not its cleanup handlers; the
  `hThread` from `PROCESS_INFORMATION` must also be closed or it leaks.
- Output redirection requires: file handles created inheritable
  (`bInheritHandle = TRUE` or `SetHandleInformation`),
  `STARTF_USESTDHANDLES` set, and `bInheritHandles = TRUE` on `CreateProcessW`.

**Example — piped stdout with full redirection plumbing (C++, Win32):**

```cpp
SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
HANDLE readEnd = nullptr;
HANDLE writeEnd = nullptr;
CreatePipe(&readEnd, &writeEnd, &sa, 0);
SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0); // read end stays with the parent

STARTUPINFOW si{};
si.cb = sizeof(si);
si.dwFlags = STARTF_USESTDHANDLES;
si.hStdOutput = writeEnd; // child stdout -> pipe write end
si.hStdError = writeEnd;

PROCESS_INFORMATION pi{};
CreateProcessW(L"...\\EngineTest.exe", commandLine, nullptr, nullptr, TRUE,
	0, nullptr, nullptr, &si, &pi);
CloseHandle(writeEnd); // parent's write end closed NOW, or EOF never comes

DWORD available = 0;
DWORD bytesRead = 0;
char buffer[4096];
if (PeekNamedPipe(readEnd, nullptr, 0, nullptr, &available, nullptr) && available > 0)
{
	ReadFile(readEnd, buffer, sizeof(buffer), &bytesRead, nullptr); // ERROR_BROKEN_PIPE = EOF
}

const DWORD wait = WaitForSingleObject(pi.hProcess, 50); // WAIT_OBJECT_0 / WAIT_TIMEOUT
if (wait == WAIT_OBJECT_0)
{
	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);
}

TerminateProcess(pi.hProcess, 1);               // on deadline
WaitForSingleObject(pi.hProcess, 1000);
CloseHandle(pi.hProcess);
CloseHandle(pi.hThread);
CloseHandle(readEnd);
```

## 9. Appendix — IPC Mechanisms (Reference)

Why the harness uses exactly one IPC mechanism, and what the alternatives
would have bought. Sources: mechanism taxonomy (Wikipedia, POSIX),
kernel-involvement analysis (opencsf IPC models), latency orderings
(UDS ~5–10 µs, TCP loopback ~15–30 µs).

### 9.1 Mechanism comparison

| Mechanism | POSIX | Windows | Direction | Kernel per exchange | Latency (order) | Harness fit |
| --- | :-: | :-: | --- | --- | --- | --- |
| Anonymous pipe | `pipe(2)` | `CreatePipe` | one-way | data copy | µs | ✅ **chosen** |
| Named pipe / FIFO | `mkfifo` | named pipes (different API) | one-way | data copy | µs | ✖ no unrelated-process need |
| Unix domain socket | ✅ | recent Windows only | two-way | data copy | ~5–10 µs | ✖ two-way not needed |
| TCP (loopback) | ✅ | ✅ | two-way | full network stack | ~15–30 µs | ✖ |
| Message queue | System V / POSIX mq | ⚠️ partial | discrete messages | per message | µs | ✖ |
| Shared memory | `shm` / `memfd` | file mapping | byte buffer | **none after mapping** | fastest | ✖ no bulk data to share |
| Memory-mapped file | `mmap` | `MapViewOfFile` | byte buffer | page faults only | — | ✖ |
| Signal | `kill` | ✖ no direct equivalent | control only | minimal | cheapest control plane | only `KillProcess`'s `SIGKILL` |

### 9.2 Findings behind the harness decision

- Message-passing mechanisms (pipe, socket, message queue) involve the kernel
  in every exchange — data is copied through kernel memory; shared memory
  performs no syscalls after mapping. The harness exchanges no bulk data, so
  shared memory's advantage is irrelevant here.
- An anonymous pipe is usable only between related processes (parent/child) —
  exactly the runner/child topology; named pipes exist for unrelated processes.
- The harness's entire IPC need is: one-way byte stream (child output), an
  exit code (the wait answer), and force-termination (the kill signal). All
  three are answered by pipe + `waitpid`/`GetExitCodeProcess` +
  `SIGKILL`/`TerminateProcess`, so two-way mechanisms (UDS, sockets),
  zero-copy mechanisms (shared memory), and decoupled queues (message queues)
  buy nothing while costing portability and code.
