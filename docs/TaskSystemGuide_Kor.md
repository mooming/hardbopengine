# TaskSystem 가이드

TaskSystem은 여러 스트림에 걸쳐 워커 스레드와 작업 분배를 관리하는 멀티스레드 작업 실행 프레임워크입니다.

## 아키텍처 개요

```
TaskSystem
├── 메인 스트림 (인덱스 0) - 메인 애플리케이션 스레드
├── IO 스트림 (인덱스 1)   - I/O 작업
└── 워커 스트림            - 병렬 작업 실행을 위한 워커 스레드
```

## 핵심 구성 요소

### 1. Task (`Task.h`)
작업(Task)은 병렬 실행을 위해 여러 하위 작업으로 나눌 수 있는 작업 단위를 나타냅니다.

**주요 특징:**
- 사용자 데이터를 포함하는 실행 가능한 함수를 포함합니다.
- 하위 작업의 개수와 완료된 하위 작업의 개수를 추적합니다.
- 병렬 처리를 위한 범위 지정 하위 작업을 생성할 수 있습니다.
- Busy-wait 및 인터벌 기반 대기 메서드를 제공합니다.

**사용법:**
```cpp
auto func = [](void* userData, std::size_t start, std::size_t end) -> std::size_t {
    // 범위 [start, end) 처리
    return end - start;
};

Task task("MyTask", func, &userData);
task.Start(numSubTasks, startIndex, endIndex);
task.BusyWait();  // 완료될 때까지 대기
```

### 2. RangedTask (`RangedTask.h`)
특정 범위 `[start, end)`를 가진 하위 작업입니다. 각 RangedTask는 부모 Task에 속합니다.

**주요 특징:**
- 우선순위, 어피니티(affinity) 및 범위 정보를 포함합니다.
- 실행 중 현재 인덱스를 추적합니다.
- 완료 시 부모 Task에 자동으로 보고합니다.

### 3. TaskStream (`TaskStream.h`)
RangedTask 큐를 처리하는 단일 스레드입니다.

**주요 특징:**
- 작업 실행을 위한 전용 스레드를 소유합니다.
- 작업 스케줄링을 위해 우선순위 큐를 사용합니다.
- 작업 어피니티를 지원합니다 (작업을 특정 스트림에 바인딩할 수 있음).
- 효율적인 메모리 할당을 위해 MultiPoolAllocator를 사용합니다.

**동작:**
- 큐가 비어 있을 때 조건 변수(condition variable)에서 대기합니다.
- 우선순위 순서대로 작업을 처리합니다.
- 긴급한 작업을 위한 깨우기(wake-up) 신호를 지원합니다.

### 4. TaskStreamAffinity (`TaskStreamAffinity.h`)
어떤 작업 스트림이 작업을 처리했는지 추적하는 템플릿 클래스입니다.

**구현:**
- 64비트 비트 배열을 사용합니다.
- `Set(bitIndex)`는 해당 스트림이 작업을 처리했음을 표시합니다.
- `Get(bitIndex)`는 해당 스트림이 작업을 처리했는지 확인합니다.

### 5. TaskSystem (`TaskSystem.h`)
모든 작업 스트림을 관리하는 메인 시스템입니다.

**책임:**
- 하드웨어 동시성(hardware concurrency)을 기반으로 워커 스레드를 생성하고 관리합니다.
- 메인 스트림(인덱스 0)과 IO 스트림(인덱스 1)을 제공합니다.
- 우선순위 스케줄링이 적용된 글로벌 작업 큐를 처리합니다.
- 스트림 이름 및 인덱스를 위한 스레드 로컬 저장소(thread-local storage)를 관리합니다.

**정적 메서드:**
```cpp
TaskSystem::GetNumHardwareThreads();       // CPU 코어 수 가져오기
TaskSystem::IsMainThread();                 // 메인 스레드인지 확인
TaskSystem::IsIOThread();                    // IO 스레드인지 확인
TaskSystem::GetCurrentStreamIndex();        // 현재 스트림 인덱스 가져오기
TaskSystem::GetCurrentThreadName();         // 스레드 이름 가져오기
```

## 작업 분배 (Task Distribution)

### 스트림 전용 작업
작업을 특정 스트림에 인큐(enqueue)할 수 있습니다:
```cpp
taskSystem.Enqueue(streamIndex, rangedTask);
```

### 글로벌 작업 큐
낮은 우선순위의 작업은 글로벌 큐에 추가될 수 있습니다:
```cpp
taskSystem.Enqueue(task);  // 글로벌 우선순위 큐에 추가됨
```

### 어피니티 기반 디큐 (Affinity-Based Dequeue)
글로벌 큐에서 디큐할 때, 스트림 기아 상태(starvation)를 방지하기 위해 작업에 자동으로 어피와티를 할당할 수 있습니다:
```cpp
std::optional<RangedTask> task;
taskSystem.Dequeue(task);  // 스트림 어피니티가 있는 작업을 가져옴
```

## 사전 정의된 스트림

| 인덱스 | 이름  | 용도                                     |
|-------|-------|------------------------------------------|
| 0     | Main  | 메인 애플리케이션 스레드                  |
| 1     | IO    | I/O 작업                                 |
| 2+    | Worker| 병렬 작업 실행                          |

## 사용 예시

### 작업 생성 및 실행
```cpp
auto& taskSystem = engine.GetTaskSystem();

auto computeFunc = [](void* data, std::size_t start, std::size_t end) -> std::size_t {
    auto* result = static_cast<double*>(data);
    for (std::size_t i = start; i < end; ++i) {
        *result += 1.0 / (i + 1);
    }
    return end - start;
};

Task task("Compute", computeFunc, &result);

// 1,000,000개를 10개의 하위 작업으로 분할
constexpr std::size_t count = 1000000;
constexpr std::size_t numSubtasks = 10;
constexpr std::size_t increment = count / numSubtasks;

for (std::size_t i = 0; i < count; i += increment) {
    taskSystem.Enqueue(task.GenerateSubTask(i, i + increment));
}

task.BusyWait();  // 모든 하위 작업이 완료될 때까지 대기
```

### 특정 스트림에 인큐하기
```cpp
// I/O 바운드 작업을 위해 IO 스트림에 인큐
taskSystem.Enqueue(TaskSystem::GetIOTaskStreamIndex(), rangedTask);

// 워커 스트림에 인큐
taskSystem.Enqueue(2, rangedintTask);
```

## 스레드 안전성 (Thread Safety)

- 작업 큐는 동기화를 위해 `std::mutex`를 사용합니다.
- 조건 변수가 스레드의 수면/깨우기를 조정합니다.
- `std::atomic`이 작업 완료를 추적합니다.
- 스레드 로컬 저장소에 스레드별 스트림 인덱스와 이름이 저장됩니다.

## 메모리 관리

- TaskStream은 효율적인 작업 할당을 위해 `MultiPoolAllocator`를 사용합니다.
- 빈번한 작업 생성/파괴에 대한 할당 오버헤드를 줄입니다.

## 엔진 통합

TaskSystem은 Engine에 의해 초기화됩니다:
```cpp
auto& engine = Engine::Get();
auto& taskSystem = engine.GetTaskSystem();

taskSystem.Initialize();
// ... 작업 실행 ...
taskSystem.RequestShutDown();
taskSystem.JoinAndClean();  // 모든 스트림이 종료될 때까지 대기
```

## 테스트

`TaskSystem.cpp`의 유닛 테스트에는 다음 내용이 포함됩니다:
- 빈 작업 검증
- 크기가 0인 작업 처리
- 병렬 계산 (Bagel 문제)
- 증분적 작업 처리
