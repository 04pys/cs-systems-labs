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

  int N = 8192;                 // 필요 시 2048, 4096, 8192로 변경 가능함
  vector<int> a((long long)N * N, 1);

  auto bench_rowcol = [&](const string& name, bool row_major) {
    volatile long long sum = 0;
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
    cout << name << " N=" << N << " time_ms=" << ms << " sum=" << sum << "\n";
  };

  auto bench_tiled_col = [&](int B) {
    volatile long long sum = 0;
    long long t0 = now_ns();

    // col-major-like( j outside, i inside )를 유지하되
    // (i, j)를 BxB 블록으로 쪼개서 작은 작업집합으로 처리하는 구조임
    for (int jj = 0; jj < N; jj += B) {
      int j_end = min(jj + B, N);
      for (int ii = 0; ii < N; ii += B) {
        int i_end = min(ii + B, N);
        for (int j = jj; j < j_end; ++j) {
          for (int i = ii; i < i_end; ++i) {
            sum += a[(long long)i * N + j];
          }
        }
      }
    }

    long long t1 = now_ns();
    double ms = (t1 - t0) / 1e6;
    cout << "tiled_col"
         << " N=" << N
         << " B=" << B
         << " time_ms=" << ms
         << " sum=" << sum << "\n";
  };

  // 워밍업
  bench_rowcol("warmup_row", true);

  // baseline
  bench_rowcol("row_major", true);
  bench_rowcol("col_major_like", false);

  // tiled variants
  for (int B : {1,2,4,8, 16, 32, 64, 128, 256}) {
    bench_tiled_col(B);
  }

  return 0;
}
