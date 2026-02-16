// labs/lab05_false_sharing/main.cpp
// 목표: false sharing(거짓 공유)로 인한 캐시 라인 ping-pong 비용을 관찰하는 실험 코드
// 환경: Google Colab (x86_64 가정), std::thread 기반 멀티스레드 실행

#include <bits/stdc++.h>
using namespace std;

#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
static inline uint64_t rdtsc_begin() {
  unsigned lo, hi;
  asm volatile("lfence\nrdtsc" : "=a"(lo), "=d"(hi) :: "memory");
  return (uint64_t(hi) << 32) | lo;
}
static inline uint64_t rdtsc_end() {
  unsigned lo, hi;
  asm volatile("rdtscp\nlfence" : "=a"(lo), "=d"(hi) :: "rcx", "memory");
  return (uint64_t(hi) << 32) | lo;
}
#else
#error "RDTSC not supported on this architecture"
#endif

// median 유틸
static uint64_t median_u64(vector<uint64_t>& v) {
  nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
  return v[v.size() / 2];
}

// 캐시 흔들림 완화용 간단 thrash 버퍼
static inline void thrash_cache(vector<uint8_t>& buf) {
  volatile uint64_t s = 0;
  for (size_t i = 0; i < buf.size(); i += 64) s += buf[i];
  (void)s;
}

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
// 코어 pin은 노이즈 감소에 도움이 되지만, Colab 스케줄러 정책상 항상 보장되지는 않음
static inline void pin_thread_to_cpu(int cpu) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}
#else
static inline void pin_thread_to_cpu(int) {}
#endif

// 간단 spin barrier 구현
struct SpinBarrier {
  int total;
  atomic<int> arrived;
  atomic<int> epoch;
  explicit SpinBarrier(int n) : total(n), arrived(0), epoch(0) {}
  void wait() {
    int e = epoch.load(memory_order_relaxed);
    if (arrived.fetch_add(1, memory_order_acq_rel) + 1 == total) {
      arrived.store(0, memory_order_relaxed);
      epoch.fetch_add(1, memory_order_release);
    } else {
      while (epoch.load(memory_order_acquire) == e) {
        _mm_pause();
      }
    }
  }
};

// case1: packed 배열
// 서로 다른 스레드가 서로 다른 원소를 업데이트해도, 같은 캐시 라인(64B)에 여러 counter가 들어가면 false sharing 발생 가능함
struct PackedCounter {
  volatile uint64_t v;
};

// case2: padded 배열
// 각 counter를 64B 정렬/크기로 분리해서 동일 캐시 라인 공유를 회피하는 형태
struct alignas(64) PaddedCounter {
  volatile uint64_t v;
  // alignas(64)로 인해 sizeof가 64로 padding되는 것이 일반적임
};

enum class Layout {
  PACKED,
  PADDED,
};

// 실험 본체
// 각 스레드는 자기 counter 하나만 iters번 증가시키고, 전체 걸린 cycles를 반환
template <typename CounterT>
static uint64_t run_one_trial(
    vector<CounterT>& counters,
    uint64_t iters,
    int tid,
    SpinBarrier& bar,
    bool pin,
    int cpu_base) {

  if (pin) {
    int cpu = cpu_base + tid;
    pin_thread_to_cpu(cpu);
  }

  // 각 스레드는 자기 슬롯만 사용
  volatile uint64_t* p = &counters[tid].v;
  *p = 0;

  bar.wait(); // 시작 정렬용 barrier
  uint64_t t0 = rdtsc_begin();

  for (uint64_t i = 0; i < iters; ++i) {
    (*p)++; // 메모리에 실제 store가 발생하도록 volatile 사용
  }

  uint64_t t1 = rdtsc_end();
  bar.wait(); // 종료 정렬용 barrier

  // 최적화 방지용 관측
  asm volatile("" :: "m"(*p) : "memory");

  return (t1 - t0);
}

// layout별로 trials 반복 후 median cycles/op를 반환
static double bench_layout_cycles_per_inc(
    Layout layout,
    int nthreads,
    uint64_t iters_per_thread,
    int trials,
    vector<uint8_t>& thrash,
    bool pin,
    int cpu_base) {

  vector<uint64_t> total_cycles_trials;
  total_cycles_trials.reserve(trials);

  for (int t = 0; t < trials; ++t) {
    thrash_cache(thrash);

    SpinBarrier bar(nthreads);

    vector<uint64_t> cyc_per_thread(nthreads, 0);
    vector<thread> th;
    th.reserve(nthreads);

    if (layout == Layout::PACKED) {
      vector<PackedCounter> counters(nthreads);
      for (int i = 0; i < nthreads; ++i) counters[i].v = 0;

      for (int tid = 0; tid < nthreads; ++tid) {
        th.emplace_back([&, tid]() {
          cyc_per_thread[tid] = run_one_trial(counters, iters_per_thread, tid, bar, pin, cpu_base);
        });
      }
      for (auto& x : th) x.join();

      // 스레드별 구간이 barrier로 정렬되어 있어 max가 전체 구간 길이에 해당하는 
      uint64_t mx = 0;
      for (auto c : cyc_per_thread) mx = max(mx, c);
      total_cycles_trials.push_back(mx);

    } else {
      vector<PaddedCounter> counters(nthreads);
      for (int i = 0; i < nthreads; ++i) counters[i].v = 0;

      for (int tid = 0; tid < nthreads; ++tid) {
        th.emplace_back([&, tid]() {
          cyc_per_thread[tid] = run_one_trial(counters, iters_per_thread, tid, bar, pin, cpu_base);
        });
      }
      for (auto& x : th) x.join();

      uint64_t mx = 0;
      for (auto c : cyc_per_thread) mx = max(mx, c);
      total_cycles_trials.push_back(mx);
    }
  }

  uint64_t med_total_cycles = median_u64(total_cycles_trials);
  double total_incs = (double)nthreads * (double)iters_per_thread;
  return (double)med_total_cycles / total_incs;
}

// 간단 CLI 파서임
static uint64_t parse_u64(const char* s, uint64_t defv) {
  if (!s) return defv;
  char* endp = nullptr;
  unsigned long long v = strtoull(s, &endp, 10);
  if (endp == s) return defv;
  return (uint64_t)v;
}

int main(int argc, char** argv) {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  // 기본 파라미터임
  // Colab은 코어 수/클럭 스케줄링 변동이 있어 trials를 조금 키우는 편이 안전함
  int hw = (int)thread::hardware_concurrency();
  if (hw <= 0) hw = 4;

  int nthreads = (int)parse_u64((argc >= 2 ? argv[1] : nullptr), (uint64_t)min(8, hw));
  uint64_t iters = parse_u64((argc >= 3 ? argv[2] : nullptr), 50'000'000ULL); // 스레드당 증가 횟수임
  int trials = (int)parse_u64((argc >= 4 ? argv[3] : nullptr), 7ULL);

  // pinning 옵션임
  // 0이면 off, 1이면 on임
  bool pin = false;
  if (argc >= 5) pin = (parse_u64(argv[4], 0) != 0);

  // 코어 베이스 오프셋임
  int cpu_base = 0;
  if (argc >= 6) cpu_base = (int)parse_u64(argv[5], 0);

  // thrash 버퍼임
  // 너무 크면 실험 시간이 늘어나므로 32MB 정도로 둠
  vector<uint8_t> thr(32ull << 20, 1);

  // warmup
  thrash_cache(thr);

  cout << fixed << setprecision(4);

  cout << "lab05 false_sharing\n";
  cout << "nthreads=" << nthreads
       << " iters_per_thread=" << iters
       << " trials=" << trials
       << " pin=" << (pin ? 1 : 0)
       << " hw_threads=" << hw
       << "\n\n";

  // PACKED: false sharing 발생 가능 layout임
  double packed_cpi = bench_layout_cycles_per_inc(Layout::PACKED, nthreads, iters, trials, thr, pin, cpu_base);

  // PADDED: false sharing 회피 layout임
  double padded_cpi = bench_layout_cycles_per_inc(Layout::PADDED, nthreads, iters, trials, thr, pin, cpu_base);

  cout << "result (median-based)\n";
  cout << "layout=packed  cycles_per_inc=" << packed_cpi << "\n";
  cout << "layout=padded  cycles_per_inc=" << padded_cpi << "\n";

  if (padded_cpi > 0.0) {
    double ratio = packed_cpi / padded_cpi;
    cout << "packed_over_padded_ratio=" << ratio << "\n";
  }

  //cout << "\nnotes\n";
  //cout << "- packed는 서로 다른 스레드가 다른 counter를 갱신해도 같은 캐시 라인을 공유하면 invalidation ping-pong이 발생 가능함\n";
  //cout << "- padded는 counter를 캐시 라인 단위로 분리해 false sharing을 회피하는 의도임\n";
 // cout << "- pin=1은 스레드가 다른 코어에 고정될 때 효과가 더 뚜렷해질 수 있으나 Colab에서 항상 보장되지는 않음\n";

  return 0;
}
