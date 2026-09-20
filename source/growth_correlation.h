/**
 * GROWTH_CORRELATION.H
 * 
 * Growth Correlation Principle: Bounded Exponential Growth Detection
 * 
 * FUNDAMENTAL MECHANISM:
 * ==================
 * Markets crash when subsystems decouple in their growth rates:
 * 
 *   Agent Growth Rate (13-D, exponential):      dC_A/dt ∝ e^{αt}
 *   Structure Growth Rate (27-D, linear):       dC_S/dt ∝ βt
 * 
 * When ρ_growth (correlation of growth rates) collapses while volatility is LOW,
 * the system has MAXIMALLY STRETCHED. The snap-back is imminent.
 * 
 * MATHEMATICAL GUARANTEE (Banach Contraction):
 * ============================================
 * For any bounded exponential expansion:
 *   ||expansion_t|| ≤ α^t ||expansion_0||  where α < 1
 * 
 * The contraction bound ensures the system cannot escape its topological boundary.
 * When the bound is violated (ρ_growth < threshold), λ_min compresses.
 * Compression forces geometric correction → crash.
 */

#ifndef GROWTH_CORRELATION_H
#define GROWTH_CORRELATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef int64_t wad_t;

#define WAD_SCALE   1000000000000000000LL   /* 10^18 */
#define WAD_ONE     WAD_SCALE
#define WAD_HALF    (WAD_SCALE / 2)

/* ========== GROWTH RATE METRICS ========== */

typedef struct {
    wad_t agent_growth_rate;        /* Growth rate of 13-D subspace */
    wad_t structure_growth_rate;    /* Growth rate of 27-D subspace */
    wad_t growth_correlation;       /* ρ_growth: correlation of rates */
    wad_t subsystem_divergence;     /* |agent_rate - structure_rate| */
    wad_t exponential_ratio;        /* agent_rate / structure_rate */
    
    uint32_t timestamp;
    bool valid;
    
} GrowthMetrics;

typedef struct {
    GrowthMetrics *metrics;
    uint32_t count;
    uint32_t capacity;
} GrowthTimeSeries;

/* ========== PHASE TRANSITION DETECTION ========== */

typedef struct {
    wad_t rho_growth;               /* Correlation coefficient */
    wad_t volatility_concurrent;    /* Volatility at same time */
    bool phase_strain_detected;     /* ρ_growth < threshold && vol < vol_threshold */
    bool maximum_stretch_detected;  /* Agent growth >> structure growth */
    
    wad_t stress_score;             /* Composite bubble risk (0-1 in WAD) */
    int32_t days_to_correction;     /* Estimated time to snap-back */
    
    uint32_t timestamp;
} PhaseTransitionSignal;

/* ========== SCALING LAW ANALYSIS ========== */

typedef struct {
    /* Power law exponent: growth ∝ t^α */
    wad_t agent_scaling_exponent;       /* α for agent subspace */
    wad_t structure_scaling_exponent;   /* α for structure grid */
    
    /* Lyapunov exponent: measures divergence rate */
    wad_t lyapunov_exponent;            /* λ_lyap: how fast agents diverge from structure */
    
    /* Correlation stability */
    wad_t rho_growth_stability;         /* How stable is ρ_growth over time */
    
    /* Asset class signature */
    bool is_crypto;                     /* true for crypto, false for equities */
    char asset_class_name[32];
    
} ScalingLawAnalysis;

/* ========== FUNCTION DECLARATIONS ========== */

/* Growth rate computation */
GrowthMetrics* growth_compute_metrics(
    const wad_t *agent_trajectory,
    const wad_t *structure_trajectory,
    uint32_t window_size,
    uint32_t timestamp
);

/* Growth correlation: rolling ρ_growth */
wad_t growth_correlation_coefficient(
    const wad_t *agent_growth_rates,
    const wad_t *structure_growth_rates,
    uint32_t window_size
);

/* Phase transition detection */
PhaseTransitionSignal* growth_detect_phase_transition(
    const GrowthTimeSeries *growth_ts,
    const wad_t *volatility_ts,
    wad_t rho_threshold,
    wad_t vol_threshold
);

/* Scaling law analysis */
ScalingLawAnalysis* growth_analyze_scaling_law(
    const GrowthTimeSeries *growth_ts,
    const char *asset_class_name,
    bool is_crypto
);

/* Subsystem divergence (agent vs structure) */
wad_t growth_subsystem_divergence(
    wad_t agent_rate,
    wad_t structure_rate
);

/* Exponential ratio: detect if agent >> structure */
wad_t growth_exponential_ratio(
    wad_t agent_rate,
    wad_t structure_rate
);

/* Stress score: composite risk metric (0-1 in WAD) */
wad_t growth_compute_stress_score(
    wad_t rho_growth,
    wad_t volatility,
    wad_t exponential_ratio,
    wad_t subsystem_divergence
);

/* Time-to-correction: days until snap-back */
int32_t growth_estimate_correction_window(
    wad_t stress_score,
    wad_t lyapunov_exponent
);

/* Memory management */
GrowthTimeSeries* growth_timeseries_alloc(uint32_t capacity);
void growth_timeseries_free(GrowthTimeSeries *ts);
void growth_metrics_free(GrowthMetrics *m);
void growth_transition_free(PhaseTransitionSignal *s);
void growth_scaling_free(ScalingLawAnalysis *s);

/* Reporting */
void growth_metrics_print(const GrowthMetrics *m, FILE *fp);
void growth_phase_signal_print(const PhaseTransitionSignal *sig, FILE *fp);
void growth_scaling_print(const ScalingLawAnalysis *s, FILE *fp);
void growth_timeseries_export_csv(const GrowthTimeSeries *ts, const char *filename);

/* Comparative analysis: equities vs crypto */
typedef struct {
    wad_t equity_scaling_exponent;
    wad_t crypto_scaling_exponent;
    wad_t scaling_exponent_ratio;   /* crypto / equity */
    wad_t equity_stability;
    wad_t crypto_stability;
    wad_t correlation_difference;   /* |ρ_equity - ρ_crypto| */
} AssetClassComparison;

AssetClassComparison* growth_compare_asset_classes(
    const ScalingLawAnalysis *equity_analysis,
    const ScalingLawAnalysis *crypto_analysis
);

void growth_asset_comparison_print(const AssetClassComparison *comp, FILE *fp);

#endif /* GROWTH_CORRELATION_H */
