세션 스냅샷 (새 채팅 시작 시 그대로 붙여넣기 용도)

1) 내 목표와 방향
- cs-system-labs 레포지토리를 학습 기록 + 깃허브 포트폴리오로 활용 중인 상황임
- 최종 목표는 GPU/NPU/PIM 등 메모리 중심 시스템 연구 역량을 쌓고, Intellegence Embedded System LAB 학부연구생 지원 및 관련 직무 취업 준비임
- 현재 단계는 low-level 기본기 복구 및 실험 기반 학습 루틴 확립임

2) 레포지토리 운영 방식
- labs/ 아래에 실험 단위로 디렉토리 생성 및 관리 중인 상황임
- notes/에는 개념 정리 문서 저장하는 방식임
- logbook/에는 날짜별로 짧은 요약 기록하는 방식임
- README.md는 실험 목적, 빌드/실행 방법, 실험 설계, 파일 구성 중심으로 작성하는 스타일 선호임
- result.md는 실험 결과와 해석만 유지하는 원칙 적용 중임
- 문장에 따옴표 사용을 피하고 자연스럽게 쓰는 스타일 선호임

3) 완료한 lab 요약
Lab01 cache_walk
- row_major vs col_major_like 비교 실험 수행함
- N이 작으면 차이 거의 없음, N이 커질수록 col_major_like가 크게 느려짐 관측함
- 원인으로 cache line 재사용, prefetcher 적중률, TLB 압박 차이 정리함
- 관련 파일
  - labs/lab01_cache_walk/main.cpp
  - labs/lab01_cache_walk/README.md
  - labs/lab01_cache_walk/result.md
  - notebooks/로 colab notebook 이동 완료함

Lab02 tiling_blocking
- col_major_like를 타일링으로 개선하는 tiled_col 실험 수행함
- N=4096과 N=8192에서 B를 스윕하여 최적 B가 32 또는 64 근처에서 자주 나타남 관측함
- B가 너무 작으면 고정 오버헤드 대비 재사용 이득이 작고, 너무 크면 working set 증가로 캐시/TLB/세트 충돌 증가로 손해 발생 가능성 정리함
- result.md에는 실험 결과만 유지하고, tiling 개념 설명은 notes로 분리하기로 결정함

Lab03 matrix_transpose tiling
- naive transpose와 tiled transpose 비교 실험 수행함
- read-friendly와 write-friendly 변형을 같이 비교함
- 실험 결과에서 tiled 특히 write-friendly가 크게 빨라지는 경향 관측함
- 같은 실험에서도 변동이 존재하므로 반복/median/추세 중심 해석 습관 확립함
- correctness 검증을 위해 spot_check 사용함
  - 랜덤 샘플 좌표를 뽑아 B[j,i] == A[i,j]인지 확인하는 방식임
  - 계산을 빠뜨려 시간만 줄어드는 버그 방지 목적임
- 벤치 프레임워크로 template 기반 bench 함수와 람다 사용 학습함
  - 여러 커널을 함수 인자로 받아 같은 측정 루프를 재사용하는 구조임
  - 실험 케이스 추가가 쉬워지는 장점 정리함

Lab04 prefetch 실험 진행 중
- 목표는 hardware prefetcher 한계 구간에서 software prefetch(__builtin_prefetch) 효과 관찰하는 방향임
- 초기 실험에서 stride 커질수록 시간이 단조 증가하지 않는 현상, dist 최적값이 stride마다 들쭉날쭉한 현상 확인함
- 단조성 가설이 틀렸다는 결론으로 전환하고, stride별 최적 dist를 찾는 문제로 재정의함

4) prefetch 실험 코드 구조 핵심
Main 실험 커널 (main.cpp 쪽)
- 벡터 a를 256MB 정도로 만들어 캐시 밖 워킹셋으로 구성함
- stride가 커도 모든 원소를 정확히 1번씩 방문하도록 offset loop 구조 적용함
  - for off in [0..stride-1], for i=off; i<n; i+=stride 형태임
- prefetch는 i에 대해 i + dist*stride 위치를 미리 가져오는 방식임
- dist 후보군을 개선하여 작은 값과 큰 값(예: 1,2,3,4,6,8,11,15,16,18,22,24,32,64 등)을 같이 스윕함
- 결과는 stride별 base와 prefetch(dist별) 시간을 출력하고 최적 dist를 찾는 방식임

Test 실험 커널 (test.cpp 쪽)
- ns 기반이 아니라 TSC 기반 cycle 측정으로 전환함
- rdtsc/rdtscp + lfence 조합으로 구간 측정 안정화함
- L_lat_cyc 측정은 pointer-chase 성격의 접근 패턴을 사용해 메모리 latency 추정 방향임
- T_iter_cyc는 실제 커널에서 접근 1회당 비용 추정 방향임
- dist_pred는 대략 L_lat_cyc / T_iter_cyc로 계산하는 방식임
- 후보 dist는 dist_pred 주변 + power-of-two 근처 포함으로 최소 실험으로 best_dist 찾는 방식임

5) test.cpp에서 얻은 cycle 기반 결과 샘플
- 출력 형식 예시
  stride=1 L_lat_cyc=13.511 T_iter_cyc=7.068 dist_pred=2 candidates=[1,2,3,4] base_cyc_per_access=7.982 best_dist=1 best_cyc_per_access=7.487 improvement=6.208%
  stride=8 L_lat_cyc=15.438 T_iter_cyc=7.587 dist_pred=3 candidates=[1,2,3,4,6] base_cyc_per_access=8.336 best_dist=6 best_cyc_per_access=7.786 improvement=6.596%
  stride=64 L_lat_cyc=123.570 T_iter_cyc=8.697 dist_pred=15 candidates=[7,8,11,15,16,18,22,30] base_cyc_per_access=24.290 best_dist=22 best_cyc_per_access=23.962 improvement=1.350%
  stride=512 L_lat_cyc=142.379 T_iter_cyc=8.462 dist_pred=17 candidates=[8,12,16,17,21,25,32,34] base_cyc_per_access=25.479 best_dist=32 best_cyc_per_access=22.859 improvement=10.281%
  stride=2048 L_lat_cyc=266.339 T_iter_cyc=8.035 dist_pred=34 candidates=[17,25,32,34,42,51,64,68] base_cyc_per_access=29.723 best_dist=17 best_cyc_per_access=27.301 improvement=8.148%

6) main.cpp에서 새 후보군으로 얻은 3회 결과 데이터 존재
- stride별 base_ms와 dist별 pf_ms 출력이 매우 긴 형태로 3세트 존재함
- 이 3세트로 median을 계산해 stride별 최적 dist를 도출하고, test.cpp의 dist_pred 및 best_dist와 비교하는 result.md를 작성하려는 상태임
- none인 경우는 후보군 안에서 base보다 좋아지는 dist가 없거나 측정 노이즈로 개선이 안 보이는 케이스로 해석하는 방향임

7) 지금 하려던 산출물
Result 문서
- lab04/result.md로 다음 내용을 정리하려는 상태임
  - main.cpp 3회 결과의 stride별 median 표 작성
  - stride별 main 최적 dist와 test.cpp best_dist, dist_pred 비교
  - 왜 맞는지 또는 왜 어긋나는지 이유 분석 포함
  - dist 최적점은 memory penalty와 1회 반복 비용의 균형으로 결정된다는 결론 강조
  - rdtsc/lfence/rdtscp 세부 지식은 result.md에는 참조 수준만, 자세한 설명은 notes로 분리하는 원칙 유지

Notes 문서
- TSC 측정 노트 작성 요청 있었음
  - rdtsc, lfence, rdtscp 의미
  - asm volatile 문법, 입력/출력/클로버 개념
  - begin/end에서 순서가 다른 이유 정리
  - cache 흔들림 완화용 thrash 버퍼 의미 정리

Logbook 문서
- 2026-02-08.md를 What I did / Key results / Takeaways로 작성 완료한 상태임

8) 나의 질문/관심 포인트 정리
- 왜 stride 커질수록 base 시간이 단조 증가하지 않는지에 대한 실험적 해석 필요함
- 왜 dist 최적값이 stride별로 다르게 나오고, 때로는 큰 값(32,64)도 최적이 되는지 정밀 해석 필요함
- dist_pred 계산이 실제 실험과 일치하는 정도를 표로 비교하고, 불일치 구간의 원인을 설명하고 싶음
- colab 환경 변동성, 측정 노이즈, prefetch 오버헤드, 캐시 계층(L2/L3) 전환, TLB, MLP, HW prefetcher와의 상호작용 같은 요인이 후보 원인임

9) 다음 액션 플랜 후보
- lab04/result.md 작성 및 main median 표 + test 비교 완료함
- notes에 TSC 측정법 정리 문서 추가함
- 이후 lab05 멀티스레드 데이터 배치 실험 또는 lab06 branch/BTB 관련 실험으로 확장 후보 검토함
- IES Lab 학부연구생용 포트폴리오 관점에서 실험 설계, 재현성, 문서화 품질을 강화하는 방향 유지함
