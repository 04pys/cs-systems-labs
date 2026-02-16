# thread.md — C++ std::thread + Linux CPU affinity(pin) + 동기화(Barrier/Atomic) 노트

이 문서는 lab05 false sharing 실험에서 등장한 스레드/CPU 고정/동기화 요소를 한 번에 복습하기 위한 notes임  
목표는 실험 재현성 향상과 노이즈 감소를 위해 왜 이런 코드를 쓰는지까지 연결해서 이해하는 것임  

---

## 1) std::thread 기본 개념

### 1-1) std::thread는 무엇임
- C++ 표준 라이브러리의 스레드 래퍼임  
- OS 스레드(리눅스에서는 pthread) 위에 얇게 올라간 추상화로 보면 됨  
- 생성 시점에 실행할 함수/람다를 넘기면 바로 스레드가 실행됨  

예시(개념):
```
std::thread th([&](){
  // 스레드에서 실행되는 작업
});
```

### 1-2) join은 왜 필요함
- `join()`은 해당 스레드가 끝날 때까지 호출한 스레드(보통 main)가 기다리게 만드는 동기화임  
- join을 호출하지 않고 스레드 객체가 파괴되면 프로그램이 강제 종료될 수 있음(terminate)  
- 실험에서는 “모든 워커 스레드가 끝난 뒤 결과를 모으기” 위해 반드시 join이 필요함  

실험 코드 패턴:
```
vector<thread> th;
for (int tid=0; tid<nthreads; ++tid) {
  th.emplace_back([&, tid](){ work(tid); });
}
for (auto& x : th) x.join();  // 모든 스레드 종료 대기
```

### 1-3) thread id란 무엇임
- `std::this_thread::get_id()`로 얻는 값은 “스레드 식별자”임  
- 디버깅/로깅 용도에 가깝고, 실험 성능 자체에는 보통 직접적으로 영향 없음  
- 중요한 건 “tid(0..nthreads-1 같은 논리 id)”를 직접 만들어 슬롯을 나누는 방식임  
  - 예: `counters[tid]`처럼 tid가 배열 인덱스로 쓰임  

---

## 2) Linux에서 pinning이 필요한 이유와 의미

### 2-1) pinning의 목적
pinning(affinity 설정)은 “특정 스레드를 특정 CPU 코어에서만 돌게” 만드는 것임  
실험에서 pinning을 하는 이유는 주로 노이즈 감소 때문임  

- 스레드가 실행 중간에 다른 코어로 migrate되면
  - 해당 스레드의 L1/L2 캐시가 바뀌고  
  - 캐시 warm/cold 상태가 흔들리고  
  - 측정한 cycles가 튀기 쉬움  
- false sharing 실험에서는 “서로 다른 코어에서 같은 캐시 라인을 두고 invalidate ping-pong”이 핵심이므로  
  - 스레드가 코어를 떠돌면 관측이 깨끗하지 않게 됨  

정리하면 pinning은 “필수 기능”이라기보다 “실험을 깔끔하게 보이게 하는 장치”임  

### 2-2) Linux가 아닐 때 no-op인 이유
- `pthread_setaffinity_np`는 리눅스/글리브씨 기반에서 제공되는 비표준 API임  
- macOS/Windows에는 동일한 함수가 없음  
- 그래서 `#if defined(__linux__)`로 리눅스에서만 실제 구현을 두고, 아니면 “아무것도 안 하는 함수(no-op)”로 두는 패턴이 흔함  
- 이렇게 하면 “코드가 다른 OS에서도 컴파일은 되게” 만들 수 있음  

핵심은
- 리눅스면 pinning 옵션을 켜서 노이즈를 줄일 수 있고  
- 리눅스가 아니면 pinning 없이도 실험은 돌아가지만 재현성은 떨어질 수 있다는 것임  

---

## 3) pin_thread_to_cpu 코드 해부

### 3-1) cpu_set_t는 무엇임
- CPU affinity 마스크를 표현하는 자료형임  
- “이 스레드가 실행될 수 있는 CPU들의 집합”을 비트셋처럼 담음  

### 3-2) CPU_ZERO / CPU_SET은 무엇임
- `CPU_ZERO(&cpuset)`  
  - 마스크를 전부 0으로 초기화함(허용 CPU 없음 상태)  
- `CPU_SET(cpu, &cpuset)`  
  - 특정 cpu 번호 비트를 1로 설정함(그 cpu만 허용)  

### 3-3) pthread_self / pthread_setaffinity_np 의미
- `pthread_self()`  
  - 현재 실행 중인 스레드의 pthread 핸들을 얻음  
- `pthread_setaffinity_np(thread, sizeof(cpu_set_t), &mask)`  
  - 해당 스레드의 affinity(실행 가능 CPU 집합)를 설정함  
  - `_np`는 non-portable(이식성 낮음) 의미임  

실험에서의 의도는
- tid=0은 cpu_base+0  
- tid=1은 cpu_base+1  
- … 식으로 “스레드를 서로 다른 코어에 분산”시키는 것임  

### 3-4) Colab에서 pinning이 완전히 보장되지 않을 수 있는 이유
- Colab은 가상화/컨테이너/스케줄링 정책이 섞인 환경이라  
  - affinity 요청이 무시되거나  
  - 코어 번호가 실제 물리 코어와 1:1로 대응하지 않거나  
  - CPU 자원이 공유되어 성능이 흔들릴 수 있음  
- 그래도 실험 노이즈를 줄이는 방향으로는 도움이 되는 경우가 많음  

---

## 4) Barrier(장벽)와 왜 필요한지

### 4-1) barrier가 하는 역할
barrier는 “모든 스레드가 특정 지점에 도착할 때까지 기다렸다가, 다 같이 다음 단계로 넘어가게” 하는 동기화임  

실험에서 barrier가 없으면 생기는 문제:
- 어떤 스레드는 이미 루프를 도는 중인데  
- 어떤 스레드는 아직 시작도 안 함  
→ 측정 구간이 스레드마다 달라지고 “동시에 경쟁하는 상황”이 깨짐  

그래서 false sharing 실험에서는 보통
- 시작 전에 한 번 barrier  
- 끝나고 한 번 barrier  
를 둬서 “동일한 구간을 동시에 실행”시키는 것이 중요함  

### 4-2) SpinBarrier가 spin인 이유
- mutex/condition_variable 같은 블로킹 동기화 대신  
- 짧은 구간에서는 busy-wait(spin)이 오버헤드가 더 작을 수 있음  
- `_mm_pause()`는 x86에서 spin loop에 넣는 힌트로  
  - 파이프라인/전력/하이퍼스레딩 자원 경쟁을 완화하는 데 도움됨  

---

## 5) atomic은 무엇이고 왜 쓰는지

### 5-1) std::atomic<T> 의미
- 멀티스레드에서 동시에 읽고/쓰는 변수를 “데이터 레이스 없이” 다루기 위한 타입임  
- `arrived.fetch_add(1)` 같은 연산은 원자적으로 수행됨  
  - 두 스레드가 동시에 증가시켜도 값이 꼬이지 않음  

### 5-2) memory_order_*는 왜 나옴
- 원자 연산은 “동기화 강도”를 선택할 수 있음  
- barrier 용도로는 최소한의 질서만 보장하면 되는 경우가 많아서
  - `relaxed`, `acquire`, `release`, `acq_rel` 조합이 자주 쓰임  

여기서 직관적으로만 잡으면:
- release: “이전 작업이 끝났음을 다른 스레드에 보이게”  
- acquire: “다른 스레드가 끝냈다고 했으니 그 결과를 제대로 보게”  
- acq_rel: 둘 다  

완벽한 메모리 모델까지 파고들기보다는  
“barrier가 올바르게 작동하도록 최소 질서를 준다” 정도로 잡으면 실험 목적에는 충분함  

---

## 6) explicit 생성자란 무엇임

### 6-1) 왜 explicit을 붙임
- `explicit SpinBarrier(int n)`은 “암시적 형변환으로 객체가 만들어지는 것”을 막는 문법임  
- 예: 함수가 SpinBarrier를 받는데 실수로 int를 넘겨도 자동 생성되는 걸 방지함  

즉, 실험 코드 안정성을 위한 습관적 방어임  

---

## 7) packed vs padded와 alignas(64)의 의미

### 7-1) false sharing의 핵심 구조
- 캐시 라인은 보통 64B 단위임  
- packed 구조에서는
  - `PackedCounter{ uint64_t v; }`가 연속으로 붙어 배치됨  
  - 여러 스레드의 `v`가 같은 캐시 라인에 같이 들어갈 확률이 큼  
  - 서로 다른 코어가 서로 다른 `v`를 갱신해도
    - 캐시 coherence 때문에 같은 라인 전체가 invalidate되며 ping-pong 발생 가능함  

### 7-2) alignas(64)가 왜 중요한지
- `alignas(64)`는 “해당 객체의 시작 주소를 64B 경계에 맞춰라”는 요구임  
- 보통 `sizeof(PaddedCounter)`도 64B로 커지거나(패딩이 붙어)  
  - 결과적으로 각 counter가 캐시 라인을 독점하게 되는 배치가 유도됨  

즉,
- packed: 같은 캐시 라인 공유 가능 → false sharing 유도  
- padded: 캐시 라인 분리 유도 → false sharing 회피  

---

## 8) hw_threads(thread::hardware_concurrency)의 의미와 주의점

- `std::thread::hardware_concurrency()`는 “동시 실행 가능한 하드웨어 스레드 수 추정값”임  
- Colab에서 2가 나온다는 것은
  - 보통 vCPU가 2개로 할당됐다는 뜻에 가까움  
- nthreads를 2보다 크게 만들어도 의미가 없어지는 것은 아님  
  - OS는 더 많은 소프트웨어 스레드를 만들 수 있고  
  - 그 스레드들이 2개의 vCPU 위에서 time-slicing됨  
  - 그래서 “동시에 진짜 병렬”은 아니지만 경쟁/스케줄링/캐시 효과가 일부 관측될 수 있음  
- 다만 false sharing의 “코어 간 ping-pong”이 가장 깔끔하게 보이는 조건은  
  - 실제로 서로 다른 코어에서 동시에 돌 때임  
  - 그래서 nthreads는 보통 “가용 hw_threads 근처”가 관측이 안정적인 편임  

---

## 9) 실험에서 pinning을 켤 때의 실행 예시(Colab)

- 예: 스레드 2개, 스레드당 5천만 inc, trials 11, pin on, cpu_base 0  
  - ./main 2 50000000 11 1 0

인자 의미(실험 코드 기준):
- argv[1] = nthreads  
- argv[2] = iters_per_thread  
- argv[3] = trials  
- argv[4] = pin (0/1)  
- argv[5] = cpu_base  

---

## 10) 추천 실험 루틴(노이즈 대응)

- nthreads는 hw_threads 이하부터 시작해서 1, 2, (가능하면 4) 정도로 비교  
- packed/padded ratio를 중심으로 해석  
- trials는 7~15에서 median을 쓰고, 필요하면 각 trial 값을 로그로 함께 남김  
- pin=1은 가능하면 켜고, 효과가 불안정하면 pin=0 결과와 같이 기록  

---

## 11) 체크 포인트 요약

- join은 반드시 필요함(결과 수집 + 종료 보장)  
- barrier는 “동시에 시작/동시에 종료”를 강제해 측정 구간을 맞추는 장치임  
- max(thread_cycles)를 쓰는 이유는 “동시에 돌린 구간의 전체 길이”를 대표시키기 위함임  
  - 가장 늦게 끝난 스레드가 전체 구간 종료 시점을 결정하므로 max가 자연스러운 요약 통계가 됨  
- pinning은 필수는 아니지만 노이즈 감소에 유리함  
- packed/padded 차이는 캐시 라인 공유 여부이며, false sharing의 본체임  
