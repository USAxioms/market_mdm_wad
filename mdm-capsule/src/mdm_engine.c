/**
 * MDM_ENGINE.C
 * Market Decomposition Manifold Implementation
 * Pure WAD arithmetic, all calculations in 18-decimal fixed-point
 */

#include "mdm_engine.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ========== WAD PRIMITIVES ========== */

static inline wad_t wad_add(wad_t a, wad_t b) {
    return a + b;  /* Overflow handled by caller if needed */
}

static inline wad_t wad_sub(wad_t a, wad_t b) {
    return a - b;
}

static inline wad_t wad_mul(wad_t a, wad_t b) {
    if (a == 0 || b == 0) return 0;
    long double result = ((long double)a * (long double)b) / 1e18;
    if (result > 9.223e18) return LLONG_MAX;
    if (result < -9.223e18) return LLONG_MIN;
    return (wad_t)result;
}

static inline wad_t wad_div(wad_t a, wad_t b) {
    if (b == 0) return 0;
    long double result = ((long double)a * 1e18) / (long double)b;
    if (result > 9.223e18) return LLONG_MAX;
    if (result < -9.223e18) return LLONG_MIN;
    return (wad_t)(result + (result >= 0 ? 0.5 : -0.5));
}

static inline wad_t wad_abs(wad_t x) {
    return (x < 0) ? -x : x;
}

static inline wad_t wad_sqrt(wad_t x) {
    if (x <= 0) return 0;
    long double root = sqrtl((long double)x / 1e18);
    return (wad_t)(root * 1e18);
}

#define WAD_ONE 1000000000000000000LL

/* ========== INITIALIZATION ========== */

MDMState* mdm_init(uint32_t time_steps) {
    MDMState *state = (MDMState *)malloc(sizeof(MDMState));
    
    state->open = (wad_t *)calloc(time_steps, sizeof(wad_t));
    state->high = (wad_t *)calloc(time_steps, sizeof(wad_t));
    state->low = (wad_t *)calloc(time_steps, sizeof(wad_t));
    state->close = (wad_t *)calloc(time_steps, sizeof(wad_t));
    state->volume = (wad_t *)calloc(time_steps, sizeof(wad_t));
    
    state->feature_matrix = matrix_alloc(time_steps, 13);
    state->structure_matrix = matrix_alloc(time_steps, 27);
    state->combined_manifold = matrix_alloc(time_steps, 40);
    state->covariance = matrix_alloc(40, 40);
    state->eigen = NULL;
    
    state->length = time_steps;
    
    return state;
}

void mdm_free(MDMState *state) {
    if (state == NULL) return;
    free(state->open);
    free(state->high);
    free(state->low);
    free(state->close);
    free(state->volume);
    matrix_free(state->feature_matrix);
    matrix_free(state->structure_matrix);
    matrix_free(state->combined_manifold);
    matrix_free(state->covariance);
    if (state->eigen) eigen_free(state->eigen);
    free(state);
}

/* ========== DATA INGESTION ========== */

void mdm_add_price_point(MDMState *state, uint32_t idx,
                        wad_t open, wad_t high, wad_t low, wad_t close, wad_t volume) {
    if (idx >= state->length) return;
    state->open[idx] = open;
    state->high[idx] = high;
    state->low[idx] = low;
    state->close[idx] = close;
    state->volume[idx] = volume;
}

void mdm_load_ohlcv(MDMState *state, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return;
    
    uint32_t idx = 0;
    char line[512];
    
    while (fgets(line, sizeof(line), fp) && idx < state->length) {
        wad_t o, h, l, c, v;
        /* Parse CSV: open,high,low,close,volume (values in WAD scale) */
        if (sscanf(line, "%lld,%lld,%lld,%lld,%lld", 
                   (long long *)&o, (long long *)&h, (long long *)&l, 
                   (long long *)&c, (long long *)&v) == 5) {
            mdm_add_price_point(state, idx, o, h, l, c, v);
            idx++;
        }
    }
    
    fclose(fp);
}

/* ========== FEATURE ENGINEERING ========== */

void mdm_extract_agent_features(MDMState *state) {
    /**
     * 13-D Agent/Conscious Subspace captures:
     *   C_01: Price momentum (close[t] - close[t-1])
     *   C_02: Volatility (high - low normalized)
     *   C_03: Volume momentum
     *   C_04: Intrabar range
     *   C_05-C_13: Lagged/transformed versions for sentiment
     */
    
    for (uint32_t t = 1; t < state->length; t++) {
        /* C_01: Price momentum */
        wad_t momentum = wad_sub(state->close[t], state->close[t-1]);
        matrix_set(state->feature_matrix, t, 0, momentum);
        
        /* C_02: Intrabar volatility */
        wad_t intrabar = wad_sub(state->high[t], state->low[t]);
        matrix_set(state->feature_matrix, t, 1, intrabar);
        
        /* C_03: Volume momentum */
        wad_t vol_change = wad_sub(state->volume[t], state->volume[t-1]);
        matrix_set(state->feature_matrix, t, 2, vol_change);
        
        /* C_04: Close proximity to high/low */
        wad_t range = wad_sub(state->high[t], state->low[t]);
        wad_t close_pos = wad_div(
            wad_sub(state->close[t], state->low[t]),
            (range == 0) ? WAD_ONE : range
        );
        matrix_set(state->feature_matrix, t, 3, close_pos);
        
        /* C_05-C_13: Lagged versions */
        for (uint32_t lag = 1; lag < 9 && t >= lag; lag++) {
            wad_t lagged_momentum = wad_sub(state->close[t-lag], 
                                           (t-lag > 0 ? state->close[t-lag-1] : 0));
            matrix_set(state->feature_matrix, t, 4 + lag - 1, lagged_momentum);
        }
    }
}

void mdm_extract_structure_features(MDMState *state) {
    /**
     * 27-D Structural Grid captures:
     *   C_14-C_20: Liquidity depth, bid-ask spread proxies
     *   C_21-C_27: Execution dynamics, slippage
     *   C_28-C_40: Market microstructure state
     * 
     * Since we don't have explicit orderbook data, approximate from OHLCV
     */
    
    for (uint32_t t = 1; t < state->length; t++) {
        /* C_14: Spread proxy (high - low) */
        wad_t spread = wad_sub(state->high[t], state->low[t]);
        matrix_set(state->structure_matrix, t, 0, spread);
        
        /* C_15: Normalized spread */
        wad_t norm_spread = wad_div(spread, 
                                   (state->close[t] == 0) ? WAD_ONE : state->close[t]);
        matrix_set(state->structure_matrix, t, 1, norm_spread);
        
        /* C_16-C_27: Lagged liquidity metrics */
        for (uint32_t i = 2; i < 27; i++) {
            if (t >= i) {
                wad_t lagged_spread = wad_sub(state->high[t-i], state->low[t-i]);
                matrix_set(state->structure_matrix, t, i, lagged_spread);
            }
        }
    }
}

void mdm_combine_manifold(MDMState *state) {
    /**
     * Stack [13-D agent | 27-D structure] → 40-D combined manifold
     */
    
    for (uint32_t t = 0; t < state->length; t++) {
        /* Copy agent features (columns 0-12) */
        for (uint32_t i = 0; i < 13; i++) {
            wad_t val = matrix_get(state->feature_matrix, t, i);
            matrix_set(state->combined_manifold, t, i, val);
        }
        
        /* Copy structure features (columns 13-39) */
        for (uint32_t i = 0; i < 27; i++) {
            wad_t val = matrix_get(state->structure_matrix, t, i);
            matrix_set(state->combined_manifold, t, 13 + i, val);
        }
    }
}

/* ========== SPECTRAL ANALYSIS ========== */

static void compute_covariance(const WadMatrix *data, WadMatrix *cov) {
    /**
     * Compute sample covariance matrix
     * Cov[i,j] = E[(X_i - mean_i)(X_j - mean_j)]
     */
    
    uint32_t n_samples = data->rows;
    uint32_t n_vars = data->cols;
    
    /* Compute means */
    wad_t *means = (wad_t *)calloc(n_vars, sizeof(wad_t));
    for (uint32_t j = 0; j < n_vars; j++) {
        wad_t sum = 0;
        for (uint32_t i = 0; i < n_samples; i++) {
            sum = wad_add(sum, matrix_get(data, i, j));
        }
        means[j] = wad_div(sum, (wad_t)n_samples * WAD_ONE);
    }
    
    /* Compute covariance */
    for (uint32_t i = 0; i < n_vars; i++) {
        for (uint32_t j = 0; j < n_vars; j++) {
            wad_t cov_sum = 0;
            for (uint32_t k = 0; k < n_samples; k++) {
                wad_t diff_i = wad_sub(matrix_get(data, k, i), means[i]);
                wad_t diff_j = wad_sub(matrix_get(data, k, j), means[j]);
                cov_sum = wad_add(cov_sum, wad_mul(diff_i, diff_j));
            }
            wad_t cov_val = wad_div(cov_sum, (wad_t)n_samples * WAD_ONE);
            matrix_set(cov, i, j, cov_val);
        }
    }
    
    free(means);
}

MDMSnapshot* mdm_compute_spectrum(MDMState *state, uint32_t snapshot_idx) {
    if (snapshot_idx >= state->length) return NULL;
    
    MDMSnapshot *snap = (MDMSnapshot *)malloc(sizeof(MDMSnapshot));
    snap->timestamp = snapshot_idx;
    snap->valid = true;
    
    /* Compute covariance of 40-D manifold */
    compute_covariance(state->combined_manifold, state->covariance);
    
    /* Eigendecomposition */
    if (state->eigen) eigen_free(state->eigen);
    state->eigen = eigen_decompose(state->covariance, 50);
    
    if (state->eigen && state->eigen->num_eigenvalues > 0) {
        snap->lambda_min_raw = state->eigen->eigenvalues[state->eigen->num_eigenvalues - 1];
        snap->lambda_max = state->eigen->eigenvalues[0];
        snap->trace_value = matrix_trace(state->covariance);
        
        /* Regularization: project lambda_min to unit floor */
        snap->lambda_min_projected = (snap->lambda_min_raw < WAD_ONE) ? WAD_ONE : snap->lambda_min_raw;
        
        /* Deformation index */
        snap->deformation_index = (snap->lambda_min_projected == 0) ? 0 :
            wad_div(snap->lambda_min_raw, snap->lambda_min_projected);
    }
    
    /* Subspace energies */
    snap->agent_energy = matrix_frobenius_norm(state->feature_matrix);
    snap->structure_energy = matrix_frobenius_norm(state->structure_matrix);
    
    /* CMI (Conditional Mutual Information proxy via energy coupling) */
    snap->cmi_agent_only = wad_mul(snap->agent_energy, snap->agent_energy);
    snap->cmi_structure_only = wad_mul(snap->structure_energy, snap->structure_energy);
    snap->cmi_total = wad_add(snap->cmi_agent_only, snap->cmi_structure_only);
    
    return snap;
}

MDMTimeSeries* mdm_rolling_spectrum(MDMState *state, uint32_t window_size) {
    MDMTimeSeries *ts = (MDMTimeSeries *)malloc(sizeof(MDMTimeSeries));
    ts->capacity = state->length - window_size + 1;
    ts->snapshots = (MDMSnapshot *)calloc(ts->capacity, sizeof(MDMSnapshot));
    ts->count = 0;
    
    for (uint32_t t = window_size; t < state->length; t++) {
        MDMSnapshot *snap = mdm_compute_spectrum(state, t);
        if (snap) {
            ts->snapshots[ts->count] = *snap;
            ts->count++;
            free(snap);
        }
    }
    
    return ts;
}

/* ========== FORECASTING SIGNALS ========== */

ForecastingSignal mdm_extract_signal(const MDMSnapshot *s1, const MDMSnapshot *s2) {
    ForecastingSignal sig;
    
    sig.lambda_min_change = wad_sub(s2->lambda_min_raw, s1->lambda_min_raw);
    sig.eigenvector_rotation = wad_sub(s2->deformation_index, s1->deformation_index);
    sig.cmi_decoupling = wad_abs(wad_sub(s2->cmi_total, 
                                        wad_add(s2->cmi_agent_only, s2->cmi_structure_only)));
    sig.structural_stress = wad_sub(s2->structure_energy, s1->structure_energy);
    
    return sig;
}

/* ========== LEAD-LAG CORRELATION ========== */

LeadLagResult mdm_correlate_with_observable(const MDMTimeSeries *mdm_signals,
                                            const wad_t *observables,
                                            uint32_t observable_len,
                                            int32_t lag_min, int32_t lag_max) {
    LeadLagResult result = {0, 0, 0, 0};
    wad_t best_corr = 0;
    
    for (int32_t lag = lag_min; lag <= lag_max; lag++) {
        uint32_t n_aligned = 0;
        wad_t sum_xy = 0, sum_x = 0, sum_y = 0, sum_x2 = 0, sum_y2 = 0;
        
        for (uint32_t i = 0; i < mdm_signals->count; i++) {
            int32_t obs_idx = (int32_t)i + lag;
            if (obs_idx >= 0 && obs_idx < (int32_t)observable_len) {
                /* Use lambda_min_change as signal */
                wad_t x = (i > 0) ? 
                    wad_sub(mdm_signals->snapshots[i].lambda_min_raw,
                           mdm_signals->snapshots[i-1].lambda_min_raw) : 0;
                wad_t y = observables[obs_idx];
                
                sum_xy = wad_add(sum_xy, wad_mul(x, y));
                sum_x = wad_add(sum_x, x);
                sum_y = wad_add(sum_y, y);
                sum_x2 = wad_add(sum_x2, wad_mul(x, x));
                sum_y2 = wad_add(sum_y2, wad_mul(y, y));
                n_aligned++;
            }
        }
        
        if (n_aligned > 2) {
            wad_t n = (wad_t)n_aligned * WAD_ONE;
            wad_t numerator = wad_sub(wad_mul(n, sum_xy), wad_mul(sum_x, sum_y));
            wad_t denom_x = wad_sub(wad_mul(n, sum_x2), wad_mul(sum_x, sum_x));
            wad_t denom_y = wad_sub(wad_mul(n, sum_y2), wad_mul(sum_y, sum_y));
            
            if (denom_x > 0 && denom_y > 0) {
                wad_t denominator = wad_mul(wad_sqrt(denom_x), wad_sqrt(denom_y));
                wad_t corr = (denominator == 0) ? 0 : wad_div(numerator, denominator);
                
                if (wad_abs(corr) > wad_abs(best_corr)) {
                    best_corr = corr;
                    result.correlation = corr;
                    result.optimal_lag = lag;
                    result.num_samples = n_aligned;
                }
            }
        }
    }
    
    return result;
}

/* ========== REPORTING ========== */

void mdm_snapshot_print(const MDMSnapshot *s, FILE *fp) {
    fprintf(fp, "SNAPSHOT #%u\n", s->timestamp);
    fprintf(fp, "  λ_min (raw):       %lld\n", (long long)s->lambda_min_raw);
    fprintf(fp, "  λ_min (projected): %lld\n", (long long)s->lambda_min_projected);
    fprintf(fp, "  λ_max:             %lld\n", (long long)s->lambda_max);
    fprintf(fp, "  Trace:             %lld\n", (long long)s->trace_value);
    fprintf(fp, "  Agent Energy:      %lld\n", (long long)s->agent_energy);
    fprintf(fp, "  Structure Energy:  %lld\n", (long long)s->structure_energy);
    fprintf(fp, "  CMI Total:         %lld\n", (long long)s->cmi_total);
    fprintf(fp, "  Deformation Index: %lld\n", (long long)s->deformation_index);
}

void mdm_timeseries_export_csv(const MDMTimeSeries *ts, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) return;
    
    fprintf(fp, "timestamp,lambda_min_raw,lambda_min_projected,lambda_max,agent_energy,structure_energy,cmi_total,deformation_index\n");
    
    for (uint32_t i = 0; i < ts->count; i++) {
        const MDMSnapshot *s = &ts->snapshots[i];
        fprintf(fp, "%u,%lld,%lld,%lld,%lld,%lld,%lld,%lld\n",
               s->timestamp,
               (long long)s->lambda_min_raw,
               (long long)s->lambda_min_projected,
               (long long)s->lambda_max,
               (long long)s->agent_energy,
               (long long)s->structure_energy,
               (long long)s->cmi_total,
               (long long)s->deformation_index);
    }
    
    fclose(fp);
}

void mdm_correlation_report(const LeadLagResult *result, FILE *fp) {
    fprintf(fp, "LEAD-LAG CORRELATION REPORT\n");
    fprintf(fp, "============================\n");
    fprintf(fp, "Correlation:    %lld (in WAD units)\n", (long long)result->correlation);
    fprintf(fp, "Optimal Lag:    %d days\n", result->optimal_lag);
    fprintf(fp, "P-value:        %lld\n", (long long)result->p_value);
    fprintf(fp, "N Samples:      %u\n", result->num_samples);
}
