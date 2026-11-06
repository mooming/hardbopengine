// win32_pipe_spawn.cpp — win32_pipe_spawn의 Windows 쌍둥이.
// 파이프 stdout, 비블로킹 배수, 데드라인 대기, 타임아웃 kill의 완결 예제.
// 빌드: cl /EHsc /W3 /utf-8 win32_pipe_spawn.cpp
// 사용: win32_pipe_spawn.exe <명령줄...>
//   win32_pipe_spawn.exe cmd /c echo hello from child   (정상 종료 관찰)
//   win32_pipe_spawn.exe timeout /t 10                   (2초 데드라인 kill 관찰)

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>

static const DWORD DeadlineMs = 2000;
static const DWORD SliceMs = 50;

int wmain(int argc, wchar_t** argv)
{
	if (argc < 2)
	{
		wprintf(L"usage: %s <command line...>\n", argv[0]);
		return 2;
	}

	wchar_t commandLine[32768] = L"";
	for (int i = 1; i < argc; ++i) // 예제 단순화: 따옴표 처리 없이 공백으로 연결
	{
		if (i > 1)
		{
			wcscat_s(commandLine, L" ");
		}
		wcscat_s(commandLine, argv[i]);
	}

	SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
	HANDLE readEnd = nullptr;
	HANDLE writeEnd = nullptr;
	if (!CreatePipe(&readEnd, &writeEnd, &inheritable, 0))
	{
		wprintf(L"CreatePipe failed (%lu)\n", GetLastError());
		return 1;
	}
	if (!SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0)) // 읽기 끝은 부모에만 남긴다
	{
		wprintf(L"SetHandleInformation failed (%lu)\n", GetLastError());
		return 1;
	}

	STARTUPINFOW startupInfo{};
	startupInfo.cb = sizeof(startupInfo);
	startupInfo.dwFlags = STARTF_USESTDHANDLES;
	startupInfo.hStdOutput = writeEnd; // 자식 stdout -> 파이프 쓰기 끝
	startupInfo.hStdError = writeEnd;

	PROCESS_INFORMATION processInfo{};
	if (!CreateProcessW(nullptr, commandLine, nullptr, nullptr, TRUE, 0, nullptr, nullptr,
						&startupInfo, &processInfo))
	{
		wprintf(L"CreateProcessW failed (%lu)\n", GetLastError());
		return 1;
	}
	CloseHandle(writeEnd); // 부모의 쓰기 끝을 지금 닫는다 — 아니면 EOF가 영원히 안 온다

	const ULONGLONG deadline = GetTickCount64() + DeadlineMs;
	bool killed = false;
	bool exited = false;

	while (true)
	{
		DWORD available = 0;
		while (PeekNamedPipe(readEnd, nullptr, 0, nullptr, &available, nullptr) && available > 0)
		{
			char buffer[4096];
			DWORD bytesRead = 0;
			if (!ReadFile(readEnd, buffer, sizeof(buffer), &bytesRead, nullptr))
			{
				break;
			}
			fwrite(buffer, 1, bytesRead, stdout);
		}

		const DWORD wait = WaitForSingleObject(processInfo.hProcess, SliceMs);
		if (wait == WAIT_OBJECT_0)
		{
			exited = true;
			break;
		}

		if (GetTickCount64() >= deadline)
		{
			wprintf(L"[runner] deadline (%lu ms) exceeded -> TerminateProcess\n", DeadlineMs);
			TerminateProcess(processInfo.hProcess, 1);
			WaitForSingleObject(processInfo.hProcess, 1000);
			killed = true;
			break;
		}
	}

	for (;;) // 마지막 배수 (EOF까지; ERROR_BROKEN_PIPE = EOF)
	{
		char buffer[4096];
		DWORD bytesRead = 0;
		if (!ReadFile(readEnd, buffer, sizeof(buffer), &bytesRead, nullptr) || bytesRead == 0)
		{
			break;
		}
		fwrite(buffer, 1, bytesRead, stdout);
	}

	DWORD exitCode = 0;
	GetExitCodeProcess(processInfo.hProcess, &exitCode);

	if (killed)
	{
		wprintf(L"[runner] child was KILLED (exit code %lu)\n", exitCode);
	}
	else
	{
		wprintf(L"[runner] child exited with code %lu\n", exitCode);
	}
	(void)exited;

	CloseHandle(processInfo.hProcess);
	CloseHandle(processInfo.hThread);
	CloseHandle(readEnd);
	return killed ? 3 : 0;
}
