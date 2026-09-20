/**
 * MATRIX_FIXED_POINT_H
 * Dense matrix operations in pure WAD fixed-point arithmetic
 * 18-decimal places of precision, zero floating-point operations
 * 
 * For MDM: Supports up to 40x40 dense matrices (1,600 scalars)
 */

#ifndef MATRIX_FIXED_POINT_H
#define MATRIX_FIXED_POINT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>

typedef int64_t wad_t;
typedef struct {
    wad_t *data;      /* Row-major: [0,0], [0,1], ..., [0,n], [1,0], ... */
    uint32_t rows;
    uint32_t cols;
} WadMatrix;

/* Constants */
#define WAD_SCALE     1000000000000000000LL
#define WAD_ONE       WAD_SCALE
#define WAD_HALF      (WAD_SCALE / 2)
#define WAD_ZERO      0LL
#define WAD_MAX       LLONG_MAX
#define WAD_MIN       LLONG_MIN

/* Memory Management */
WadMatrix* matrix_alloc(uint32_t rows, uint32_t cols);
void matrix_free(WadMatrix *m);
void matrix_zero(WadMatrix *m);
void matrix_identity(WadMatrix *m);
void matrix_copy(const WadMatrix *src, WadMatrix *dst);

/* Accessors */
wad_t matrix_get(const WadMatrix *m, uint32_t i, uint32_t j);
void matrix_set(WadMatrix *m, uint32_t i, uint32_t j, wad_t value);

/* Basic Operations */
void matrix_add(const WadMatrix *a, const WadMatrix *b, WadMatrix *result);
void matrix_sub(const WadMatrix *a, const WadMatrix *b, WadMatrix *result);
void matrix_multiply(const WadMatrix *a, const WadMatrix *b, WadMatrix *result);
void matrix_scalar_multiply(const WadMatrix *a, wad_t scalar, WadMatrix *result);

/* Linear Algebra */
wad_t matrix_trace(const WadMatrix *m);
wad_t matrix_frobenius_norm(const WadMatrix *m);
void matrix_transpose(const WadMatrix *a, WadMatrix *result);

/* Eigendecomposition (via QR iteration) */
typedef struct {
    wad_t *eigenvalues;          /* Length: min(rows, cols) */
    WadMatrix *eigenvectors;     /* Shape: (cols, min(rows, cols)) */
    uint32_t num_eigenvalues;
    uint32_t iterations;
    bool converged;
} EigenDecomposition;

EigenDecomposition* eigen_decompose(const WadMatrix *m, uint32_t max_iterations);
void eigen_free(EigenDecomposition *e);

/* I/O */
void matrix_print(const WadMatrix *m);
void matrix_export_csv(const WadMatrix *m, FILE *fp);
void eigen_print_report(const EigenDecomposition *e, FILE *fp);

#endif /* MATRIX_FIXED_POINT_H */
