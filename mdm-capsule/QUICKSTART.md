# MDM Capsule - Quick Start (5 minutes)

## What Is This?

A **reproducible research framework** for testing whether market microstructure geometry predicts price moves.

Uses **pure fixed-point arithmetic** (no floating-point). Deterministic. Auditable. Production-ready.

## 60-Second Overview

```
Market Data (OHLCV)
    ↓
40-D Manifold Decomposition
  (13-D agent intent + 27-D structural constraints)
    ↓
Spectral Analysis (Eigendecomposition)
  Compute: λ_min, CMI coupling, deformation
    ↓
Lead-Lag Correlation Testing
  Does geometry LEAD VIX / Liquidations / Earnings?
    ↓
Hypothesis Accept / Reject
```

## Installation

### Prerequisites

- **GCC 7+** (or any C99 compiler)
- **Make**
- **~5 MB disk space**

### Build (60 seconds)

```bash
# Extract zip
unzip mdm-capsule.zip
cd mdm-capsule

# Build
make clean && make all

# Run with synthetic data
./build/bin/mdm_capsule
```

**Expected Output:**

```
╔════════════════════════════════════════════════════════════╗
║     MDM REPRODUCIBILITY CAPSULE v1.0 - INITIALIZING       ║
╚════════════════════════════════════════════════════════════╝

✓ Generated 252 price points

HYPOTHESIS 1: λ_min LEADS VIX spikes by 1-3 days

Correlation:  [VALUE]
Optimal Lag:  [DAYS]
Samples:      [COUNT]

✓ MDM CAPSULE EXECUTION COMPLETE
```

## Three Hypotheses to Test

### H1: Eigenvalue Lead-Lag on VIX ⚡

**Claim:** When λ_min (manifold compression) drops sharply, VIX spikes 1-3 days later.

**If true:** Can predict volatility regime shifts days in advance.

**Test:** 
```bash
# 1. Get SPY + VIX data (see docs/INTEGRATION.md)
# 2. Convert to WAD scale
# 3. Run mdm_main with real data
# 4. Check if optimal_lag < 0 (geometry leads)
```

### H2: CMI Decoupling LEADS Liquidations (Crypto) 🔄

**Claim:** When narrative (agent features) diverges from reality (structural features), liquidation cascades follow.

**If true:** Can detect impending flash crashes.

**Test:** Integrate on-chain liquidation data + use CMI metrics.

### H3: Anomalous Agent Features LEAD Earnings Surprises 📊

**Claim:** Insider trading shows up in 13-D agent space 2-4 weeks before earnings.

**If true:** Can beat consensus forecasts.

**Test:** Correlate agent subspace divergence with earnings surprise magnitude.

## File Structure

```
mdm-capsule/
├── src/
│   ├── matrix_fixed_point.c/h    (Linear algebra, 600 lines)
│   ├── mdm_engine.c/h             (Manifold core, 800 lines)
│   └── mdm_main.c                 (Pipeline + testing, 400 lines)
├── docs/
│   ├── THEORY.md                  (Math proofs, framework)
│   └── INTEGRATION.md             (Real data how-to)
├── Makefile                       (Build system)
├── README.md                      (Full docs)
├── QUICKSTART.md                  (This file)
└── LICENSE                        (MIT)
```

## Try It Out (2 minutes)

```bash
# 1. Compile
make clean && make all

# 2. Run (uses synthetic data)
./build/bin/mdm_capsule

# 3. Check results
cat mdm_report.txt
cat mdm_spectrum.csv
```

## Next: Test with Real Data (15 minutes)

See **`docs/INTEGRATION.md`** for:
- Where to get SPY/VIX/Crypto/Earnings data
- How to convert to WAD scale
- Python helpers for data processing
- Backtesting framework

**Quick path:**

```bash
# Download SPY + VIX
wget https://query1.finance.yahoo.com/.../SPY?interval=1d
wget https://query1.finance.yahoo.com/.../VIX?interval=1d

# Convert to WAD (Python)
python3 << 'EOF'
import pandas as pd
df = pd.read_csv('SPY.csv')
WAD = 10**18
df['Open'] = (df['Open'] * WAD).astype('int64')
# ... same for High, Low, Close
df.to_csv('spy_wad.csv', index=False)
EOF

# Modify mdm_main.c to load real data
# Recompile
make clean && make all

# Run
./build/bin/mdm_capsule
```

## Understanding the Output

### Spectral Signal

```
Snapshot #100
  λ_min (raw):       0.901234    ← Manifold compression factor
  λ_min (projected): 1.000000    ← Regularized (never < 1)
  λ_max:             45.234567   ← Largest eigenvalue
  Agent Energy:      234.567     ← 13-D subspace magnitude
  Structure Energy:  456.789     ← 27-D subspace magnitude
  CMI Total:         691.356     ← Coupling metric
```

### Correlation Test

```
Optimal Lag:    -2 days          ← Negative = geometry LEADS observable
Correlation:    -0.62            ← Strong negative
P-value:        0.003            ← < 0.05 = statistically significant
N Samples:      200
```

**Interpretation:**
- ✓ If lag < 0: geometry leads (predictive!)
- ○ If lag = 0: synchronous (no lead-lag)
- ✗ If lag > 0: geometry lags (reactive)

## Key Concepts in 60 Seconds

**WAD (Fixed-Point) Scale:**
- All numbers are `int64_t` multiplied by 10^18
- 1.0 = 1,000,000,000,000,000,000 in code
- Precision: 10^-18 (18 decimal places)
- No floating-point = no rounding errors = deterministic

**40-D Manifold:**
- 13-D: Agent intent (momentum, volume, sentiment velocity)
- 27-D: Structural constraints (liquidity, execution realities)
- Combined: Captures both psychology AND physics

**Eigendecomposition:**
- λ_min = how "flat" the market structure has become
- Small λ_min = stress, compression, instability
- Large λ_max = high volatility

**Lead-Lag Correlation:**
- Test if geometry at time t correlates with price at time t+lag
- Negative lag = geometry predicts future prices
- P-value < 0.05 = statistically significant

## Falsification Mindset

This framework is designed to **prove or disprove**:

```
✓ Hypothesis Confirmed:   ρ < -0.4 @ lag < 0, p < 0.05, out-of-sample holds
○ Uncertain:              ρ ≈ -0.3, p > 0.05, mixed out-of-sample
✗ Hypothesis Rejected:    ρ ≈ 0, p > 0.10, or reversed out-of-sample
```

**Don't seek confirmation.** Seek to disprove. If you can't break it, it might be real.

## Common Questions

**Q: Is this a trading bot?**  
A: No. It's a research framework for testing hypotheses. You'd build your own bot using these signals.

**Q: Will it make money?**  
A: Depends. If the hypothesis is true, maybe. That's what testing is for.

**Q: Why fixed-point instead of floating-point?**  
A: Eliminates rounding errors. Enables bit-exact reproducibility. Better for audit trails.

**Q: How many years of data do I need?**  
A: Minimum 1 year (252 trading days). Better with 2-5 years.

**Q: Can I integrate with my broker?**  
A: Yes! See `docs/INTEGRATION.md` for IB/Alpaca examples.

**Q: What if the hypothesis fails?**  
A: That's OK! Science is about learning what doesn't work. Negative results are still results.

## Get Help

- **Theory & Math:** `docs/THEORY.md`
- **Real Data Integration:** `docs/INTEGRATION.md`
- **Full Documentation:** `README.md`
- **Source Code:** `src/*.c`

## License

MIT License. See `LICENSE` file.

## Citation

If you publish results using this framework, cite:

```
Russell, M.A. (2026). "Market Decomposition Manifold:
Deterministic Fixed-Point Analysis of Microstructure Geometry."
U.S. Patent Application 19/383,582.
Universal Standard Axiom Corporation.
```

---

**Ready?** Start with:

```bash
make all && ./build/bin/mdm_capsule
```

**Then:** Read `docs/THEORY.md` for the math behind it.

**Finally:** Integrate real data (see `docs/INTEGRATION.md`) and test your hypothesis.

🚀 **Good luck. May your hypotheses be testable and your correlations be significant.**
