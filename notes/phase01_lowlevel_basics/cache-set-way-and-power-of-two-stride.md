# Cache set/way & power-of-two stride (lab02 수준 정리)

## 왜 set/way 구조를 쓰나
- fully associative(어디든 배치 가능)는 충돌 미스가 적지만, 태그 비교가 비싸고(면적/전력/지연) 고속 캐시에 부담이 큼
- direct-mapped(1-way)는 가장 빠르고 싸지만 충돌 미스가 심함
- set-associative(N-way)는 “비용을 제한하면서 충돌 미스를 줄이는” 현실적 타협임

## conflict miss가 무엇인가
캐시 용량이 충분해도, 서로 다른 주소들이 같은 set에 몰리면 way 수를 초과하면서 서로를 계속 쫓아내는 미스가 발생할 수 있음  
이 현상을 conflict miss(충돌 미스), 심하면 thrashing이라고 부름

## 거듭제곱 stride가 위험한 직관
캐시는 (단순화하면) 주소의 일부 비트로 set을 고름

- cache line이 64B라면 하위 6비트는 line 내부 offset(어느 바이트인지)임
- 그 위의 몇 비트가 set index(어느 set인지)를 고르는 데 쓰임

col_major_like에서 i가 1 증가할 때 주소는 N*4B만큼 증가함  
예: N=4096이면 stride = 4096*4B = 16384B = 2^14

2^14를 더하면 “하위 14비트”는 변하지 않음  
따라서 set index로 쓰이는 비트가 그 범위 안에 걸려 있으면,
i가 증가해도 set index가 거의 바뀌지 않아 특정 set(또는 소수의 set)로 몰릴 수 있음  
→ way 초과 시 충돌 미스가 커질 수 있음

## tiling이 완화하는 이유(직관)
col_major_like는 i를 0..N-1까지 길게 돌면서 같은 set에 매우 많은 라인이 몰릴 수 있음  
tiling은 i 범위를 B로 제한해, “한 번에” 같은 set에 몰리는 라인 수를 줄여 way 범위 안에서 버틸 확률을 높이는 방향으로 작동할 수 있음

## 정리
- set/way는 속도/비용을 위한 구조이며 충돌 미스를 완전히 없애진 못함
- 거듭제곱 stride는 set 충돌을 크게 만들 수 있음
- tiling은 working set 제한 + 재사용 증가 + 충돌 완화로 col-major-like를 개선할 수 있음
