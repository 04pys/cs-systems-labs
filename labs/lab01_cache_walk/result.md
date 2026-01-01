# Result - lab01_cache_walk

## Setup
- Compiler: g++ (-O2 -std=c++17)
- Environment: Google Colab
- Measurement:
  - N=4096: time_ms 출력
  - N=4: time_ns 출력(작은 크기에서 접근 패턴 확인용)

## Timing results

### N = 4096 (time_ms)
| case | time_ms | sum |
|---|---:|---:|
| warmup_row | 7.79522 | 16777216 |
| row_major | 7.92456 | 16777216 |
| col_major_like | 102.229 | 16777216 |

- col/row ratio ≈ 102.229 / 7.92456 ≈ **12.90x**

### N = 4 (time_ns)
| case | time_ns | sum |
|---|---:|---:|
| warmup_row | 200 | 16 |
| row_major | 120 | 16 |
| col_major_like | 120 | 16 |

## Access pattern check (N=4)
- row_major index order: `0,1,2,3,4,5,...,15`
- col_major_like index order: `0,4,8,12,1,5,9,13,2,6,10,14,3,7,11,15`

## Interpretation
- **N=4에서는 차이가 거의 없음**
  - 4×4 int = 16개 × 4B = **64B**로, 보통 CPU의 캐시라인(64B) 1개 수준에 데이터가 들어감
  - 따라서 row/col 모두 같은 캐시라인에서 대부분 해결되어 성능 차이가 거의 나타나지 않음

- **N=4096에서 col_major_like가 매우 느려지는 이유(하드웨어 관점)**
  - row_major는 연속 접근이라 캐시라인(예: 64B)을 가져오면 그 안의 여러 int를 연달아 사용함 → **캐시라인 재사용률(공간 지역성)**이 높음
  - col_major_like는 `a[i*N + j]`에서 `i`가 1 증가할 때 주소가 `N*4B`만큼 점프함  
    - N=4096이면 stride = 4096×4B = **16KB**
    - 결과적으로 접근마다 다른 캐시라인/페이지를 건드리게 되어 **캐시 미스가 증가**하고, 더 낮은 메모리 계층(L2/L3/DRAM) 접근 비중이 커질 수 있음
  - 큰 stride 접근은 **TLB(주소 변환 캐시)**에도 불리할 수 있어, 페이지를 넓게 건드리며 TLB 엔트리 압박 → **TLB miss 증가 가능성**이 있음
  - 연속 접근(row_major)은 하드웨어 **prefetcher**가 다음 라인을 예측해 선행 로드하기 유리하지만, 큰 stride 접근(col_major_like)은 예측/효율이 떨어질 수 있음

- **결론(소프트웨어 관점)**
  - 동일한 연산(합산)이라도 **데이터 레이아웃과 루프 순서(접근 패턴)**에 따라 하드웨어(캐시/TLB/prefetcher)를 얼마나 효율적으로 쓰는지가 갈림
  - 이번 실험에서는 그 차이가 N=4096에서 약 **12.9배**로 크게 드러남
