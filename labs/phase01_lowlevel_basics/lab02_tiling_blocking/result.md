# Result - lab02_tiling_blocking

## Setup
- Environment: Google Colab
- Compiler: g++ (-O2 -std=c++17)
- Workload: `vector<int> a(N*N, 1)` 전체 합산(sum) 수행함
- Cases:
  - `row_major`: (i outer, j inner) 연속 접근 패턴임
  - `col_major_like`: (j outer, i inner) stride 접근 패턴임
  - `tiled_col(B)`: (i,j) 공간을 B×B 블록으로 쪼개서 col-major-like를 완화하는 패턴임
- Note: `warmup_row`는 초기 흔들림(캐시 워밍/페이지 폴트 등) 완화용 워밍업 측정값임

---

## Raw outputs

### N = 4096
```
warmup_row N=4096 time_ms=36.5356 sum=16777216
row_major N=4096 time_ms=41.4764 sum=16777216
col_major_like N=4096 time_ms=234.087 sum=16777216
tiled_col N=4096 B=1 time_ms=494.682 sum=16777216
tiled_col N=4096 B=2 time_ms=173.691 sum=16777216
tiled_col N=4096 B=4 time_ms=155.983 sum=16777216
tiled_col N=4096 B=8 time_ms=110.584 sum=16777216
tiled_col N=4096 B=16 time_ms=83.4921 sum=16777216
tiled_col N=4096 B=32 time_ms=76.3236 sum=16777216
tiled_col N=4096 B=64 time_ms=81.9885 sum=16777216
tiled_col N=4096 B=128 time_ms=83.9152 sum=16777216
tiled_col N=4096 B=256 time_ms=87.8456 sum=16777216
```

### N = 8192
```
warmup_row N=8192 time_ms=141.795 sum=67108864
row_major N=8192 time_ms=144.349 sum=67108864
col_major_like N=8192 time_ms=935.589 sum=67108864
tiled_col N=8192 B=1 time_ms=2333.78 sum=67108864
tiled_col N=8192 B=2 time_ms=1143.54 sum=67108864
tiled_col N=8192 B=4 time_ms=778.164 sum=67108864
tiled_col N=8192 B=8 time_ms=499.459 sum=67108864
tiled_col N=8192 B=16 time_ms=389.186 sum=67108864
tiled_col N=8192 B=32 time_ms=348.018 sum=67108864
tiled_col N=8192 B=64 time_ms=569.496 sum=67108864
tiled_col N=8192 B=128 time_ms=558.668 sum=67108864
tiled_col N=8192 B=256 time_ms=533.197 sum=67108864
```


---

## Summary tables

### N = 4096 (baseline: row=41.4764ms, col=234.087ms)
- col/row ratio ≈ 234.087 / 41.4764 ≈ **5.64×** 느림

| case | B | time_ms | speedup vs col (col/time) | slowdown vs row (time/row) |
|---|---:|---:|---:|---:|
| row_major | - | 41.4764 | 5.64 | 1.00 |
| col_major_like | - | 234.087 | 1.00 | 5.64 |
| tiled_col | 1 | 494.6820 | 0.47 | 11.93 |
| tiled_col | 2 | 173.6910 | 1.35 | 4.19 |
| tiled_col | 4 | 155.9830 | 1.50 | 3.76 |
| tiled_col | 8 | 110.5840 | 2.12 | 2.67 |
| tiled_col | 16 | 83.4921 | 2.80 | 2.01 |
| tiled_col | 32 | **76.3236** | **3.07** | **1.84** |
| tiled_col | 64 | 81.9885 | 2.86 | 1.98 |
| tiled_col | 128 | 83.9152 | 2.79 | 2.02 |
| tiled_col | 256 | 87.8456 | 2.66 | 2.12 |

- Best B: **32** (col 대비 **3.07×** 개선, row 대비 **1.84×** 느림)

### N = 8192 (baseline: row=144.349ms, col=935.589ms)
- col/row ratio ≈ 935.589 / 144.349 ≈ **6.48×** 느림

| case | B | time_ms | speedup vs col (col/time) | slowdown vs row (time/row) |
|---|---:|---:|---:|---:|
| row_major | - | 144.349 | 6.48 | 1.00 |
| col_major_like | - | 935.589 | 1.00 | 6.48 |
| tiled_col | 1 | 2333.780 | 0.40 | 16.17 |
| tiled_col | 2 | 1143.540 | 0.82 | 7.92 |
| tiled_col | 4 | 778.164 | 1.20 | 5.39 |
| tiled_col | 8 | 499.459 | 1.87 | 3.46 |
| tiled_col | 16 | 389.186 | 2.40 | 2.70 |
| tiled_col | 32 | **348.018** | **2.69** | **2.41** |
| tiled_col | 64 | 569.496 | 1.64 | 3.95 |
| tiled_col | 128 | 558.668 | 1.67 | 3.87 |
| tiled_col | 256 | 533.197 | 1.75 | 3.69 |

- Best B: **32** (col 대비 **2.69×** 개선, row 대비 **2.41×** 느림)

---

## Interpretation

### 1) 왜 `tiled_col(B=1)`이 최악인지
- B=1이면 사실상 col-major-like를 “블록으로 쪼개기”만 하고, 지역성 이득은 거의 없고 루프가 2겹(jj/ii) 추가되는 형태임  
- 결과적으로 `col_major_like`보다도 느려지는 결과 관측임  
- 즉, B=1은 tiling의 장점(재사용) 없이 오버헤드만 증가하는 설정임

### 2) B가 커질수록 빨라지다가(B=2→32) 다시 느려지는 이유(B≥64)
- B가 커질수록 같은 row(i 고정)에서 `j`를 연속으로 더 많이 만지게 되어 캐시라인(예: 64B) 재사용이 증가하는 경향임  
  - `int(4B)` 기준으로 캐시라인 1개에 최대 16개 원소가 들어가므로, B가 8~32 구간에서 “한 번 가져온 라인에서 여러 번 쓰는” 효과가 커지는 구조임  
- 동시에, 블록 내 작업집합(working set)도 대략 `B×B×4B`로 증가하는 구조임  
  - B가 너무 커지면(예: 64 이상) 블록 내에서 동시에 유지해야 할 캐시라인/세트 충돌 가능성이 커지고, TLB/캐시 압박이 증가하여 이득이 감소하는 경향임  
- 이 trade-off로 인해 **중간 B(이번 측정에서는 32)가 최적**으로 관측된 결과임

### 3) 왜 `tiled_col`이 `row_major`만큼 빨라지진 않는지
- `tiled_col`은 “col-major-like의 나쁨을 완화”하는 기법이지, 저장 레이아웃 자체를 row-major 연속 접근처럼 완전히 바꾸는 기법이 아님  
- 타일링으로 재사용을 만들었어도, 여전히 (i,j) 순서/stride 영향과 추가 루프 오버헤드가 남는 구조임  
- 그래서 최적 B에서도 `row_major` 대비 1.84×(N=4096), 2.41×(N=8192) 느린 결과 관측임

---

## N=4096 vs N=8192에서 유의미한 변화 여부

- **최적 B는 둘 다 32로 동일**한 결과임  
- 하지만 N이 더 커진 8192에서는
  - baseline col/row 격차가 더 커짐(5.64× → 6.48×)  
  - 동시에 tiling의 “최대 개선폭”은 약간 감소함(4096에서 3.07×, 8192에서 2.69×)  
  - 그리고 큰 B(64 이상)에서 성능 저하가 더 크게 나타나는 결과임  
- 해석: N이 커질수록 작업집합/페이지 범위가 커지고(TLB/캐시 압박 증가 가능성), 큰 타일이 그 압박을 더 키워서 B가 너무 큰 구간이 더 불리해지는 경향으로 해석 가능함

---
