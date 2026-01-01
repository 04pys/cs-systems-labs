# Result - lab01_cache_walk

## Setup
- Compiler: g++ (-O2 -std=c++17)
- Environment: Google Colab
- Workload: `vector<int> a(N*N, 1)`을 순회하며 합산(sum) 수행함
- Note: `warmup_row`는 캐시 워밍/페이지 폴트 등 초기 흔들림 완화 목적의 워밍업용 측정값임

## Timing results

### N = 4 (time_ns)
| case | time_ns | sum |
|---|---:|---:|
| warmup_row | 200 | 16 |
| row_major | 120 | 16 |
| col_major_like | 120 | 16 |

### N >= 256 (time_ms)
- 데이터 크기(MiB) = `N*N*4 / 1024^2` 기준임
- stride(KiB) = col_major_like에서 `i`가 1 증가할 때 주소 점프량 `N*4B` 기준임

| N | data size (MiB) | stride (KiB) | warmup_row (ms) | row_major (ms) | col_major_like (ms) | ratio (col/row) |
|---:|---:|---:|---:|---:|---:|---:|
| 256  | 0.25  | 1   | 0.04635 | 0.03391 | 0.08958  | 2.64 |
| 512  | 1.00  | 2   | 0.17416 | 0.11723 | 0.44543  | 3.80 |
| 1024 | 4.00  | 4   | 0.43342 | 0.45221 | 3.98134  | 8.80 |
| 2048 | 16.00 | 8   | 1.89672 | 1.90973 | 24.7508  | 12.96 |
| 4096 | 64.00 | 16  | 7.79522 | 7.92456 | 102.229  | 12.90 |
| 8192 | 256.0 | 32  | 33.6993 | 32.7658 | 1304.5   | 39.81 |

## Access pattern check (N=4)
- row_major index order: `0,1,2,3,4,5,...,15`
- col_major_like index order: `0,4,8,12,1,5,9,13,2,6,10,14,3,7,11,15`
- Note: N=4에서는 전체 데이터가 64B(=16 ints) 수준이라 캐시라인 1개로 대부분 해결 가능하여 차이가 거의 나타나지 않은 것으로 해석 가능함

## Interpretation (HW/SW)
- **N이 작을 때(예: 4)**  
  - 데이터가 캐시라인(보통 64B) 내에서 해결되는 비율이 높아 row/col 차이가 작게 관측된 결과임

- **N이 커질수록 col_major_like가 급격히 느려지는 이유(하드웨어 관점)**  
  - row_major는 연속 접근이므로 캐시라인을 가져오면 그 안의 여러 int를 연달아 사용 가능하여 **캐시라인 재사용률(공간 지역성)**이 높아지는 경향임  
  - col_major_like는 stride가 `N*4B`로 커지며(예: N=4096이면 16KiB 점프), 접근마다 새로운 캐시라인/페이지를 건드릴 확률이 커져 **캐시 미스 증가 및 하위 계층(L2/L3/DRAM) 접근 비중 증가** 가능성임  
  - 큰 stride 접근은 페이지를 넓게 건드리는 경향이 있어 **TLB(주소 변환 캐시) 엔트리 압박 → TLB miss 증가 가능성** 존재함  
  - 연속 접근(row_major)은 하드웨어 prefetcher가 다음 라인을 예측/선행 로드하기 유리하나, 큰 stride(col_major_like)는 예측/효율이 떨어질 수 있는 구조임

- **소프트웨어 관점 결론**  
  - 동일한 합산 연산이라도 **데이터 레이아웃과 루프 순서(접근 패턴)**에 따라 하드웨어(캐시/TLB/prefetcher)를 얼마나 효율적으로 쓰는지가 달라지는 결과임  
  - 본 실험에서 ratio(col/row)가 N 증가에 따라 2.64x → 39.81x까지 커지는 경향 확인 결과임
