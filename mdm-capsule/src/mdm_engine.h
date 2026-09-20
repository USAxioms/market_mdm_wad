/**
 * MDM_ENGINE.H
 * Market Decomposition Manifold: Pure WAD fixed-point
 * 40-dimensional spectral analysis of market microstructure
 * 
 * Dual-layer topology:
 *   - 13-D Agent/Conscious Subspace (intent, sentiment, behavioral pressure)
 *   - 27-D Structural Grid (mechanics, liquidity, execution constraints)
 */

#ifndef MDM_ENGINE_H
#define MDM_ENGINE_H

#include "matrix_fixed_point.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

typedef int64_t wad_t;

/* ========== MDM STATE ========== */

typedef struct {
    /* Raw market data (OHLCV) */
    wad_t *open;      /* Opening prices */
    wad_t *high;      /* High prices */
    wad_t *low;       /* Low prices */
    wad_t *close;     /* Closing prices */
    wad_t *volume;    /* Trading volume */
    uint32_t length;  /* Number of time steps */
    
    /* Computed feature space */
    WadMatrix *feature_matrix;  /* (length, 13) for agent subspace */
    WadMatrix *structure_matrix;  /* (length, 27) for structural grid */
    
    /* Combined 40-D manifold */
    WadMatrix *combined_manifold;  /* (length, 40) */
    
    /* Spectral analysis */
    WadMatrix *covariance;  /* (40, 40) covariance matrix */
    EigenDecomposition *eigen;  /* Eigenvalues and eigenvectors */
    
} MDMState;

typedef struct {
    wad_t lambda_min_raw;       /* Minimum eigenvalue (raw) */
    wad_t lambda_min_projected; /* Minimum eigenvalue (regularized) */
    wad_t lambda_max;           /* Maximum eigenvalue */
    wad_t trace_value;          /* Trace (sum of eigenvalues) */
    
    /* Subspace analysis */
    wad_t agent_energy;         /* Frobenius norm of 13-D subspace */
    wad_t structure_energy;     /* Frobenius norm of 27-D subspace */
    
    /* Coupling metric (CMI: Conditional Mutual Information proxy) */
    wad_t cmi_total;            /* Total interaction strength */
    wad_t cmi_agent_only;       /* 13-D subspace alone */
    wad_t cmi_structure_only;   /* 27-D subspace alone */
    
    /* Deformation index */
    wad_t deformation_index;    /* lambda_raw / lambda_projected */
    
    /* Timestamp */
    uint32_t timestamp;
    bool valid;
    
} MDMSnapshot;

typedef struct {
    MDMSnapshot *snapshots;
    uint32_t count;
    uint32_t capacity;
} MDMTimeSeries;

/* ========== INITIALIZATION ========== */

MDMState* mdm_init(uint32_t time_steps);
void mdm_free(MDMState *state);

/* ========== DATA INGESTION ========== */

void mdm_load_ohlcv(MDMState *state, const char *filename);
void mdm_add_price_point(MDMState *state, uint32_t idx, 
                        wad_t open, wad_t high, wad_t low, wad_t close, wad_t volume);

/* ========== FEATURE ENGINEERING ========== */

void mdm_extract_agent_features(MDMState *state);
void mdm_extract_structure_features(MDMState *state);
void mdm_combine_manifold(MDMState *state);

/* ========== SPECTRAL ANALYSIS ========== */

MDMSnapshot* mdm_compute_spectrum(MDMState *state, uint32_t snapshot_idx);
MDMTimeSeries* mdm_rolling_spectrum(MDMState *state, uint32_t window_size);

/* ========== FORECASTING SIGNALS ========== */

typedef struct {
    wad_t lambda_min_change;      /* d(lambda_min) / dt */
    wad_t eigenvector_rotation;   /* Rotation rate of top 3 eigenvectors */
    wad_t cmi_decoupling;         /* |CMI_total - CMI_agent - CMI_structure| */
    wad_t structural_stress;      /* Magnitude of structural grid deformation */
} ForecastingSignal;

ForecastingSignal mdm_extract_signal(const MDMSnapshot *s1, const MDMSnapshot *s2);

/* ========== LEAD-LAG CORRELATION ========== */

typedef struct {
    wad_t correlation;
    int32_t optimal_lag;       /* In days (can be negative) */
    wad_t p_value;             /* Statistical significance */
    uint32_t num_samples;
} LeadLagResult;

LeadLagResult mdm_correlate_with_observable(const MDMTimeSeries *mdm_signals,
                                            const wad_t *observables,
                                            uint32_t observable_len,
                                            int32_t lag_min, int32_t lag_max);

/* ========== REPORTING ========== */

void mdm_snapshot_print(const MDMSnapshot *s, FILE *fp);
void mdm_timeseries_export_csv(const MDMTimeSeries *ts, const char *filename);
void mdm_correlation_report(const LeadLagResult *result, FILE *fp);

#endif /* MDM_ENGINE_H */
