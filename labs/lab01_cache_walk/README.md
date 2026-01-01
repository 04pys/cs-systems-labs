# lab01_cache_walk

## What this is
Row-major(연속 접근)와 col-major-like(큰 stride 접근)의 **메모리 접근 패턴 차이**가 실행 시간에 어떤 영향을 주는지 측정하는 실험임.

## How to run
### Local / Linux shell
```bash
g++ -O2 -std=c++17 main.cpp -o main
./main

### Google Colab
!g++ -O2 -std=c++17 main.cpp -o main
!./main

## What to change

main.cpp의 int N = ...; 값을 바꿔가며 측정함

예: 256, 512, 1024, 2048, 4096, 8192

출력은 warmup_row, row_major, col_major_like 3줄이 나오며, time_ms를 비교함

## Notes

첫 실행은 캐시 워밍/페이지 폴트 등으로 흔들릴 수 있어 warmup_row는 워밍업 용도임

결과 및 해석은 result.md에 기록함(각 N별 시간 + 원인 추론)

::contentReference[oaicite:0]{index=0}
