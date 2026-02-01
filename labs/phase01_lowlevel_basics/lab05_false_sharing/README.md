# lab05_false_sharing/README.md

## Goal
멀티스레드에서 데이터 배치가 성능을 망치는 대표 사례인 false sharing을 직접 확인함. 같은 연산이라도 캐시라인을 공유하면 병렬 스케일링이 깨질 수 있음을 실험함.

## What to measure
- thread 수를 1, 2, 4, 8, … 로 늘리며 실행 시간 비교함
- 비교 케이스
  - packed: 스레드별 카운터가 인접하게 배치됨(같은 cache line 공유 가능)
  - padded: 스레드별 카운터를 cache line(64B) 단위로 분리함

## Hypothesis
packed는 thread 수가 늘수록 cache line ping-pong으로 성능이 급격히 나빠질 수 있고, padded는 상대적으로 안정적인 스케일링이 나올 것임.

## How to run
- `g++ -O2 -std=c++17 -pthread main.cpp -o run`
- `./run`
- 파라미터를 받도록 만들면: `./run --threads=8 --iters=...`

## What to record (result.md)
- threads별 시간, speedup(1-thread 대비)
- packed vs padded 차이
- 차이를 cache line, coherence 트래픽 관점으로 설명함
