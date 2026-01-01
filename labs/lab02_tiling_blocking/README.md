# lab02_tiling_blocking

## What this is
lab01에서 느려졌던 col_major_like 접근을 **tiling(=blocking)**으로 개선해보는 실험임.  
큰 stride 접근을 작은 블록 안으로 가둬서 캐시 지역성(locality)을 높이는 것이 목표임.

## How to run
### Local / Linux shell
```bash
g++ -O2 -std=c++17 main.cpp -o main
./main
