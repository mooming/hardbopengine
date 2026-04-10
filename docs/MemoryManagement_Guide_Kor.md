# HardBop Engine 메모리 관리 가이드

HardBop Engine의 메모리 관리 문서에 오신 것을 환영합니다. 이 가이드는 고성능, 저지연 엔진 작업을 위해 설계된 정교한 메모리 관리 시스템에 대한 개요를 제공합니다.

## 개요

HardBop Engine은 계층화된 다중 레이어 메모리 아키텍처를 채택하고 있습니다. 범용 시스템 힙에만 의존하는 대신, 엔진은 파편화를 최소화하고 할당 오버헤드를 줄이며 결정론적 성능을 제공하기 위해 특수화된 할당기(allocator)를 사용합니다.

모든 메모리 작업의 중앙 권한은 `MemoryManager` 싱글톤입니다.

## 핵심 구성 요소

### 1. `MemoryManager` (조율자)

`MemoryManager`는 모든 메모리 관련 활동의 중심 허브 역할을 합니다.
- **중앙 집중식 레지스트리**: `TAllocatorID`를 통해 등록된 모든 할당기를 추적합니다.
- **수명 주기 관리**: 객체의 타입 안전한 생성(`New<T>`) 및 파괴(`Delete<T>`)를 제공합니다.
- **추적 및 통계**: 등록된 모든 할당기에 대해 할당/해제 횟수, 사용량 및 용량을 실시간으로 추적합니다.
- **폴백 메커니즘(Fallthrough Mechanism)**: 할당기가 고갈되었을 때를 처리하기 위한 계층적 폴백 시스템입니다.
- **구성 지속성**: `MultiProfileConfigCache`를 통해 `MultiPool` 구성을 로드하고 저장합니다.
- **프로파일링 지원**: `PROFILE_ENABLED`가 활성화되어 있을 때 메모리 조사, 로깅 및 상세 사용량 보고를 위한 내장 훅을 제공합니다.

### 2. 할당 전략

| 할당기 유형 | 최적의 사용 사례 | 특징 |
| :--- | :--- | :--- |
| **`SystemAllocator`** | 드물고, 크거나, 오래 지속되는 할당. | OS 수준의 `malloc`/`free` 또는 `VirtualAlloc`을 래핑합니다. |
| **`PoolAllocator`** | 동일한 크기의 고빈도 할당. | 고정 크기 블록, 매우 빠른 $O(1)$ 복잡도. |
| **`MultiPoolAllocator`**| 범위 내에서 다양한 할당 크기. | 다양한 블록 크기를 가진 여러 풀을 관리합니다. 최적화된 구성 캐싱 및 뱅크 확장 기능을 갖추고 있습니다. |
| **`StackAllocator`** | 임시적이고 수명이 짧은 스코프 기반 데이터. | 버퍼로부터 선형 할당; 개별 해제가 불가능합니다. |
| **`MonotonicAllocator`**| Arena 스타일, 수명에 종속된 할당. | 빠르고 추가 전용(append-only)인 할당; 한꺼번에 모두 해제됩니다. |
| **`InlinePoolAllocator`**| 매우 작고 스택에 상주하는 풀. | 힙 오버헤드가 없음; 풀 자체를 위해 스택 메모리를 사용합니다. |

### 3. 계층적 스코프 할당 (RAII 접근 방식)

엔진은 `ScopedAllocator`와 `AllocatorScope`를 사용하여 할당 컨텍텍스트를 자동으로 관리합니다. 이를 통해 매우 효율적인 계층적 할당 패턴이 가능해집니다:

**패턴**: 관리되는 스코프 내에 할당기를 중첩할 수 있습니다. 예를 들어, `StackAllocator`가 스코프에 할당될 수 있으며, 해당 스코프 내에서 엔진은 하위 할당을 관리하기 위해 `InlinePoolAllocator` 또는 `MultiPoolAllocator`를 사용할 수 있습니다. 스코프를 벗어나면 컨텍텍스트는 자동으로 이전 할당기로 되돌아갑니다.

```cpp
{
    // 1. 프레임/태스크를 위한 고성능 스택 할당기 생성
    hbe::StackAllocator taskAllocator("TaskBuffer", 1024 * 64);

    // 2. 모든 'New' 호출을 이 스택 할당기로 리다이렉트하는 스코프 진입
    hbe::ScopedAllocator scope(taskAllocator);

    // 3. 이 스코프 내에서, 고갈될 경우 현재 스코프의 할당기로 
    //    폴백할 수 있는 다른 특수화된 할당기를 사용합니다.
    auto* data = hbe::MemoryManager::GetInstance().New<MyType>();
    
} // 4. 컨텍텍스트가 자동으로 이전 할당기(예: System)로 되돌아갑니다
```

## 디버깅 및 성능 모니터링

`MemoryManager`는 메모리 오염을 디버깅하고 성능을 최적화하는 데 도움이 되도록 `Engine/Config/BuildConfig.h`를 통해 강력한 컴파일 타임 구성 옵션을 제공합니다.

### 메모리 제어를 위한 전처리기 매크로

| 매크로 | 용도 | 권장 설정 |
| :--- | :--- | :--- |
| `__MEMORY_VERIFICATION__` | 메모리 무결성 검증 (예: 가드 밴드 또는 체크섬 확인). | `1` (Debug) / `0` (Release) |
| `__MEMORY_LOGGING__` | 모든 할당 및 해제 이벤트의 로깅을 활성화합니다. 누수 추적에는 유용하지만 상당한 오버헤드가 발생합니다. | `1` (Debugging) / `0` (Performance) |
| `__MEMORY_INVESTIGATION__` | 할당을 수행한 스레드나 위치를 추적하는 것을 포함하여 상세한 메모리 조사를 활성화합니다. | `1` (Deep Debugging) / `0` (Normal) |
| `__MEMORY_DANGLING_POINTER_CHECK__`| 해제된 메모리를 지움으로써 use-after-free 오류를 감지하려고 시도합니다. | `1` (Debug) / `0` (Release) |
| `__MEMORY_BUFFER_UNDERRUN_CHECK__` | 할당 주변에 가드 페이지 또는 카나리 값을 배치하여 버퍼 오버플로/언더플로를 감지합니다. | `int` (Debug) / `0` (Release) |
| `PROFILE_ENABLED` | 사용량 통계 및 `AllocStats`와 같은 할당기별 메트릭을 포함한 성능 프로파일링을 활성화합니다. | `1` (Profiling) / `0` (Release) |

### 고급 디버깅 기능

#### 메모리 무결성 검증
`__MEMORY_VERIFICATION__`이 활성화되면 엔진은 할당 및 해제 중에 인접한 쓰기로 인해 메모리가 손상되지 않았는지 확인하는 작업을 수행할 수 있습니다. 이는 커스텀 할당기에서의 "침묵하는" 버그를 잡는 데 결정적입니다.

#### Stomp Allocator 패턴 (Dangling Pointer 감지)
`__MEMORY_DANGLING_POINTER_CHECK__`를 특수화된 할당기 구성과 결합하면 엔진은 "stomp" 또는 "poisoning" 패턴을 구현할 수 있습니다. 메모리가 해제되면 그 내용은 특정 패턴(예: `0xDEADBEEF`)으로 덮어씌워집니다. 만약 이 메모리에 대한 포인터가 나중에 사용된다면, 애플리케이션은 높은 확률로 충돌하거나 예측 가능한 동작을 보여 버그를 훨씬 쉽게 추적할 수 있게 합니다.

#### 메모리 로깅 및 조사
`__MEMORY_LOGGING__`이 활성화되면 모든 메모리 작업의 전체 기록을 로그 파일에 출력할 수 있습니다. 이는 다음 작업에 필수적입니다:
- 누수가 정확히 어디에서 시작되었는지 식별.
- 특정 메모리 블록의 수명 주기 추적.
- 시간에 따른 파편화 패턴 분석.

`__MEMORY_LOG_INVESTIGATION__`이 활성화되면 추가 메타데이터(예: `threadId`)가 `AllocatorProxy` 내에 저장되어, 오류가 나중에 다른 스레드에서 나타나더라도 어떤 스레드가 할당을 담당했는지 확인할 수 있습니다.

#### `AllocStats`를 이용한 프로파일링
`PROFILE_ENABLED`를 활성화하면 `MemoryManager`를 통해 상세한 통계에 접근할 수 있습니다. 여기에는 다음이 포함됩니다:
- **최대 사용량 (Peak Usage)**: 한 번에 할당기가 사용하는 최대 메모리 양.
- **할당 횟수 (Allocation Counts)**: `Allocate` 및 `Deallocate`가 호출된 횟수.
- **폴백 횟수 (Fallback Count)**: 할당기가 용량에 도달하여 상위 할당기(예: System Allocator)로 폴백해야 했던 횟수.

이 정보는 메모리 예산을 조정하고, `PoolAllocator`와 같은 고빈도 할당기가 비용이 많이 드는 폴백 작업을 피할 수 있도록 적절한 크기로 설정되어 있는지 확인하는 데 매우 중요합니다.

---
*최종 수정일: 2026년 4월 10일*
