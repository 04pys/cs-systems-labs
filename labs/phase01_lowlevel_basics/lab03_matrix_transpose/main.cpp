#include <bits/stdc++.h>
using namespace std;

static inline long long now_ns() {
  return chrono::duration_cast<chrono::nanoseconds>(
           chrono::steady_clock::now().time_since_epoch())
      .count();
}

// A[i*N + j] is row-major
static inline long long idx(long long i, long long j, int N) {
  return i * (long long)N + j;
}

// ---- Transpose variants ----

// naive_read_rowmajor: i outer, j inner
// - Read A is contiguous
// - Write B is stride
void transpose_naive_read_rowmajor(const vector<int>& A, vector<int>& B, int N) {
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      B[idx(j, i, N)] = A[idx(i, j, N)];
    }
  }
}

// naive_write_rowmajor: j outer, i inner
// - Write B is contiguous
// - Read A is stride
void transpose_naive_write_rowmajor(const vector<int>& A, vector<int>& B, int N) {
  for (int j = 0; j < N; ++j) {
    for (int i = 0; i < N; ++i) {
      B[idx(j, i, N)] = A[idx(i, j, N)];
    }
  }
}

// tiled_read_friendly: tile loops (ii,jj) then i inner over rows, j inner over cols
// - Inside tile: read A tends to be contiguous (j inner)
void transpose_tiled_read_friendly(const vector<int>& A, vector<int>& B, int N, int BS) {
  for (int ii = 0; ii < N; ii += BS) {
    int i_end = min(ii + BS, N);
    for (int jj = 0; jj < N; jj += BS) {
      int j_end = min(jj + BS, N);
      for (int i = ii; i < i_end; ++i) {
        for (int j = jj; j < j_end; ++j) {
          B[idx(j, i, N)] = A[idx(i, j, N)];
        }
      }
    }
  }
}

// tiled_write_friendly: tile loops (ii,jj) then j inner over cols, i inner over rows
// - Inside tile: write B tends to be contiguous (i inner)
void transpose_tiled_write_friendly(const vector<int>& A, vector<int>& B, int N, int BS) {
  for (int ii = 0; ii < N; ii += BS) {
    int i_end = min(ii + BS, N);
    for (int jj = 0; jj < N; jj += BS) {
      int j_end = min(jj + BS, N);
      for (int j = jj; j < j_end; ++j) {
        for (int i = ii; i < i_end; ++i) {
          B[idx(j, i, N)] = A[idx(i, j, N)];
        }
      }
    }
  }
}

// ---- Simple correctness spot-check (cheap) ----
bool spot_check(const vector<int>& A, const vector<int>& B, int N, int samples = 12) {
  std::mt19937 rng(123);
  std::uniform_int_distribution<int> dist(0, N - 1);
  for (int k = 0; k < samples; ++k) {
    int i = dist(rng);
    int j = dist(rng);
    if (B[idx(j, i, N)] != A[idx(i, j, N)]) return false;
  }
  return true;
}

// ---- Benchmark helper ----
struct Result {
  string name;
  int N;
  int BS; // -1 if not tiled
  long long time_ns;
  long long checksum;
  bool ok;
};

template <class Fn>
Result bench(const string& name, int N, int BS, const vector<int>& A, vector<int>& B, Fn fn) {
  // Clear B to avoid reusing prior results (and to keep work consistent)
  std::fill(B.begin(), B.end(), 0);

  long long t0 = now_ns();
  fn();
  long long t1 = now_ns();

  // Volatile checksum to prevent dead-code elimination
  volatile long long sum = 0;
  for (int k = 0; k < 256 && k < (int)B.size(); ++k) sum += B[k];
  // also touch a few far positions
  if (!B.empty()) {
    sum += B.back();
    sum += B[B.size() / 2];
  }

  bool ok = spot_check(A, B, N);

  Result r;
  r.name = name;
  r.N = N;
  r.BS = BS;
  r.time_ns = (t1 - t0);
  r.checksum = (long long)sum; // note: partial checksum, just anti-optimization
  r.ok = ok;
  return r;
}

int main(int argc, char** argv) {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);

  int N = 4096;
  if (argc >= 2) N = atoi(argv[1]); // e.g., ./main 2048

  // Tile sizes to test (you can edit or pass different N)
  vector<int> tile_sizes = {16, 32, 64};

  // Allocate A and B (N*N ints)
  vector<int> A((long long)N * N);
  vector<int> B((long long)N * N);

  // Fill A with a non-trivial pattern (better for correctness checking)
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      // Simple deterministic pattern (avoid all-ones)
      unsigned x = (unsigned)i * 1315423911u + (unsigned)j * 2654435761u;
      A[idx(i, j, N)] = (int)(x % 1000u);
    }
  }

  vector<Result> results;

  // ---- Warmup: run one variant once (reduces first-run noise) ----
  {
    auto r = bench("warmup_naive_read", N, -1, A, B, [&]() {
      transpose_naive_read_rowmajor(A, B, N);
    });
    results.push_back(r);
  }

  // ---- Naive comparisons ----
  results.push_back(bench("naive_read_rowmajor", N, -1, A, B, [&]() {
    transpose_naive_read_rowmajor(A, B, N);
  }));

  results.push_back(bench("naive_write_rowmajor", N, -1, A, B, [&]() {
    transpose_naive_write_rowmajor(A, B, N);
  }));

  // ---- Tiled comparisons (two loop-order variants) ----
  for (int BS : tile_sizes) {
    results.push_back(bench("tiled_read_friendly", N, BS, A, B, [&]() {
      transpose_tiled_read_friendly(A, B, N, BS);
    }));
    results.push_back(bench("tiled_write_friendly", N, BS, A, B, [&]() {
      transpose_tiled_write_friendly(A, B, N, BS);
    }));
  }

  // ---- Print results ----
  // For large N, ms is more readable; still preserve ns
  for (auto& r : results) {
    double ms = r.time_ns / 1e6;
    cout << r.name
         << " N=" << r.N;
    if (r.BS != -1) cout << " B=" << r.BS;
    cout << " time_ms=" << ms
         << " checksum=" << r.checksum
         << " ok=" << (r.ok ? "true" : "false")
         << "\n";
  }

  return 0;
}
