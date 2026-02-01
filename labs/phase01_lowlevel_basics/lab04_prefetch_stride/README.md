# lab04_prefetch_stride/README.md

## Goal
stride(간격) 접근에서 성능이 언제부터 급격히 나빠지는지 확인하고, 하드웨어 prefetch가 어떤 패턴에서 잘/못 동작하는지 관찰함. 필요하면 `__builtin_prefetch`로 소프트웨어 prefetch도 비교함.

## What to measure
- stride를 1, 2, 4, 8, … 처럼 늘리면서 동일한 총 접근 횟수에서 시간 비교함  
- 옵션: 기본 루프 vs prefetch 힌트 추가 루프

## Hypothesis
연속 접근(stride=1)에 가까울수록 빠르고, stride가 커지면 캐시라인 낭비/TLB 압박으로 느려질 것임. 특정 stride 이후에는 prefetch 효과가 약해지거나 사라질 수 있음.

## How to run
- `g++ -O2 -std=c++17 main.cpp -o run`
- `./run`  
- 파라미터를 인자로 받도록 만들면: `./run --n=... --stride_max=... --iters=...`

## What to record (result.md)
- stride별 시간(ms 또는 ns)
- 급격한 전환점이 어디인지
- prefetch 버전이 도움이 된 구간/안 된 구간
- 관찰을 cache line(64B), page(4KB), TLB 관점으로 해석함
