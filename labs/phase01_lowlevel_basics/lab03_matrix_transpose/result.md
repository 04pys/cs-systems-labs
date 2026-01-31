# lab03_matrix_transpose — results & notes

## 0) What I tested
N=4096에서 transpose 4종 비교 실험 진행함.

- naive_read_rowmajor: read(A) 연속 / write(B) stride
- naive_write_rowmajor: write(B) 연속 / read(A) stride
- tiled_read_friendly(B): 타일 내부에서 j inner (read 연속 성향)
- tiled_write_friendly(B): 타일 내부에서 i inner (write 연속 성향)
- B in {16, 32, 64}

모든 케이스에서 ok=true 확인함.

---

## 1) Raw measurements (3 runs)

### Run 1
- naive_read_rowmajor: 318.166 ms
- naive_write_rowmajor: 262.850 ms
- tiled_read_friendly B=16: 126.610 ms
- tiled_write_friendly B=16: 46.019 ms
- tiled_read_friendly B=32: 82.519 ms
- tiled_write_friendly B=32: 37.223 ms
- tiled_read_friendly B=64: 79.439 ms
- tiled_write_friendly B=64: 43.065 ms

### Run 2
- naive_read_rowmajor: 302.589 ms
- naive_write_rowmajor: 256.827 ms
- tiled_read_friendly B=16: 128.018 ms
- tiled_write_friendly B=16: 53.393 ms
- tiled_read_friendly B=32: 104.917 ms
- tiled_write_friendly B=32: 59.712 ms
- tiled_read_friendly B=64: 91.219 ms
- tiled_write_friendly B=64: 44.107 ms

### Run 3
- naive_read_rowmajor: 284.549 ms
- naive_write_rowmajor: 225.892 ms
- tiled_read_friendly B=16: 121.316 ms
- tiled_write_friendly B=16: 47.372 ms
- tiled_read_friendly B=32: 83.455 ms
- tiled_write_friendly B=32: 38.863 ms
- tiled_read_friendly B=64: 82.067 ms
- tiled_write_friendly B=64: 45.368 ms

---

## 2) Summary (mean over 3 runs)
단위 ms

| case | mean_ms | notes |
|---|---:|---|
| naive_read_rowmajor | ~301.8 | read row-major / write stride |
| naive_write_rowmajor | ~248.5 | write row-major / read stride |
| tiled_read_friendly B=16 | ~125.3 | 타일링으로 개선 |
| tiled_write_friendly B=16 | ~48.9 | write row-major 효과 크게 관측 |
| tiled_read_friendly B=32 | ~90.3 | B=32에서 개선 폭 큼 |
| tiled_write_friendly B=32 | ~45.3 | 일부 run에서 변동 큼 |
| tiled_read_friendly B=64 | ~84.2 | B=64에서도 좋은 성능 |
| tiled_write_friendly B=64 | ~44.2 | 전체적으로 최상급 |

대략적인 speedup(naive_read 대비)
- naive_write: ~1.21x
- tiled_read_32: ~3.34x
- tiled_write_64: ~6.83x 수준

---

## 3) Main observations
1) naive에서 write를 row-major로 만든 케이스가 read를 row-major로 만든 케이스보다 꾸준히 빠른 결과 관측함  
2) tiling 적용 시 naive 대비 큰 폭의 성능 개선 관측함  
3) 현재 환경에서는 B=32 또는 B=64에서 최적에 가까운 결과 관측함  
4) tiled_write_friendly가 tiled_read_friendly보다 전반적으로 더 빠른 경향 관측함

---

## 4) Why results differ across runs
동일한 코드라도 매 run마다 시간이 다른 이유 정리함.

- N=4096은 A/B 각각 64MB 수준으로, 연산보다 메모리 트래픽과 대역폭에 의해 성능이 좌우되는 작업임  
- Colab 같은 VM 환경에서는 다른 작업과 메모리 대역폭을 공유하거나 스케줄링이 달라져 실행 시간이 흔들릴 수 있음  
- CPU 주파수 변화, 코어 마이그레이션 등으로 캐시 상태가 달라질 수 있음  
- bench 내부에서 fill(B, 0)가 수행되어 대형 연속 write가 측정에 포함되고, 이 때문에 전체 측정이 시스템 메모리 상태에 더 민감해질 수 있음  

따라서 몇 퍼센트에서 수십 퍼센트 정도의 변동은 자연스러운 범위로 판단함.  
다만 naive가 300ms대이고 tiled_write가 40~50ms대처럼 수 배 차이가 나는 부분은 구조적 차이로 해석 가능함.

---

## 5) Why write row-major tends to win
naive_write_rowmajor가 naive_read_rowmajor보다 빠른 이유 정리함.

- 많은 CPU 캐시는 write-allocate 정책을 사용하며, store miss 시 RFO(Read For Ownership)로 캐시라인을 먼저 가져오는 트래픽이 발생할 수 있음  
- stride write는 매 접근마다 새로운 캐시라인에 store를 유발하여 캐시라인 낭비와 메모리 트래픽 증가가 커지기 쉬움  
- 연속 write는 store buffer와 write-combining이 잘 동작하고 캐시라인을 효율적으로 채우는 형태가 되어 유리해지기 쉬움  

그래서 write를 연속으로 만드는 쪽이 더 좋은 결과가 흔히 관측되는 편임.  
다만 non-temporal store 사용 여부나 캐시 정책에 따라 세부 결과는 달라질 수 있음.

---

## 6) Why block size often peaks around 32–64

B가 32나 64 근처에서 좋아지는 핵심은, 캐시를 크게 넘기지 않는 범위에서 재사용을 크게 만들 수 있기 때문임.  
여기에 캐시 라인, TLB, 벡터화, 루프 오버헤드 같은 현실 요인이 겹치며 32 또는 64에서 최적점이 자주 나타나는 편임.

### 6.1) 타일 working set이 L1/L2에 들어가는 구간이 32/64 근처인 경우가 많음
타일링에서 성능이 좋아지려면 타일 내부에서 반복 접근하는 데이터가 L1 또는 최소 L2에 오래 남아야 함.  
커널이 여러 배열을 동시에 붙잡는 형태라면 working set이 대략 배열 수 곱하기 B^2 곱하기 원소 크기 수준이 되기 쉬움.

참고 예시로 3배열 커널에서 double(8B) 기준
- B=32이면 3 * 32^2 * 8 ≈ 24KB로 L1 32KB급에 걸치는 구간이 되기 쉬움  
- B=64이면 3 * 64^2 * 8 ≈ 96KB로 L1은 초과하지만 L2 256KB급에는 들어가는 구간이 되기 쉬움  

transpose는 A와 B 두 배열 중심이고 접근 재사용도 행렬곱과 달라서 위 숫자가 그대로 적용되지는 않음.  
그럼에도 32는 L1 친화, 64는 L2 친화라는 감각은 실험에서 자주 맞는 편임.

### 6.2) 캐시 라인 관점에서 32/64는 규칙성이 좋음
캐시는 보통 64B 라인 단위로 움직임.
- int(4B) 기준으로 한 라인에 16개 원소가 들어감  
- double(8B) 기준으로 한 라인에 8개 원소가 들어감  

B가 32나 64이면 루프 길이와 메모리 패턴이 깔끔해지는 경우가 많아 캐시 라인 낭비가 줄고, prefetcher가 규칙성을 잘 타며, SIMD 벡터화에서도 유리해질 수 있음.

### 6.3) TLB 관점에서도 32/64가 경계가 되는 경우가 많음
TLB는 보통 4KB 페이지 단위로 주소 변환을 캐시함.  
B가 커질수록 한 타일이 걸치는 페이지 수가 늘고 TLB 압박이 커지는 방향임.  
많은 실험에서 32에서 64로 갈 때는 아직 괜찮다가 128 이상에서 TLB miss나 캐시 충돌이 늘며 성능이 꺾이는 모양이 관측되기도 함.

### 6.4) 루프 오버헤드와 재사용 이득의 균형점이 32/64에서 잡히기 쉬움
- B가 너무 작으면 타일 경계 처리, 인덱스 계산, 분기 같은 고정 비용 비중이 커지고 재사용 기회도 작음  
- B를 키우면 재사용이 늘고 고정 비용 비중이 줄어듦  
- 하지만 너무 커지면 working set이 커져 캐시와 TLB, 세트 충돌로 재사용이 깨질 수 있음  

상승 구간과 하강 구간이 교차하는 지점이 환경에 따라 32 또는 64 근처에서 자주 나타나는 편임.

### 6.5) 결론 정리
- 캐시 크기를 크게 넘기지 않는 범위에서 재사용을 크게 만들면 성능이 좋아지는 경향이 있음  
- 여기에 캐시 라인 정렬, SIMD 처리의 깔끔함, TLB와 세트 충돌 임계점이 겹치며 32나 64에서 최적점이 자주 나타남  
- 커널 종류, 원소 타입, CPU 캐시 구성에 따라 최적 B는 달라질 수 있음  

---

## 7) Template note (callable benchmark pattern)
template <class Fn>에서 Fn은 라이브러리 클래스가 아니라, 호출 시점에 컴파일러가 추론하는 타입 자리표시자임.  
bench는 마지막 인자로 호출 가능한 것을 받아 fn()으로 실행하며 시간을 측정하는 구조임.

- 측정 프레임을 공통으로 두고 실험 대상만 람다로 교체 가능함  
- 템플릿은 타입이 컴파일 타임에 확정되어 인라이닝과 최적화 가능성이 높고 오버헤드가 적은 편임  
- std::function<void()>는 간접 호출이나 할당 가능성으로 벤치마크 잡음이 될 수 있어 템플릿 방식이 유리한 경우가 많음  
