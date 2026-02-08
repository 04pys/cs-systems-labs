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

static uint64_t median_u64(vector<uint64_t>& v) {
  nth_element(v.begin(), v.begin() + v.size()/2, v.end());
  return v[v.size()/2];
}

// 캐시 흔들림 완화용 간단 thrash용 버퍼임
static inline void thrash_cache(vector<uint8_t>& buf) {
  volatile uint64_t s = 0;
  for (size_t i = 0; i < buf.size(); i += 64) s += buf[i];
  (void)s;
}

// stride 접근 순서와 동일한 order를 메모리에 저장하는 next 테이블 생성임
// order는 off=0..s-1, i=off; i<n; i+=s 순서임
// next[i] = 다음으로 갈 인덱스임
static inline uint32_t succ_index(size_t i, size_t n, size_t s) {
  size_t off = i % s;
  size_t ni = i + s;
  if (ni < n) return (uint32_t)ni;
  // 해당 residue 마지막이면 다음 residue의 첫 원소로 이동임
  size_t off2 = off + 1;
  if (off2 < s) return (uint32_t)off2;
  return 0u;
}

static void build_next_in_place(vector<uint32_t>& next, size_t s) {
  const size_t n = next.size();
  for (size_t i = 0; i < n; ++i) next[i] = succ_index(i, n, s);
}

// pointer-chase 레이턴시 측정임
// steps번 의존적 로드를 수행하고 cycles/step을 반환함
static double measure_pointer_chase_cycles_per_step(
    const vector<uint32_t>& next,
    size_t steps,
    int trials,
    vector<uint8_t>* thrash_buf = nullptr) {

  const size_t n = next.size();
  if (steps > n) steps = n;

  vector<uint64_t> cyc;
  cyc.reserve(trials);

  for (int t = 0; t < trials; ++t) {
    if (thrash_buf) thrash_cache(*thrash_buf);

    volatile uint32_t cur = 0;
    volatile uint64_t sink = 0;

    uint64_t t0 = rdtsc_begin();
    for (size_t k = 0; k < steps; ++k) {
      cur = next[cur];     // 의존적 로드 발생함
      sink += cur;         // 제거 방지용 관측값임
    }
    uint64_t t1 = rdtsc_end();
    (void)sink;
    cyc.push_back(t1 - t0);
  }

  uint64_t med = median_u64(cyc);
  return (double)med / (double)steps;
}

// hot에서 stride 루프의 cycles/access를 재기 위한 벤치임
// reps를 키워 총 접근 횟수를 늘릴 수 있게 함
static uint64_t bench_stride_cycles_hot(
    const vector<int>& a,
    size_t stride,
    bool use_prefetch,
    size_t dist,
    size_t reps) {

  volatile long long sum = 0;
  const size_t n = a.size();

  uint64_t t0 = rdtsc_begin();

  for (size_t rep = 0; rep < reps; ++rep) {
    for (size_t off = 0; off < stride; ++off) {
      for (size_t i = off; i < n; i += stride) {
        if (use_prefetch) {
          size_t ni = i + dist * stride;
          if (ni < n) __builtin_prefetch(&a[ni], 0, 1);
        }
        sum += a[i];
      }
    }
  }

  uint64_t t1 = rdtsc_end();
  (void)sum;
  return t1 - t0;
}

static double bench_hot_cycles_per_access_median(
    const vector<int>& hot,
    size_t stride,
    bool use_prefetch,
    size_t dist,
    size_t reps,
    int trials,
    vector<uint8_t>* thrash_buf = nullptr) {

  vector<uint64_t> v;
  v.reserve(trials);
  for (int t = 0; t < trials; ++t) {
    if (thrash_buf) thrash_cache(*thrash_buf);
    v.push_back(bench_stride_cycles_hot(hot, stride, use_prefetch, dist, reps));
  }
  uint64_t med = median_u64(v);
  double total_access = (double)reps * (double)hot.size();
  return (double)med / total_access;
}

// dist 후보 생성은 기존 구조를 유지하되 pred 기반임
static vector<size_t> make_candidates(size_t pred) {
  vector<size_t> c;

  auto push = [&](size_t x) {
    if (x < 1) x = 1;
    if (x > 4096) x = 4096;
    c.push_back(x);
  };

  push(pred / 2);
  push((pred * 3) / 4);
  push(pred);
  push((pred * 5) / 4);
  push((pred * 3) / 2);
  push(pred * 2);

  auto lower_pow2 = [&](size_t x) {
    size_t p = 1;
    while ((p << 1) <= x) p <<= 1;
    return p;
  };
  auto upper_pow2 = [&](size_t x) {
    size_t p = 1;
    while (p < x) p <<= 1;
    return p;
  };
  push(lower_pow2(max<size_t>(1, pred)));
  push(upper_pow2(max<size_t>(1, pred)));

  sort(c.begin(), c.end());
  c.erase(unique(c.begin(), c.end()), c.end());

  if (c.size() > 10) {
    sort(c.begin(), c.end(), [&](size_t a, size_t b) {
      auto da = (a > pred) ? (a - pred) : (pred - a);
      auto db = (b > pred) ? (b - pred) : (pred - b);
      if (da != db) return da < db;
      return a < b;
    });
    c.resize(10);
    sort(c.begin(), c.end());
  }
  return c;
}

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  const size_t n_cold = 1ull << 26; // 256MB 영역임
  vector<uint32_t> next(n_cold);

  const size_t n_hot = 1ull << 13;  // 32KB 근처임
  vector<int> hot(n_hot);
  for (size_t i = 0; i < n_hot; ++i) hot[i] = int(i * 2654435761u);

  vector<size_t> strides = {1,2,4,8,16,32,64,128,256,512,1024,2048};

  // thrash 버퍼는 L3 일부를 흔들기 위한 용도임
  // 너무 크게 잡으면 오히려 시간이 늘 수 있어 64MB 정도로 둠
  vector<uint8_t> thr(64ull << 20, 1);

  // pointer chase steps는 너무 크면 오래 걸리니 일부만 사용함
  // prefix만 돌려도 chain이 큰 메모리 범위를 순회하므로 cold 성격 유지 가능함
  const size_t CHASE_STEPS = 1ull << 22; // 4M steps 임
  const int CHASE_TRIALS = 5;
  const int HOT_TRIALS = 7;

  cout << fixed << setprecision(3);

  // warmup
  thrash_cache(thr);
  (void)bench_hot_cycles_per_access_median(hot, 1, false, 0, 256, 3, &thr);

  for (size_t s : strides) {
    // stride별 next 테이블을 만들어 pointer-chase 레이턴시를 직접 측정함
    build_next_in_place(next, s);

    // 1) L_lat_cycles_per_step 측정임
    double L_lat_cyc = measure_pointer_chase_cycles_per_step(next, CHASE_STEPS, CHASE_TRIALS, &thr);

    // 2) T_iter_cycles_per_access 측정임
    // hot에서 stride 루프의 평균 cycles/access를 측정함
    // reps를 늘려 충분히 긴 구간을 재서 노이즈를 줄임
    size_t reps_hot = 2048; // hot.size()*reps_hot 접근횟수임
    double T_iter_cyc = bench_hot_cycles_per_access_median(hot, s, false, 0, reps_hot, HOT_TRIALS, &thr);

    // 3) dist_pred 계산임
    // dist * T_iter >= L_lat 형태임
    size_t dist_pred = 1;
    if (T_iter_cyc > 0.0) {
      dist_pred = (size_t)ceil(L_lat_cyc / T_iter_cyc);
      if (dist_pred < 1) dist_pred = 1;
      if (dist_pred > 4096) dist_pred = 4096;
    }

    // dist 후보는 pred 기반으로만 생성함
    vector<size_t> cand = make_candidates(dist_pred);

    // cold benchmark는 원래 코드처럼 sum 기반 스트리밍이며, cycle로 측정함
    // next 배열은 uint32_t라 (?) hot과 타입이 달라 bench 함수 재사용이 어려워 별도 배열을 준비함
    // 여기서는 cold 스트리밍을 next 자체를 int로 view 해서 수행함
    // 값의 의미는 중요하지 않고 주소 접근이 중요함
    const int* cold_view = reinterpret_cast<const int*>(next.data());
    size_t cold_n_int = next.size(); // uint32_t == int 가정 환경임

    auto bench_cold_cycles = [&](bool use_pf, size_t dist) -> uint64_t {
      volatile long long sum = 0;
      uint64_t t0 = rdtsc_begin();
      for (size_t off = 0; off < s; ++off) {
        for (size_t i = off; i < cold_n_int; i += s) {
          if (use_pf) {
            size_t ni = i + dist * s;
            if (ni < cold_n_int) __builtin_prefetch(&cold_view[ni], 0, 1);
          }
          sum += cold_view[i];
        }
      }
      uint64_t t1 = rdtsc_end();
      (void)sum;
      return t1 - t0;
    };

    // base와 후보들 비교임
    vector<uint64_t> base_trials;
    base_trials.reserve(7);
    for (int t = 0; t < 7; ++t) {
      thrash_cache(thr);
      base_trials.push_back(bench_cold_cycles(false, 0));
    }
    uint64_t base_cyc = median_u64(base_trials);
    double base_cyc_per_access = (double)base_cyc / (double)cold_n_int;

    uint64_t best_cyc = base_cyc;
    size_t best_dist = (size_t)-1;
    double best_cyc_per_access = base_cyc_per_access;

    for (size_t d : cand) {
      vector<uint64_t> trials;
      trials.reserve(7);
      for (int t = 0; t < 7; ++t) {
        thrash_cache(thr);
        trials.push_back(bench_cold_cycles(true, d));
      }
      uint64_t med = median_u64(trials);
      double cyc_per_access = (double)med / (double)cold_n_int;
      if (med < best_cyc) {
        best_cyc = med;
        best_dist = d;
        best_cyc_per_access = cyc_per_access;
      }
    }

    double improve = (double)(base_cyc - best_cyc) / (double)base_cyc * 100.0;

    cout << "stride=" << s
         << " L_lat_cyc=" << L_lat_cyc
         << " T_iter_cyc=" << T_iter_cyc
         << " dist_pred=" << dist_pred
         << " candidates=[";
    for (size_t i = 0; i < cand.size(); ++i) cout << cand[i] << (i+1==cand.size()? "" : ",");
    cout << "]"
         << " base_cyc_per_access=" << base_cyc_per_access
         << " best_dist=";
    if (best_dist == (size_t)-1) cout << "none";
    else cout << best_dist;
    cout << " best_cyc_per_access=" << best_cyc_per_access
         << " improvement=" << improve << "%\n";
  }

  return 0;
}
