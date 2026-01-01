# Cache locality & access pattern (lab01 takeaway)

## 1) One-line summary
연속 접근(row-major)은 캐시라인 재사용과 prefetch가 잘 작동해 빠르고, 큰 stride 접근(col-major-like)은 캐시라인 낭비/TLB 압박으로 느려질 수 있음

## 2) Vocabulary
- **cache line**: CPU가 메모리에서 가져오는 기본 단위(보통 64B)임  
- **spatial locality**: 가까운 주소를 연속으로 접근할수록 한 cache line을 더 많이 재사용 가능함  
- **stride**: 연속 접근이 아닌 경우, 다음 접근 주소가 얼마나 점프하는지(바이트 단위)임  
- **TLB**: 가상주소→물리주소 변환을 캐시해둔 구조임(페이지 단위)  
- **prefetcher**: 다음에 쓸 데이터를 미리 가져오려는 하드웨어 기능임(연속 패턴에 강함)

## 3) What changes between row-major and col-major-like
- 데이터는 1D로 `a[i*N + j]`에 저장되어 있고, 이 레이아웃은 **row-major**임  
- **row_major** 루프는 `j`가 안쪽이라 주소가 `+4B`씩 증가(연속 접근)함  
- **col_major_like** 루프는 `i`가 안쪽이라 주소가 `+N*4B`씩 증가(stride 접근)함  
  - 예: N=4096이면 stride = 4096*4B = 16KiB

## 4) Why performance diverges as N grows
- row_major는 한 cache line(64B)을 가져오면 그 안의 여러 int를 연달아 사용 가능 → 캐시라인 재사용률↑  
- col_major_like는 stride가 커질수록 매 접근마다 다른 cache line을 건드릴 확률↑ → 캐시 미스↑, 하위 계층(L2/L3/DRAM) 접근↑ 가능  
- 페이지를 넓게 건드리면 TLB 엔트리 압박이 커져 TLB miss 증가 가능성 존재  
- 연속 접근은 prefetcher가 예측하기 쉽지만, 큰 stride는 예측/효율이 떨어질 수 있음

## 5) My lab01 evidence (from result.md)
- N=4에서는 차이 거의 없음(전체 데이터가 64B 수준이라 캐시라인 1개로 해결 가능했음)  
- N이 커질수록 ratio(col/row)가 증가했고, N=8192에서 특히 큰 격차가 관측되었음  
  - 자세한 수치는 `labs/lab01_cache_walk/result.md` 참고함

## 6) Bridge to CUDA (next motivation)
- CPU의 연속 접근/캐시라인 재사용 개념은 GPU에서 **coalescing**과 유사한 사고방식으로 연결 가능함  
- col 접근을 빠르게 만들려면 CPU에서는 **tiling(블로킹)**, GPU에서는 **shared memory tiling**을 떠올릴 수 있음
