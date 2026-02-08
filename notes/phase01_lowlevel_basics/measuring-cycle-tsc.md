# Timestamp counter로 사이클 측정하기: rdtsc/rdtscp + lfence 패턴 정리

이 노트는 lab04 test.cpp에서 실제로 사용한 방식 기준으로, C++에서 사이클 단위로 시간을 재기 위해 rdtsc, rdtscp, lfence를 어떻게 조합하는지 정리한 메모임.  
나중에 다른 실험에서 cycle 기반으로 측정할 때 그대로 가져다 쓰는 용도임.

---

## 1) 왜 ns가 아니라 cycle로 재려고 했나
- Colab 같은 공유 환경에서는 ns(ms) 측정이 흔들릴 때가 많았음
- 특히 prefetch 실험은 개선 폭이 몇 퍼센트 수준인 경우가 많아서, 흔들림이 결과를 덮어버릴 수 있음
- rdtsc 기반으로 cycle을 재면, 같은 머신 안에서는 비교가 더 뾰족해지는 경우가 많음  
  - 물론 이것도 완벽하진 않아서 fence, 스케줄링, 터보/클럭 변화 같은 현실 요인이 여전히 있음

---

## 2) rdtsc와 rdtscp가 무엇인지
### rdtsc
- CPU의 timestamp counter(TSC)를 읽는 명령임
- 결과는 64비트 값으로, 대략 CPU 시간이 흐를수록 증가함
- 문제는 rdtsc 자체가 명령 실행 순서를 강하게 잡아주지 않아서
  - 앞뒤 코드와 섞여 실행되거나
  - out-of-order 실행 때문에 측정 구간 경계가 흐려질 수 있음

### rdtscp
- rdtsc와 비슷하게 TSC를 읽지만, 추가로 보조 정보(TSC_AUX)를 읽고
- 중요한 점은 rdtscp는 실행 완료 시점에서 어느 정도 순서를 보장하는 성질이 있어 측정 종료에 자주 씀
- 다만 rdtscp도 완전한 serialize는 아니고, fence를 같이 붙여서 더 안전하게 만드는 편임

---

## 3) lfence가 왜 필요한지
### fence가 필요한 이유
CPU는 성능을 위해 명령을 순서대로만 실행하지 않음  
측정 코드에서 원하는 건 아래임

- begin을 찍은 다음에 커널이 실행되도록 하기
- 커널이 끝난 뒤에 end를 찍도록 하기

그런데 out-of-order로 인해
- begin이 커널 안쪽으로 밀려 들어가거나
- 커널 끝 부분이 end 뒤로 밀리는 일이 생기면
측정 구간이 실제 구간과 달라짐

### lfence
- load fence임
- 인텔/AMD에서 많이 쓰는 패턴은 lfence가 명령 순서를 강하게 잡아준다고 기대하고
  - begin 앞뒤
  - end 앞뒤
  에 붙여서 경계를 고정하는 방식임

실무적으로 많이 쓰는 형태는 보통 아래 둘 중 하나임

1) lfence; rdtsc; lfence  
2) rdtscp; lfence

---

## 4) begin과 end에서 어셈블리 순서를 다르게 하는 이유
측정의 목표는 경계를 최대한 정확하게 만드는 것임

### begin에서 흔한 패턴: lfence → rdtsc → lfence
- begin은 측정 시작점이므로, begin 전에 실행되던 코드(루프 준비, 변수 세팅 등)가 begin 이후로 밀려오면 안 됨
- lfence로 앞쪽을 정리하고
- rdtsc로 시간을 읽고
- 다시 lfence로 rdtsc 이후의 명령이 앞당겨 실행되는 걸 막아 경계를 고정함

요약하면, begin은 앞으로 튀어나오는 실행을 막는 역할이 강함

### end에서 흔한 패턴: rdtscp → lfence
- end는 측정 종료점이므로, 커널 끝부분이 end 뒤로 밀려나면 안 됨
- rdtscp는 종료 시점에서 순서 보장이 더 유리한 편이라 end에 자주 사용함
- 그 뒤에 lfence를 붙여서 이후 코드(출력, 다음 루프 준비)가 end보다 앞서 실행되지 않게 함

요약하면, end는 뒤로 밀리는 실행을 막는 역할이 강함

---

## 5) C++에서 inline asm를 쓰는 방법 (asm volatile 핵심)
### asm volatile이 의미하는 것
- asm은 컴파일러에게 직접 어셈블리 블록을 삽입하겠다는 의미임
- volatile은 이 블록을 컴파일러가 마음대로 없애거나 합치지 말라는 의미임

주의할 점
- volatile을 붙여도 컴파일러가 주변 코드 재배치를 완전히 막아주는 것은 아님
- 그래서 보통 아래의 두 가지를 같이 사용함
  1) memory clobber
  2) fence( lfence / mfence 등 )

---

## 6) 출력, 입력, clobber 문법 (그냥 이거만 기억해도 됨)
inline asm 기본 형태는 이렇게 생김

asm volatile(
  "어셈블리 문자열"
  : 출력 operands
  : 입력 operands
  : clobber 목록
);

### 출력 operands
- 어셈블리 결과가 들어갈 C++ 변수를 지정함
- 예를 들어 rdtsc는 결과가 EDX:EAX에 들어가니
  - eax를 받을 변수
  - edx를 받을 변수
  를 출력으로 받아야 함

### 입력 operands
- 어셈블리가 참조할 입력 값을 C++ 변수로 전달함

### clobber
- 이 asm이 어떤 레지스터나 상태를 망가뜨리는지 컴파일러에게 알려주는 목록임
- 자주 쓰는 것
  - "rax", "rdx" 같은 레지스터 이름
  - "memory"

#### memory clobber가 중요한 이유
memory를 clobber로 주면 컴파일러는
- 이 asm 블록 전후로 메모리 접근(load/store)의 재배치를 줄이려고 함
- 즉, 측정 경계 주변에서 코드가 섞이지 않도록 하는 데 도움이 됨

---

## 7) test.cpp에서 본 rdtsc_begin / rdtsc_end가 하는 일
### rdtsc_begin의 의도
- begin은 커널 실행보다 확실히 앞에 있어야 함
- 그래서 begin에서는 fence로 앞뒤를 묶는 패턴을 쓰는 편이 일반적임

대략 역할
1) begin 전에 있던 작업들이 begin 뒤로 넘어오지 않게 함
2) begin 이후 커널 코드가 begin 앞에 섞이지 않게 함

### rdtsc_end의 의도
- end는 커널 실행보다 확실히 뒤에 있어야 함
- 그래서 end에서는 rdtscp로 읽고 fence로 마무리하는 조합을 쓰는 편이 일반적임

대략 역할
1) 커널 코드가 end 뒤로 밀리지 않게 함
2) end 이후 코드가 end 앞에 끼어들지 않게 함

---

## 8) 측정 안정화용 thrash 버퍼가 뭔가
cycle 측정은 캐시 상태에 민감함  
특히 실험을 반복할 때, 직전 실험의 캐시 적중 상태가 다음 실험을 유리하게 만들 수 있음

thrash 버퍼는 간단히 말하면
- 충분히 큰 배열을 한 번 쭉 훑어서
- L1/L2/L3의 유효 라인을 어느 정도 밀어내고
- 매 실험을 비슷하게 cold-ish한 상태로 만들려는 장치임

완전한 초기화는 불가능하지만, 실험 간 편차를 줄이는 데 도움이 되는 경우가 많음

---

## 9) 실전에서 기억할 체크리스트
- rdtsc만 단독으로 쓰면 경계가 흔들릴 수 있음
- begin은 lfence + rdtsc + lfence 형태가 안정적인 편
- end는 rdtscp + lfence 형태가 안정적인 편
- asm volatile만으로는 부족할 수 있어서 memory clobber와 fence가 같이 들어가는 경우가 많음
- 실험 반복 시 캐시 상태가 결과를 왜곡할 수 있어 thrash 같은 완화 기법을 고려함

---

## 10) 다음에 이걸 어디에 재사용할까
- prefetch 실험처럼 개선 폭이 작고 노이즈에 민감한 실험
- cache miss penalty를 cycles로 역산하고 싶은 실험
- 여러 커널을 비교할 때 ns 대신 cycle 기반으로 정밀하게 비교하고 싶은 실험

필요하면 다음 노트로 분리해서 정리할 것
- rdtsc 측정의 함정: turbo/클럭 변화, core migration, invariant TSC 여부
- perf(counter)로 cycles 대신 miss/event를 직접 보는 방법
