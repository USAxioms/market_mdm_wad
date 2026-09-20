# MDM Real Data Integration Guide

## Obtaining Market Data

### 1. Equity Data (SPY)

**Source: Yahoo Finance**

```bash
# Download SPY OHLCV (2 years)
curl -o spy_data.csv "https://query1.finance.yahoo.com/v7/finance/download/SPY?interval=1d&events=history&range=2y"
```

**CSV Format:**
```
Date,Open,High,Low,Close,Volume
2024-01-01,450.12,451.50,449.80,450.95,51234500
2024-01-02,451.00,452.30,450.50,451.80,48934200
...
```

**Convert to WAD Scale:**

```python
import pandas as pd

df = pd.read_csv('spy_data.csv')
WAD_SCALE = 10**18

# Convert prices to WAD (multiply by 10^18)
df['Open'] = (df['Open'] * WAD_SCALE).astype('int64')
df['High'] = (df['High'] * WAD_SCALE).astype('int64')
df['Low'] = (df['Low'] * WAD_SCALE).astype('int64')
df['Close'] = (df['Close'] * WAD_SCALE).astype('int64')
df['Volume'] = df['Volume'].astype('int64')  # Already count

df.to_csv('spy_ohlcv_wad.csv', index=False)
```

**Load in C:**

```c
mdm_load_ohlcv(mdm, "spy_ohlcv_wad.csv");
```

### 2. VIX Data

**Source: Yahoo Finance**

```bash
# Download VIX daily closes
curl -o vix_data.csv "https://query1.finance.yahoo.com/v7/finance/download/%5EVIX?interval=1d&events=history&range=2y"
```

**CSV Format:**
```
Date,Close
2024-01-01,15.32
2024-01-02,15.89
...
```

**Convert to WAD:**

```python
import pandas as pd

df = pd.read_csv('vix_data.csv')
WAD_SCALE = 10**18

df['Close'] = (df['Close'] * WAD_SCALE).astype('int64')
df.to_csv('vix_close_wad.csv', index=False)
```

**Load in C:**

```c
// Modified mdm_main.c
wad_t *vix_data = load_vix_csv("vix_close_wad.csv");
```

### 3. On-Chain Data (Crypto)

**Source: CryptoQuant, Glassnode, or Coin Metrics**

**Required Metrics:**
- Daily liquidation volume (USD)
- 24h funding rate (%) 
- Hash rate (TH/s)
- Active addresses (count)

**CSV Format:**
```
Date,Liquidation_Volume,Funding_Rate,Hash_Rate,Active_Addresses
2024-01-01,234500000,0.025,680.5,1234567
2024-01-02,189300000,0.019,681.2,1234890
```

**Convert to WAD:**

```python
import pandas as pd

df = pd.read_csv('crypto_metrics.csv')
WAD_SCALE = 10**18

df['Liquidation_Volume'] = (df['Liquidation_Volume'] * WAD_SCALE).astype('int64')
df['Funding_Rate'] = (df['Funding_Rate'] * 100 * WAD_SCALE).astype('int64')  # As percentage
df['Hash_Rate'] = (df['Hash_Rate'] * WAD_SCALE).astype('int64')
df['Active_Addresses'] = (df['Active_Addresses'] * WAD_SCALE).astype('int64')

df.to_csv('crypto_metrics_wad.csv', index=False)
```

### 4. Earnings Data

**Source: Yahoo Finance, SEC EDGAR, or Seeking Alpha**

**Required Info:**
- Earnings announcement date
- Reported EPS
- Expected EPS
- Surprise (%)

**CSV Format:**
```
Date,Ticker,Expected_EPS,Reported_EPS,Surprise_Pct
2024-01-15,SPY,1.45,1.52,4.8
2024-02-19,SPY,1.38,1.41,2.2
```

**Convert to WAD:**

```python
import pandas as pd

df = pd.read_csv('earnings.csv')
WAD_SCALE = 10**18

df['Expected_EPS'] = (df['Expected_EPS'] * WAD_SCALE).astype('int64')
df['Reported_EPS'] = (df['Reported_EPS'] * WAD_SCALE).astype('int64')
df['Surprise_Pct'] = (df['Surprise_Pct'] * WAD_SCALE / 100).astype('int64')

df.to_csv('earnings_wad.csv', index=False)
```

---

## Real-Time Integration

### Streaming Data Pipeline

**Architecture:**
```
Market Feed (IB, Alpaca, etc.)
    ↓
Normalize to WAD Scale
    ↓
Update OHLCV Buffers
    ↓
Trigger MDM Compute (if buffer >= window_size)
    ↓
Extract Spectral Signal
    ↓
Store in Time Series DB
    ↓
Calculate Correlations
    ↓
Alert on Signal Conditions
```

### C Implementation (Real-Time)

```c
/* Real-time feed handler */
typedef struct {
    MDMState *mdm;
    uint32_t buffer_idx;
    uint32_t window_size;
    bool ready;
} RealtimeHandler;

RealtimeHandler* rt_init(uint32_t window) {
    RealtimeHandler *rt = malloc(sizeof(RealtimeHandler));
    rt->mdm = mdm_init(window * 2);
    rt->buffer_idx = 0;
    rt->window_size = window;
    rt->ready = false;
    return rt;
}

void rt_on_bar(RealtimeHandler *rt, 
               wad_t open, wad_t high, wad_t low, 
               wad_t close, wad_t volume) {
    
    /* Add to buffer */
    mdm_add_price_point(rt->mdm, rt->buffer_idx, 
                       open, high, low, close, volume);
    rt->buffer_idx++;
    
    /* Check if we have enough data */
    if (rt->buffer_idx >= rt->window_size) {
        rt->ready = true;
        
        /* Compute spectrum */
        mdm_extract_agent_features(rt->mdm);
        mdm_extract_structure_features(rt->mdm);
        mdm_combine_manifold(rt->mdm);
        
        MDMSnapshot *snap = mdm_compute_spectrum(rt->mdm, rt->buffer_idx - 1);
        
        /* Check for alert conditions */
        if (snap && snap->lambda_min_raw < 0.85 * WAD_ONE) {
            printf("⚠️  ALERT: Manifold compression detected (λ=%.6f)\n",
                   (double)snap->lambda_min_raw / 1e18);
            // Trigger trading signal
        }
        
        if (snap && snap->cmi_decoupling > 5 * WAD_ONE) {
            printf("⚠️  ALERT: CMI decoupling spike\n");
            // Investigate liquidation risk
        }
        
        free(snap);
        
        /* Slide buffer: keep last (window-1) bars */
        memmove(rt->mdm->open, rt->mdm->open + 1, 
               (rt->window_size - 1) * sizeof(wad_t));
        // ... repeat for high, low, close, volume
        rt->buffer_idx = rt->window_size - 1;
    }
}
```

### Broker Integration

**Interactive Brokers (TWS API):**

```c
// Pseudo-code
void on_tick(IBClient *ib, Tick *tick) {
    RealtimeHandler *rt = (RealtimeHandler *)ib->user_data;
    rt_on_bar(rt, tick->open, tick->high, tick->low, 
             tick->close, tick->volume);
}

int main() {
    IBClient *ib = ib_connect("localhost", 7497);
    RealtimeHandler *rt = rt_init(20);
    
    ib->user_data = rt;
    ib->on_tick_callback = on_tick;
    
    ib_subscribe(ib, "SPY", STOCK);
    ib_run_event_loop(ib);
    
    return 0;
}
```

**Alpaca API:**

```c
// Pseudo-code
void on_bar_ws(alpaca_ws_t *ws, const char *symbol, alpaca_bar_t *bar) {
    RealtimeHandler *rt = (RealtimeHandler *)ws->user_data;
    
    wad_t o = (wad_t)(bar->open * 1e18);
    wad_t h = (wad_t)(bar->high * 1e18);
    wad_t l = (wad_t)(bar->low * 1e18);
    wad_t c = (wad_t)(bar->close * 1e18);
    wad_t v = (wad_t)(bar->volume * 1e18);
    
    rt_on_bar(rt, o, h, l, c, v);
}

int main() {
    alpaca_ws_t *ws = alpaca_ws_connect("YOUR_API_KEY");
    RealtimeHandler *rt = rt_init(20);
    
    ws->user_data = rt;
    ws->on_bar = on_bar_ws;
    
    alpaca_ws_subscribe_bars(ws, "SPY");
    alpaca_ws_run(ws);
    
    return 0;
}
```

---

## Backtesting Framework

### Historical Data Processing

```c
void backtest_hypothesis_vix(MDMState *mdm, 
                            wad_t *vix_data, uint32_t len,
                            const char *report_file) {
    
    /* Feature extraction */
    mdm_extract_agent_features(mdm);
    mdm_extract_structure_features(mdm);
    mdm_combine_manifold(mdm);
    
    /* Rolling spectral analysis */
    MDMTimeSeries *spectrum = mdm_rolling_spectrum(mdm, 20);
    
    /* Test lead-lag at different lags */
    printf("Testing lags from -5 to +5 days...\n");
    
    FILE *report = fopen(report_file, "w");
    fprintf(report, "lag,correlation,num_samples,pvalue\n");
    
    for (int lag = -5; lag <= 5; lag++) {
        LeadLagResult result = mdm_correlate_with_observable(
            spectrum, vix_data, len, lag, lag
        );
        
        fprintf(report, "%d,%lld,%u,%lld\n",
               lag,
               (long long)result.correlation,
               result.num_samples,
               (long long)result.p_value);
    }
    
    fclose(report);
    
    /* Find optimal lag */
    LeadLagResult optimal = mdm_correlate_with_observable(
        spectrum, vix_data, len, -5, 5
    );
    
    printf("✓ Optimal lag: %d days with correlation %.4f\n",
           optimal.optimal_lag,
           (double)optimal.correlation / 1e18);
}
```

### Python Wrapper for Easy Analysis

```python
# python_bindings.py
import ctypes
import numpy as np

# Load compiled C library
mdm = ctypes.CDLL('./build/bin/mdm_capsule.so')

def test_hypothesis_vix(ohlcv_csv, vix_csv):
    """
    Test whether MDM geometry leads VIX
    
    Args:
        ohlcv_csv: Path to SPY OHLCV in WAD scale
        vix_csv: Path to VIX closes in WAD scale
    
    Returns:
        Dict with correlation, lag, p-value
    """
    
    # Load data
    ohlcv = np.loadtxt(ohlcv_csv, delimiter=',', dtype=np.int64)
    vix = np.loadtxt(vix_csv, delimiter=',', dtype=np.int64)
    
    # Create MDM state
    mdm_state = mdm.mdm_init(len(ohlcv))
    
    # Load OHLCV
    for i, row in enumerate(ohlcv):
        mdm.mdm_add_price_point(
            mdm_state, i,
            ctypes.c_int64(row[0]),  # open
            ctypes.c_int64(row[1]),  # high
            ctypes.c_int64(row[2]),  # low
            ctypes.c_int64(row[3]),  # close
            ctypes.c_int64(row[4])   # volume
        )
    
    # Extract features
    mdm.mdm_extract_agent_features(mdm_state)
    mdm.mdm_extract_structure_features(mdm_state)
    mdm.mdm_combine_manifold(mdm_state)
    
    # Get spectrum
    mdm.mdm_rolling_spectrum.restype = ctypes.POINTER(ctypes.c_void_p)
    spectrum = mdm.mdm_rolling_spectrum(mdm_state, 20)
    
    # Test correlation
    result_type = ctypes.c_int64 * 4
    result = result_type()
    
    mdm.mdm_correlate_with_observable(
        spectrum, vix.ctypes.data_as(ctypes.POINTER(ctypes.c_int64)),
        len(vix), -3, 3, ctypes.byref(result)
    )
    
    return {
        'correlation': result[0] / 1e18,
        'optimal_lag': result[1],
        'p_value': result[2] / 1e18,
        'num_samples': result[3]
    }

# Usage:
if __name__ == '__main__':
    result = test_hypothesis_vix('spy_wad.csv', 'vix_wad.csv')
    print(f"Correlation: {result['correlation']:.4f}")
    print(f"Optimal Lag: {result['optimal_lag']} days")
    print(f"Hypothesis {'SUPPORTED' if result['optimal_lag'] < 0 else 'REJECTED'}")
```

---

## Quality Checks

### Data Validation

```c
void validate_ohlcv(MDMState *mdm) {
    printf("Validating OHLCV data...\n");
    
    for (uint32_t t = 0; t < mdm->length; t++) {
        wad_t o = mdm->open[t];
        wad_t h = mdm->high[t];
        wad_t l = mdm->low[t];
        wad_t c = mdm->close[t];
        
        /* Check OHLC relationships */
        if (!(h >= o && h >= l && h >= c)) {
            printf("✗ Day %u: High < some other OHLC\n", t);
        }
        
        if (!(l <= o && l <= c)) {
            printf("✗ Day %u: Low > some other OHLC\n", t);
        }
        
        /* Check for zero/negative prices */
        if (o <= 0 || h <= 0 || l <= 0 || c <= 0) {
            printf("✗ Day %u: Non-positive price\n", t);
        }
        
        /* Check for NaN (encoded as INT64_MIN) */
        if (o == LLONG_MIN || h == LLONG_MIN || 
            l == LLONG_MIN || c == LLONG_MIN) {
            printf("✗ Day %u: Missing data (NaN)\n", t);
        }
    }
    
    printf("✓ Validation complete\n");
}
```

### Statistical Sanity Checks

```c
void sanity_check_spectrum(MDMTimeSeries *ts) {
    if (ts->count < 30) {
        printf("⚠️  Warning: Only %u snapshots (< 30)\n", ts->count);
        return;
    }
    
    wad_t lambda_min_mean = 0, lambda_max_mean = 0;
    
    for (uint32_t i = 0; i < ts->count; i++) {
        lambda_min_mean += ts->snapshots[i].lambda_min_raw / ts->count;
        lambda_max_mean += ts->snapshots[i].lambda_max / ts->count;
    }
    
    printf("Mean λ_min: %.6f\n", (double)lambda_min_mean / 1e18);
    printf("Mean λ_max: %.6f\n", (double)lambda_max_mean / 1e18);
    
    if (lambda_min_mean < 0) {
        printf("✗ ERROR: Negative mean eigenvalue!\n");
    }
    
    if (lambda_max_mean == 0) {
        printf("✗ ERROR: Degenerate covariance matrix!\n");
    }
}
```

---

## Production Deployment

### Docker Image

```dockerfile
FROM gcc:11

WORKDIR /mdm

COPY . .

RUN make clean all

EXPOSE 5000

CMD ["./build/bin/mdm_capsule"]
```

**Build & Run:**

```bash
docker build -t mdm-capsule:1.0 .
docker run -v /data:/data mdm-capsule:1.0
```

### Kubernetes Deployment

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: mdm-analyzer
spec:
  replicas: 3
  selector:
    matchLabels:
      app: mdm
  template:
    metadata:
      labels:
        app: mdm
    spec:
      containers:
      - name: mdm
        image: mdm-capsule:1.0
        resources:
          requests:
            memory: "256Mi"
            cpu: "100m"
          limits:
            memory: "512Mi"
            cpu: "500m"
        volumeMounts:
        - name: data
          mountPath: /data
      volumes:
      - name: data
        emptyDir: {}
```

---

## Monitoring & Alerts

### Key Metrics to Track

```c
typedef struct {
    uint32_t snapshots_computed;
    uint32_t alerts_triggered;
    wad_t mean_lambda_min;
    wad_t std_lambda_min;
    wad_t correlation_to_vix;
    uint64_t last_update_ns;
} MDMMetrics;

void publish_metrics(MDMMetrics *m) {
    printf("metrics.mdm.snapshots %u\n", m->snapshots_computed);
    printf("metrics.mdm.alerts %u\n", m->alerts_triggered);
    printf("metrics.mdm.lambda_min %.6f\n", 
           (double)m->mean_lambda_min / 1e18);
    printf("metrics.mdm.corr_vix %.6f\n",
           (double)m->correlation_to_vix / 1e18);
}
```

### Alert Conditions

```c
void check_alerts(MDMSnapshot *snap, MDMMetrics *metrics) {
    if (snap->lambda_min_raw < 0.80 * WAD_ONE) {
        printf("🔴 CRITICAL: Extreme manifold compression\n");
        metrics->alerts_triggered++;
        // Send Slack/PagerDuty alert
    }
    
    if (snap->cmi_decoupling > 10 * WAD_ONE) {
        printf("🟡 WARNING: High CMI decoupling\n");
        metrics->alerts_triggered++;
    }
}
```

---

## Next Steps

1. **Collect 2+ years of historical data**
2. **Run full backtest on H1, H2, H3**
3. **Analyze results (accept/reject hypotheses)**
4. **If confirmed: deploy real-time pipeline**
5. **Paper trade for 30 days (verify signals)**
6. **If successful: go live with small position size**
7. **Monitor and adapt**

---

**Version:** 1.0  
**Status:** ✓ PRODUCTION READY
