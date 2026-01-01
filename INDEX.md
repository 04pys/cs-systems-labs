# INDEX

이 레포는 **시스템/로우레벨 기본기**를 다시 쌓기 위한 학습 로그 및 실험 코드 모음임  
(원칙: Drive/Colab에서 실험 → GitHub에 “재현 가능한 형태”로 정리하여 남김)

## Directory map
- `logbook/` : 날짜별 학습 기록(세션 단위 회고, 다음 액션 포함)  
- `notes/` : 개념 요약 노트(다시 꺼내보기 쉬운 형태로 정리)  
- `labs/` : 재현 가능한 실험 코드(목표/실행/결과를 파일로 분리해 기록)  
- `notebooks/` : Colab 노트북(.ipynb) 저장(실험 과정/출력/그래프 등)  
- `scripts/` : 빌드/실행/측정 자동화 스크립트(필요 시 추가)

## How to add a new lab
1) `labs/labXX_topic/` 폴더 생성  
2) 최소 파일 3개 추가
   - `README.md` : 목적 + 실행 방법 + 변경할 파라미터 안내  
   - `main.cpp` : 재현 가능한 최종 코드  
   - `result.md` : 측정값(표) + 해석(짧게)  
3) 관련 개념이 있으면 `notes/`에 요약 1개 추가

## Labs
- `labs/lab01_cache_walk/`  
  - row-major vs col-major-like 메모리 접근 패턴에 따른 성능 차이 측정 실험  
  - 실행/결과/해석은 해당 폴더의 README/result 참고

## Notes (recommended reading order)
- `notes/cache-locality.md` : 캐시라인/지역성/stride/TLB/prefetcher 핵심 요약
