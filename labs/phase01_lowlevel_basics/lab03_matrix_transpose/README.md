# lab03_matrix_transpose

## 목적
행렬 전치(transpose)에서 **naive 구현**과 **tiled(blocked) 구현**의 성능 차이를 확인함.  
특히 row-major 메모리 레이아웃에서 전치가 왜 느려지기 쉬운지(접근 패턴/캐시라인 재사용 관점)를 체감하고, tiling이 이를 얼마나 완화하는지 관찰함.

---

## 실험 배경(이 실험을 하는 이유)
- 행렬 `A`가 1D 배열로 `A[i*N + j]`에 저장되어 있다면, 이는 **row-major 레이아웃**임.
- 전치 연산은 `B[j*N + i] = A[i*N + j]` 형태가 되는데,
  - `A` 읽기(read)는 보통 연속 접근(row-major에 유리)으로 될 수 있지만
  - `B` 쓰기(write)는 큰 stride로 진행되어 cache line을 낭비하기 쉬움.
- 따라서 전치는 “읽기 또는 쓰기 중 한쪽이 stride가 되는” 비대칭 패턴이 생기기 쉬워서, **tiling이 가장 자주 등장하는 대표 사례**임.

---

## 핵심 아이디어(코드 작성 전에 머릿속에 그릴 그림)

### 1) Naive transpose
전체를 한 번에 전치함:
- (i, j)를 전 범위로 순회하며 `B[j*N+i]`에 즉시 기록함
- 한쪽(read 또는 write)이 stride가 되기 쉬움 → 캐시 미스/세트 충돌/TLB 압박 가능성이 커짐

### 2) Tiled(Blocked) transpose
(i, j) 공간을 **BxB 타일**로 쪼개서 처리함:
- (ii, jj) 타일을 잡고, 그 타일 내부에서만 전치를 수행함
- 목표:
  - 한 번 가져온 캐시라인/페이지를 타일 내부에서 더 많이 재사용
  - working set을 제한하여 캐시에서 버틸 확률을 높임
- B가 너무 작으면 재사용 이득이 작고 오버헤드가 커질 수 있음
- B가 너무 크면 working set이 커져 다시 불리해질 수 있음  
→ 중간 B에서 최적점이 나타날 가능성이 있음(lab02의 “optimal B” 직관과 동일)

---

## 실험에서 비교할 케이스(권장)
필수:
1) `naive_transpose`
2) `tiled_transpose(B)` for B in {8, 16, 32, 64}

선택(확장):
- 아래 “Optional extension” 참고함

---

## 측정 방법(벤치마크 기본 규칙)
- 컴파일 옵션은 고정: `g++ -O2 -std=c++17`
- 워밍업 1회 후 측정(첫 실행 흔들림 완화)
- correctness 확인:
  - checksum(sum) 또는 일부 샘플 비교로 결과가 맞는지 검증
  - 성능만 보고 틀린 전치를 “빠르다”로 착각하지 않도록 주의함

---

## 실행 방법

### Local / Linux shell
```
g++ -O2 -std=c++17 main.cpp -o main
./main
```

### Google Colab
```
!g++ -O2 -std=c++17 main.cpp -o main
!./main
```

---

## 실험 파라미터(추천)
- N: 2048, 4096 (시간 여유 있으면 8192)
- B: 8, 16, 32, 64 (필요하면 128 추가)

---

## 기록할 것(result.md에 남길 내용)
- N, B, 각 케이스의 time_ms, correctness(checksum), 그리고 ratio를 표로 남김

예시 포맷:
```
| N | case | B | time_ms | checksum | note |
|---:|---|---:|---:|---:|---|
| 4096 | naive | - |  |  |  |
| 4096 | tiled | 8 |  |  |  |
| 4096 | tiled | 16 |  |  |  |
| 4096 | tiled | 32 |  |  |  |
| 4096 | tiled | 64 |  |  |  |
```

---

## 기대 관찰(가설)
- `naive_transpose`는 전치 특성상 read/write 중 한쪽이 stride가 되어 느려질 가능성이 큼
- `tiled_transpose`는 타일 내부 재사용으로 성능이 개선될 가능성이 큼
- B는 너무 작거나 너무 크면 손해가 생기고, 중간 값에서 최적이 나타날 가능성이 있음

---

## lab02와의 연결 포인트
- lab02에서 관찰한 “B에 최적점이 생기는 이유(재사용 vs working set/오버헤드)”가 transpose에서도 그대로 반복될 것인지 확인하는 실험임.

---

## Optional extension: read-row-major vs write-row-major (still on-topic)

Transpose는 `B[j*N + i] = A[i*N + j]` 형태라서 read(A)와 write(B) 중 한쪽이 stride가 되기 쉬운 구조임.  
따라서 “어느 쪽을 연속(row-major)으로 만들지”를 비교해보면 전치의 병목이 read인지 write인지 감을 잡는 데 도움이 됨. tiling 실험과도 잘 맞는 확장임.

### A) Naive 2종 비교(루프 순서만 바꿈)

#### 1) `naive_read_rowmajor` (A를 연속으로 읽기)
- 루프: i outer, j inner  
- `A[i*N + j]`는 연속(read-friendly)  
- `B[j*N + i]`는 stride(write-unfriendly)

```
for (int i = 0; i < N; ++i)
  for (int j = 0; j < N; ++j)
    B[(long long)j*N + i] = A[(long long)i*N + j];
```

#### 2) `naive_write_rowmajor` (B를 연속으로 쓰기)
- 루프: j outer, i inner  
- `B[j*N + i]`는 연속(write-friendly)  
- `A[i*N + j]`는 stride(read-unfriendly)

```
for (int j = 0; j < N; ++j)
  for (int i = 0; i < N; ++i)
    B[(long long)j*N + i] = A[(long long)i*N + j];
```

**기대 관찰**  
두 버전 모두 한쪽이 stride라 느려질 수 있지만, 환경에 따라 read 연속이 더 유리할 때도 있고 write 연속이 더 유리할 때도 있음. 이 비교로 전치에서 병목이 read인지 write인지 감을 잡을 수 있음.

---

### B) Tiled에서도 2종 비교(타일 내부 루프 순서만 바꿈)

tiling 구조(ii, jj로 블록) 자체는 유지하고, **타일 내부에서 inner loop를 무엇으로 두는지**만 바꿔서 비교함. (B는 16/32/64 정도만 먼저 해도 경향 확인 가능)

#### 1) `tiled_read_friendly(B)` (타일 내부에서 j를 inner로)
- 타일 내부에서 `A[i*N + j]`를 연속으로 읽는 방향  
- write(B)는 상대적으로 불리할 수 있음

```
for (int ii = 0; ii < N; ii += B)
  for (int jj = 0; jj < N; jj += B) {
    int i_end = min(ii + B, N);
    int j_end = min(jj + B, N);
    for (int i = ii; i < i_end; ++i)
      for (int j = jj; j < j_end; ++j)
        B[(long long)j*N + i] = A[(long long)i*N + j];
  }
```

#### 2) `tiled_write_friendly(B)` (타일 내부에서 i를 inner로)
- 타일 내부에서 `B[j*N + i]`를 연속으로 쓰는 방향  
- read(A)는 상대적으로 불리할 수 있음

```
for (int ii = 0; ii < N; ii += B)
  for (int jj = 0; jj < N; jj += B) {
    int i_end = min(ii + B, N);
    int j_end = min(jj + B, N);
    for (int j = jj; j < j_end; ++j)
      for (int i = ii; i < i_end; ++i)
        B[(long long)j*N + i] = A[(long long)i*N + j];
  }
```

**기대 관찰**  
tiling은 working set을 제한해 성능을 끌어올릴 수 있지만, 타일 내부 루프 순서에 따라 read-friendly / write-friendly가 갈리면서 성능 차이가 추가로 나타날 수 있음.

---

### 기록 팁(result.md)
- 최소 케이스(권장):  
  naive_read_rowmajor, naive_write_rowmajor, tiled_read_friendly(B=16/32/64), tiled_write_friendly(B=16/32/64)  
- 각 케이스에서 time_ms와 checksum을 기록하고, `naive_write / naive_read`, `tiled_write / tiled_read` ratio를 같이 남기면 해석이 쉬움
