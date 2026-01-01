# lab01_cache_walk

목적: 같은 데이터라도 접근 순서(row-major vs column-ish)가 성능에 큰 영향을 주는지 확인함  
가설: 연속 접근이 랜덤/큰 stride 접근보다 빠를 것임  
실행: main.cpp 컴파일 후 실행, N을 바꿔가며 시간 측정함  
기록: result.md에 N, 시간(ms), 해석을 남김
