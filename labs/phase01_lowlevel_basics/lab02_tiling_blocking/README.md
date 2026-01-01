# lab02_tiling_blocking

## What this is
lab01에서 느려졌던 col_major_like 접근을 **tiling(=blocking)**으로 개선해보는 실험임.  
큰 stride 접근을 작은 블록 안으로 가둬서 캐시 지역성(locality)을 높이는 것이 목표임.

## How to run
### Local / Linux shell
```bash
g++ -O2 -std=c++17 main.cpp -o main
./main
```
### Google Colab
```
!g++ -O2 -std=c++17 main.cpp -o main
!./main
```

## What to change
- `N`을 고정하거나(예: 4096, 8192) 바꿔가며 측정함
- 블록 크기 `B`를 바꿔가며(예: 8, 16, 32, 64) `tiled_col` 성능 변화를 확인함
- 비교 대상 케이스:
  - `row_major` (baseline fast)
  - `col_major_like` (baseline slow)
  - `tiled_col(B=...)` (tiling 적용)

## What to record
- 결과/해석은 `result.md`에 기록함:
  - N, B, 각 케이스 시간(ms), ratio(col/row), ratio(tiled/col) 등을 표로 정리함
  - 어떤 B에서 가장 좋아졌는지와 그 이유(캐시라인/working set 관점)를 짧게 남김
