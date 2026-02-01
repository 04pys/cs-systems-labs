# lab06_simd_vectorization/README.md

## Goal
같은 연산이라도 컴파일러 벡터화(SIMD) 여부에 따라 성능이 얼마나 달라지는지 확인함. 벡터화가 잘 되도록 코드를 쓰는 감각을 익힘.

## What to measure
간단한 커널(예: dot product, saxpy, array sum)을 대상으로 버전을 나눠 비교함.
- baseline: 단순 스칼라 루프
- vectorization-friendly: alias를 줄이고, 루프 구조를 단순하게 해서 컴파일러가 벡터화하기 쉽게 작성
- 옵션: intrinsics 버전(가능하면)

## Hypothesis
vectorization-friendly 버전이 baseline보다 빨라질 가능성이 크며, 데이터 정렬/stride/alias 여부에 따라 차이가 달라질 수 있음.

## How to run
- `g++ -O2 -std=c++17 main.cpp -o run`
- `./run`
- 벡터화 확인용: `g++ -O2 -std=c++17 -S main.cpp`로 asm을 확인하거나, 컴파일러 벡터화 리포트를 켤 수 있으면 함께 확인함

## What to record (result.md)
- 버전별 시간 및 speedup
- 벡터화가 됐는지 여부(asm에서 SIMD 명령 사용 여부 등)
- 벡터화가 안 된 경우 원인 추정(분기, alias, 데이터 정렬, 루프 형태)
