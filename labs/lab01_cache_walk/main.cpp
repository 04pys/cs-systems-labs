#include <bits/stdc++.h>
using namespace std;

static inline long long now_ns() {
  return chrono::duration_cast<chrono::nanoseconds>(
    chrono::steady_clock::now().time_since_epoch()
  ).count();
}

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  int N = 4; // 필요하면 2048, 4096, 8192로 바꿔가며 실험 가능함
  vector<int> a((long long)N * N, 1);

  auto bench = [&](const string& name, bool row_major) {
    volatile long long sum = 0; // 최적화로 루프가 사라지는 것 방지
    long long t0 = now_ns();

    if (row_major) {
      for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
          sum += a[(long long)i * N + j];
    } else {
      for (int j = 0; j < N; j++)
        for (int i = 0; i < N; i++)
          sum += a[(long long)i * N + j];
    }

    long long t1 = now_ns();
    double ms = (t1 - t0) / 1e6;
    double ns = (t1 - t0);
    cout << name << " N=" << N << " time_ns=" << ns << " sum=" << sum << "\n";
  };

  // 워밍업(첫 실행은 캐시/페이지 폴트 등으로 흔들릴 수 있음)
  bench("warmup_row", true);

  // 측정 2개
  bench("row_major", true);
  bench("col_major_like", false);

  return 0;
}
