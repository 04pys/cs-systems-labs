#include <bits/stdc++.h>
using namespace std;

static inline long long now_ns() {
  return chrono::duration_cast<chrono::nanoseconds>(
    chrono::steady_clock::now().time_since_epoch()
  ).count();
}

static inline size_t idx(size_t i, size_t n) { return i % n; }

long long bench_stride(const vector<int>& a, size_t stride, bool use_prefetch, size_t dist) {
  volatile long long sum = 0;
  const size_t n = a.size();
  long long t0 = now_ns();

  for (size_t off = 0; off < stride; ++off) {
    // off로 시작해서 s씩 점프하면 i ≡ off (mod stride)인 원소만 방문함
    for (size_t i = off; i < n; i += stride) {
      if (use_prefetch) {
        size_t ni = i + dist * stride;
        if (ni < n) __builtin_prefetch(&a[ni], 0, 1);
      }
      sum += a[i];
    }
  }

  long long t1 = now_ns();
  return (t1 - t0);
}

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  size_t n = 1ull << 26; // 67,108,864 ints ~= 256MB, 캐시 밖으로 크게
  vector<int> a(n);
  for (size_t i = 0; i < n; ++i) a[i] = int(i * 1315423911u);

  vector<size_t> strides = {1,2,4,8,16,32,64,128,256,512,1024,2048};
  vector<size_t> dists   = {1,2,3,4,6,8,11,15,16,18,22,24,32,64};

  // warmup
  (void)bench_stride(a, 1, false, 0);

  for (size_t s : strides) {
    long long base = bench_stride(a, s, false, 0);
    cout << "stride=" << s << " base_ms=" << base / 1e+6;

    for (size_t d : dists) {
      long long pf = bench_stride(a, s, true, d);
      cout << " pf(d=" << d << ")_ms=" << pf / 1e+6;
    }
    cout << "\n";
  }
}
