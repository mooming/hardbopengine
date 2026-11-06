// posix_pipe_spawn.c — 파이프 stdout, 비블로킹 배수, 데드라인 대기, 타임아웃 kill의 완결 예제.
// 빌드: cc -std=c11 -Wall -o posix_pipe_spawn posix_pipe_spawn.c
// 사용: ./posix_pipe_spawn <명령> [인자...]
//   ./posix_pipe_spawn /usr/bin/printf "hello from child\n"   (정상 종료 관찰)
//   ./posix_pipe_spawn sleep 10                                (2초 데드라인 kill 관찰)

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char** environ;

static const int DeadlineMs = 2000;
static const int SliceMs = 50;

static int NowMs(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "usage: %s <command> [args...]\n", argv[0]);
		return 2;
	}

	int fds[2];
	if (pipe(fds) != 0)
	{
		perror("pipe");
		return 1;
	}

	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO); // 자식 stdout -> 파이프
	posix_spawn_file_actions_addclose(&actions, fds[0]);               // 자식은 읽기 끝 불필요

	pid_t pid;
	char* const* childArgv = &argv[1];
	const int spawnError = posix_spawnp(&pid, argv[1], &actions, NULL, childArgv, environ);
	posix_spawn_file_actions_destroy(&actions);
	if (spawnError != 0)
	{
		fprintf(stderr, "posix_spawnp: %s\n", strerror(spawnError));
		return 1;
	}

	close(fds[1]);                      // 부모의 쓰기 끝을 지금 닫는다 — 아니면 EOF가 영원히 안 온다
	fcntl(fds[0], F_SETFL, O_NONBLOCK); // 비블로킹 배수

	const int deadline = NowMs() + DeadlineMs;
	char buffer[4096];
	int status = 0;
	int reaped = 0;
	int killed = 0;

	while (1)
	{
		ssize_t bytes;
		while ((bytes = read(fds[0], buffer, sizeof(buffer))) > 0)
		{
			fwrite(buffer, 1, (size_t)bytes, stdout);
		}

		if (bytes == 0) // EOF — 자식이 쓰기 끝을 닫았다 = 이미 종료했다
		{
			waitpid(pid, &status, 0);
			reaped = 1;
			break;
		}

		if (bytes < 0 && errno != EAGAIN)
		{
			perror("read");
			break;
		}

		if (waitpid(pid, &status, WNOHANG) == pid)
		{
			reaped = 1;
			break;
		}

		if (NowMs() >= deadline)
		{
			printf("[runner] deadline (%d ms) exceeded -> SIGKILL\n", DeadlineMs);
			kill(pid, SIGKILL);
			waitpid(pid, &status, 0); // 좀비 회수
			reaped = 1;
			killed = 1;
			break;
		}

		usleep(SliceMs * 1000);
	}

	ssize_t remaining;
	while ((remaining = read(fds[0], buffer, sizeof(buffer))) > 0) // 마지막 배수 (EOF까지)
	{
		fwrite(buffer, 1, (size_t)remaining, stdout);
	}

	if (killed)
	{
		printf("[runner] child was KILLED (SIGKILL)\n");
	}
	else if (WIFEXITED(status))
	{
		printf("[runner] child exited with code %d\n", WEXITSTATUS(status));
	}
	else if (WIFSIGNALED(status))
	{
		printf("[runner] child died from signal %d\n", WTERMSIG(status));
	}
	(void)reaped;

	close(fds[0]);
	return killed ? 3 : 0;
}
