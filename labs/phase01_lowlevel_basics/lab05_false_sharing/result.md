# lab05 false_sharing results (packed vs padded, pin on/off)

## 0) Experiment setup (as executed)
- Fixed workload
  - iters_per_thread = 50,000,000 (each thread increments its own counter this many times)
  - trials = 11 (each run prints a median-based summary across 11 trials)
- Swept parameters
  - nthreads ∈ {1, 2, 4, 8}
  - pin ∈ {0, 1}
- Machine report from the program
  - hw_threads = 2 (returned by `std::thread::hardware_concurrency()` in this environment)
  - Note: `hardware_concurrency()` is a hint, not a guarantee of physical cores. In Colab it often reflects a 2-vCPU style limit, so oversubscription (nthreads > 2) is likely.

---

## 1) What the code is measuring (exactly)
- Each thread increments only its own counter (`(*p)++`) in a tight loop.
- The only intended difference between layouts is memory placement of per-thread counters:
  - **packed**: counters are contiguous, so multiple threads’ counters can land in the same 64B cache line
  - **padded**: each counter is `alignas(64)` so different threads’ counters are separated at cache-line granularity (intended to avoid false sharing)
- Timing unit and reported metric
  - cycles are measured by `rdtsc_begin()` / `rdtsc_end()` around the increment loop
  - per trial, the benchmark records per-thread cycles for the timed section
  - trial representative cycles: `mx = max(cyc_per_thread)` (slowest thread)
  - across trials: take the **median** of `mx` values
  - final output:
    - `cycles_per_inc = median(mx) / (nthreads * iters_per_thread)`

---

## 2) Why measure `cycles_per_inc` (instead of only total cycles/time)
- Total work changes with `nthreads`:
  - total increments = `nthreads * iters_per_thread`
  - comparing raw total cycles/time across different `nthreads` mixes two effects:
    1) more work, and
    2) higher per-operation cost (e.g., false sharing)
- `cycles_per_inc` normalizes by total increments, so results across different `nthreads` are comparable as **cost per increment**.
- False sharing should show up as extra coherence traffic / invalidation cost on stores, i.e., an increase in the average cost per `(*p)++`. Measuring “per increment” makes that easier to quantify.
- This does not hide total time: the trial representative total cycles can be reconstructed as:
  - `median(mx) ≈ cycles_per_inc * nthreads * iters_per_thread`

---

## 3) Bench structure (why `max` and why median)
Per trial:
1) `thrash_cache(thrash)` to disturb cache contents before measurement (reduce dependence on the previous trial’s cache state)
2) create `SpinBarrier(nthreads)`
3) spawn `nthreads` threads; each thread:
   - optionally pins itself to a CPU (`pin_thread_to_cpu`, Linux-only)
   - waits on barrier (start alignment)
   - measures cycles for its increment loop
   - waits on barrier (end alignment)
4) after join, compute `mx = max(cyc_per_thread)` and store it as the trial’s representative total cycles

Why `max(cyc_per_thread)`:
- The start barrier aligns the beginning of the timed region as much as possible.
- The end barrier forces all threads to rendezvous after finishing the timed region.
- In practice, the group completion time is constrained by the slowest thread (scheduler effects + contention).
- So using `max` makes the trial representative value closer to “effective completion time” of the whole group.

Why median across trials:
- Colab tends to have scheduling/clock noise that can produce outliers.
- Using the median of per-trial maxima reduces sensitivity to those outliers.

---

## 4) Raw results (median-based outputs)
All values below are exactly the printed summaries.

### 4-1) pin = 0
| nthreads | packed cycles/inc | padded cycles/inc | packed/padded |
|---:|---:|---:|---:|
| 1 | 4.1608 | 4.5250 | 0.9195 |
| 2 | 7.9423 | 2.3269 | 3.4133 |
| 4 | 7.6819 | 3.1979 | 2.4021 |
| 8 | 4.8571 | 2.5028 | 1.9407 |

### 4-2) pin = 1
| nthreads | packed cycles/inc | padded cycles/inc | packed/padded |
|---:|---:|---:|---:|
| 1 | 4.5115 | 4.5181 | 0.9985 |
| 2 | 6.3849 | 3.6529 | 1.7479 |
| 4 | 7.7919 | 2.8418 | 2.7418 |
| 8 | 4.8963 | 2.8123 | 1.7411 |

---

## 5) What the results show (only what is strongly supported by these numbers)

### 5-1) nthreads = 1에서는 false sharing이 구조적으로 불가능
- 스레드가 1개면 한 코어(혹은 한 실행 컨텍스트)가 한 counter만 쓰는 구조라 캐시 라인 ping-pong이 생길 수 없음.
- 관측값
  - pin=0: packed 4.1608 vs padded 4.5250 (packed가 약 8% 낮음)
  - pin=1: packed 4.5115 vs padded 4.5181 (거의 동일)
- 결론
  - nthreads=1에서 packed/padded 차이는 false sharing 증거가 아님.
  - 이 구간의 차이는 alignment/배치 부작용, 측정 노이즈, 미세한 실행 경로 차이로 설명 가능.

### 5-2) nthreads ≥ 2에서는 packed가 padded보다 항상 느림(이번 데이터 기준)
- 이번 결과에서 핵심 시그니처는 `packed/padded > 1`이 모든 (2,4,8)과 모든 pin에서 유지된다는 점임.
- nthreads=2
  - pin=0: 3.4133배
  - pin=1: 1.7479배
- nthreads=4
  - pin=0: 2.4021배
  - pin=1: 2.7418배
- nthreads=8
  - pin=0: 1.9407배
  - pin=1: 1.7411배
- 결론
  - 여러 스레드가 동시에 store를 치는 상황에서, counter를 캐시 라인 단위로 분리(padded)하면 increment당 비용이 크게 줄어듦.
  - packed는 서로 다른 스레드가 “다른 변수”를 갱신해도 같은 캐시 라인에 얹히면 coherence invalidation이 반복되는 구조라, false sharing 페널티가 보이는 패턴과 일치.

### 5-3) pin은 크기를 바꾸지만 방향은 바꾸지 않음
- (2,4,8) 모든 케이스에서 packed는 padded보다 느렸고(ratio > 1), pin은 그 ratio의 크기만 바꿈.
- 다만 단조적이지는 않음
  - nthreads=2: pin 켰을 때 ratio 감소 (3.41 → 1.75)
  - nthreads=4: pin 켰을 때 ratio 증가 (2.40 → 2.74)
  - nthreads=8: pin 켰을 때 ratio 소폭 감소 (1.94 → 1.74)
- 결론
  - pinning은 스케줄링/배치에 영향을 줘서 magnitude를 바꾸지만, 이번 데이터만으로 “pin이 항상 좋다/나쁘다” 같은 방향성 주장은 불가.

### 5-4) hw_threads = 2 환경에서도 oversubscription 상태에서 penalty가 관측됨
- hw_threads=2라서 nthreads=4,8은 동시에 다 실행되지 않고 time-slicing이 섞일 가능성이 큼.
- 그럼에도 packed가 padded보다 느린 현상은 계속 관측됨(대략 1.74~2.74배).
- 결론
  - `nthreads ≤ hw_threads`가 아니어도 packed penalty는 측정 가능.
  - 다만 2→4→8에서 ratio가 어떻게 변하는지를 “코어 수 증가에 따른 순수 coherence 비용 증가”로 해석하면 위험하고, 스케줄링 영향이 같이 섞였다고 보는 게 안전.

---

## 6) Implementation notes (main.cpp에서 확인한 포인트)

### 6-1) thrash buffer의 역할
- `thrash_cache(thrash)`는 64B stride로 메모리를 훑어 캐시 내용을 흔들어 놓는 용도.
- 목적은 “이전 trial의 cache state가 다음 trial에 남는 것”을 줄여 trial 간 변동성을 완화하는 쪽에 가까움.
- 단순 warmup과는 성격이 다르고, 재현성을 올리려는 장치로 보는 게 자연스럽다.

### 6-2) pin_thread_to_cpu의 역할과 Linux-only 처리
- 목적은 thread affinity를 설정해서 스레드가 특정 CPU에서 계속 돌게 만들고(가능하면), migration에 의한 노이즈를 줄이려는 것.
- Linux에서만 켠 이유는 `pthread_setaffinity_np`, `cpu_set_t`, `CPU_ZERO/CPU_SET`가 Linux/POSIX 계열 API이기 때문임.
- Linux가 아닌 환경에서는 no-op으로 두어서 “컴파일은 되지만 기능은 꺼짐” 상태로 만든 형태.

### 6-3) SpinBarrier + atomic의 역할
- barrier는 “측정 구간의 시작/끝”을 스레드 간에 맞춰서 trial 단위 측정이 의미 있게 되도록 만든 장치.
- `atomic<int> arrived/epoch`는 데이터 레이스 없이 스레드들이 도착 카운트와 세대(epoch)를 공유하도록 보장함.
- `_mm_pause()`는 스핀 대기 루프에서 리소스 낭비/경합을 줄이는 힌트로 흔히 사용됨.

### 6-4) packed vs padded가 alignas(64) 하나로 크게 갈리는 이유
- coherence invalidation은 보통 캐시 라인(대개 64B) 단위로 일어남.
- packed는 서로 다른 counter가 같은 캐시 라인에 들어가면, 각 코어의 store가 서로의 라인을 계속 무효화시키는 ping-pong이 가능.
- padded는 counter를 캐시 라인 단위로 분리해 이 ping-pong을 크게 줄이려는 구조.

---

## 7) Minimal takeaways (grounded in this dataset)
- nthreads=1에서는 packed/padded 차이를 false sharing으로 해석할 근거가 없음.
- nthreads≥2에서는 packed가 padded보다 항상 느렸고, ratio가 1.7411~3.4133 범위로 관측됨.
- hw_threads=2 환경에서도 (nthreads>2) oversubscription 상황에서 packed penalty가 계속 관측됨.
- pinning은 결과의 크기를 바꾸지만, 이 데이터만으로 일관된 방향성(항상 증가/감소)은 주장하기 어렵다.
