세션 스냅샷 (새 채팅 시작 시 그대로 붙여넣기 용도)

1) 내 목표와 방향
- cs-system-labs 레포지토리를 학습 기록 + 깃허브 포트폴리오로 운영 중
- 최종 목표는 GPU/NPU/PIM 등 메모리 중심 시스템 연구 역량 강화 + IES Lab 학부연구생 지원 및 관련 직무 준비
- 현재는 low-level 기본기 복구 + “실험으로 확인 → 문서로 남김” 루틴 확립 단계

2) 레포지토리 운영 규칙/스타일
- labs/ 아래에 실험 단위로 디렉토리 생성 및 관리
- notes/에는 개념 정리 문서 저장
- logbook/에는 날짜별로 What I did / Key results / Takeaways 형식의 짧은 기록 저장
- README.md는 실험 목적, 빌드/실행, 실험 설계, 파일 구성 중심으로 작성
- result.md는 “실험 결과 + 결과로부터 확실히 말할 수 있는 해석”만 유지(개념/원리는 notes로 분리)
- 측정 변동성이 있으므로 반복(trials) + median 중심으로 정리하는 습관 유지
- correctness(정확성) 체크를 실험에 포함(시간만 줄어드는 버그 방지 목적)
- Colab에서 %%bash + g++로 C++ 실험 실행하는 방식 사용

3) 완료/진행한 lab 요약

Lab01 cache_walk
- row_major vs col_major_like 비교 실험 수행
- N이 작으면 차이 거의 없고, N이 커질수록 col_major_like가 크게 느려짐 관측
- 원인 후보: cache line 재사용, prefetcher 적중률, TLB 압박 차이
- 산출물: labs/lab01_cache_walk/main.cpp, README.md, result.md

Lab02 tiling_blocking
- col_major_like를 타일링으로 개선하는 tiled_col 실험 수행
- N=4096/8192에서 B 스윕 결과, 최적 B가 32 또는 64 근처에서 자주 나타남
- B가 너무 작으면 고정 오버헤드 대비 재사용 이득이 작고, 너무 크면 working set 증가로 캐시/TLB/세트 충돌 증가 가능성 정리
- result.md에는 결과만 유지, 타일링 개념 설명은 notes로 분리하기로 결정

Lab03 matrix_transpose tiling
- naive transpose vs tiled transpose 비교
- read-friendly / write-friendly 변형도 함께 비교
- tiled, 특히 write-friendly가 크게 빨라지는 경향 관측
- 변동성 존재 → 반복/median/추세 중심으로 해석
- correctness 검증: spot_check(랜덤 좌표 샘플로 B[j,i]==A[i,j] 확인)
- 벤치 프레임워크로 template 기반 bench 함수 + 람다 구조 사용(커널 추가가 쉬워짐)

Lab04 prefetch (main.cpp + test.cpp)
목표/핵심 질문
- hardware prefetcher만 쓸 때(base) vs software prefetch(__builtin_prefetch) 추가 시 차이 관찰
- stride별 최적 dist(얼마나 ahead로 가져올지) 찾기
- stride가 커질수록 base가 단조 증가하지 않는 현상, dist 최적이 stride별로 들쭉날쭉한 현상 확인 → “stride별 최적 dist 탐색 문제”로 재정의

main.cpp 구조(스윕 기반)
- 256MB 수준 큰 배열로 캐시 밖 워킹셋 구성
- stride여도 모든 원소를 정확히 1번씩 방문하도록 offset loop 사용:
  - for off in [0..stride-1], for i=off; i<n; i+=stride
- prefetch는 i 기준 i + dist*stride 위치를 __builtin_prefetch로 힌트
- dist 후보군(예): 1,2,3,4,6,8,11,15,16,18,22,24,32,64 등 넓게 스윕
- stride별 base와 dist별 pf 시간을 출력 → best_dist 결정(여러 회 수행 후 median으로 요약)

test.cpp 구조(예측 기반)
- ns 기반이 아니라 TSC 기반 cycle 측정으로 전환
- rdtsc/rdtscp + lfence로 구간 측정 안정화
- L_lat_cyc: pointer-chase(의존적 접근)로 대략적 load latency(cycles/step) 추정
- T_iter_cyc: 실제 stride 커널의 평균 cycles/access 추정
- dist_pred ≈ ceil(L_lat_cyc / T_iter_cyc)로 “대략 필요한 ahead 거리” 예측
- dist 후보는 dist_pred 주변 + power-of-two 근처 포함으로 최소 실험으로 best_dist 탐색

Lab04에서 확인한 관측(요약)
- stride 1: prefetch 이득 없음 또는 손해 가능
- stride 2/4/8/16: 적당 dist에서 소폭 개선(수 % 수준) 구간 존재
- stride 64/128/256: prefetch 이득 없음(none) 사례 존재
- stride 512/1024/2048: 개선이 가장 크게 관측(대략 18~22% 수준), best_dist가 16~24 같은 두 자릿수로 수렴하는 경향
- 해석 포인트 후보:
  - 작은 stride에서 HW prefetcher가 이미 잘 맞아 software prefetch가 중복/오염 가능
  - 큰 stride에서 HW prefetcher가 맞추기 어려워 software prefetch가 유효해질 가능성
  - dist 최적은 “너무 작으면 늦고, 너무 크면 캐시 오염/TLB pressure/불필요 대역폭”의 균형으로 생김
  - base 단조성 붕괴 원인 후보: off loop로 인한 다중 스트림, set conflict, TLB pressure, MLP/outstanding miss 한계 등

Lab05 false_sharing (packed vs padded, pin on/off)
목표
- false sharing(거짓 공유)로 인한 cache line ping-pong 비용 관찰

핵심 코드/실험 구조(main.cpp)
- std::thread로 nthreads개 스레드 생성
- 각 스레드는 “자기 카운터 1개만” iters_per_thread 만큼 증가시키는 tight loop 수행
- packed: counters가 연속 배치 → 서로 다른 스레드의 counter가 같은 cache line(64B)에 들어갈 수 있음
- padded: struct alignas(64)로 counter를 cache line 단위로 분리하려는 의도
- SpinBarrier(atomic 기반)로 시작/끝을 정렬하고, rdtsc로 루프 구간 cycles 측정
- 각 trial마다 thread별 cycles를 얻고, trial 대표값으로 max(thread_cycles)를 사용
  - 시작/끝 barrier가 있으므로 “그 trial에서 가장 늦게 끝난 스레드”가 그룹 완료를 결정한다는 관점
- trials 반복 후 median(max_cycles)로 노이즈 완화
- 최종 지표는 cycles_per_inc = median(max_cycles) / (nthreads * iters_per_thread)
  - 서로 다른 nthreads에서도 “증가 1회당 비용”으로 비교하기 위한 정규화
  - raw total cycles는 cycles_per_inc * nthreads * iters_per_thread로 환산 가능

환경/실행 파라미터(내가 실제 돌린 형태)
- iters_per_thread = 50,000,000 고정
- trials = 11 고정
- nthreads ∈ {1,2,4,8}, pin ∈ {0,1} 스윕
- hw_threads = std::thread::hardware_concurrency()가 2로 출력됨
  - hardware_concurrency는 “힌트”이며 물리 코어 수를 보장하지 않음
  - Colab에서는 2 vCPU처럼 보이는 제한이 있는 경우가 많아 nthreads>2에서 oversubscription 가능성이 큼

Lab05 결과(핵심 결론: 데이터 기반으로 확실한 것만)
- nthreads=1:
  - packed와 padded 차이는 있어도 false sharing 증거로 보기 어렵고, 대체로 비슷한 수준
- nthreads>=2:
  - 모든 조건에서 packed가 padded보다 느림(비율이 대략 1.74~3.41 범위로 관측)
  - 즉, counter를 cache line으로 분리(padded)하면 per-inc 비용이 줄어드는 패턴이 반복 관측됨
- pinning:
  - 크기(magnitude)는 바뀌지만 방향( packed > padded )은 유지됨
  - pin이 항상 penalty를 키운다/줄인다 같은 단정은 현재 데이터만으로 어렵다고 정리함
- hw_threads=2 환경에서도 nthreads=4/8에서 packed penalty가 관측됨
  - 다만 2→4→8 스케일링을 “코어 수 증가 효과”로 단순 해석하면 안 되고 스케줄링(시간분할) 효과가 섞였다고 보는 편이 안전

Lab05에서 정리한 개념/메모(내가 새로 확실히 이해한 것들)
- thrash_cache:
  - 매 trial 전에 캐시 상태를 흔들어 이전 trial 잔존 효과를 줄이려는 용도
  - “예열”이라기보다 “상태 의존성 감소” 목적에 가깝게 이해함
- pin_thread_to_cpu:
  - 목적: thread affinity를 설정해 스레드가 특정 CPU에 머물도록 유도하여 migration 노이즈 감소
  - Linux에서 pthread_setaffinity_np, cpu_set_t, CPU_ZERO/CPU_SET 등 사용
  - 비-Linux에서는 no-op로 둬서 컴파일/이식성을 확보하는 방식
- SpinBarrier(atomic + spin):
  - 시작/끝 정렬을 위한 동기화 장치
  - atomic은 data race 없이 arrived/epoch 같은 공유 상태를 갱신하기 위한 타입
  - _mm_pause는 spin 루프에서 자원 경쟁/전력 낭비를 줄이는 힌트
- packed vs padded:
  - coherence는 cache line 단위로 invalidation이 발생
  - packed에서 서로 다른 변수라도 같은 cache line 공유 시 false sharing으로 ping-pong 비용 발생 가능
  - padded는 그 가능성을 줄이는 구조

4) 지금 시점의 산출물/해야 할 문서
- Lab04:
  - result.md: main median 표 + test 비교 + 불일치 원인 요약(개념 설명 과다 금지)
  - notes: TSC 측정법(rdtsc/rdtscp/lfence, asm volatile, clobber 등) 정리
- Lab05:
  - result.md: 실험 결과 표 + bench 구조 + packed/padded 차이 + cycles_per_inc 선택 이유 + pin/hw_threads 해석 주의
  - notes: thread/cpu/pinning/barrier 정리(복사 붙여넣기 가능한 형태)
  - logbook: 2026-02-16 What I did / Key results / Takeaways 작성

5) 다음에 이어갈 작업 (Lab06 + Phase 2)
Lab06 simd_vectorization(예정)
- 목적: SIMD 벡터화가 실제로 성능을 얼마나 올리는지, 어떤 조건에서 이득/손해가 나는지 실험으로 확인
- 실험 설계 방향(다음 채팅에서 바로 이어서 설계 가능):
  - 커널 후보: vector add / saxpy / dot product / reduction(sum) / 1D stencil 중 1~2개 선택
  - 비교 축:
    - scalar baseline vs 컴파일러 auto-vectorization vs intrinsics(SSE/AVX)
    - data alignment(unaligned vs aligned 32/64B), array size(캐시 내/밖), stride(연속/간격), unroll 여부
  - 측정:
    - rdtsc 기반 cycles(가능하면 pin + warmup + trials + median)
    - correctness 체크 포함(특히 reduction/dot은 오차 허용 범위 설정)
  - 문서화:
    - result.md에는 측정치와 확실한 결론만
    - vectorization 원리/ABI/alignas 등의 설명은 notes로 분리

Phase 2(확장 방향 후보)
- 목표를 “GPU/NPU/PIM 연구 역량” 쪽으로 한 단계 올리기 위한 주제 후보:
  - memory-level parallelism(MLP) 관찰/조절 실험
  - prefetch + unroll + SIMD 결합 효과
  - multi-thread scaling에서 bandwidth 관찰(환경 가능 시)
  - roofline 관점(대략 operational intensity vs bandwidth/compute)으로 결과 해석 연습
- 문서 품질 강화 포인트:
  - 실험 재현성(고정 seed, 컴파일 옵션 명시, 환경 출력)
  - 변동성 보고(median + min/max 또는 IQR)
  - 해석을 과하게 하지 않기 원칙 유지

6) 내 실행 환경/습관 메모
- Google Colab에서 %%bash + g++로 빌드/실행
- 변동성 큰 환경이라 trial 수를 확보하고 median으로 요약하는 편이 안전
- pinning은 Colab에서 항상 기대대로 보장되지는 않을 수 있지만, Linux 환경일 가능성이 높아 시도는 가능
(끝)
