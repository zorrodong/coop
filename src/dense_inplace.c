/*  Copyright (c) 2016, Schmidt
    All rights reserved.
    
    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    
    1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
    
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
    TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
    CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Functions for computing covariance, (pearson) correlation, and cosine similarity

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "coop.h"
#include "omp.h"


// O(1) storage
static int coop_covar_vecvec_inplace(const int n, const double *restrict x, const double *restrict y, double *restrict cor)
{
  int i;
  const double denom = (double) 1/n;
  double meanx;
  double meany;       // :DDD
  double mmcp = 0.0;  // minus-mean-crossproduct
  
  meanx = 0.0;
  meany = 0.0;
  
  SAFE_FOR_SIMD
  for (i=0; i<n; i++)
  {
    meanx += x[i];
    meany += y[i];
  }
  
  meanx *= denom;
  meany *= denom;
  
  SAFE_FOR_SIMD
  for (i=0; i<n; i++)
    mmcp += (x[i] - meanx) * (y[i] - meany);
  
  *cor = mmcp / ((double)(n-1));
  
  return 0;
}



// O(m+n) storage
static int co_mat_inplace(const int m, const int n, const double *restrict x, double *restrict cov)
{
  int i, j, k;
  int mj, mi;
  double meanx;
  double meany; // :DDD
  double mmcp;  // minus-mean-crossproduct
  double *vec = malloc(m * sizeof(*vec));
  CHECKMALLOC(vec);
  double *means = malloc(n * sizeof(*means));
  if (means==NULL)
  {
    free(vec);
    return -1;
  }
  const double denom_mean = (double) 1./m;
  const double denom_cov = (double) 1./(m-1);
  
  #pragma omp parallel for private(i, j, mj) if (m*n > OMP_MIN_SIZE)
  for (j=0; j<n; j++)
  {
    mj = m*j;
    
    means[j] = 0.0;
    SAFE_SIMD
    for (i=0; i<m; i++)
      means[j] += x[i + mj];
    
    means[j] *= denom_mean;
  }
  
  
  for (j=0; j<n; j++)
  {
    mj = m*j;
    
    memcpy(vec, x+mj, m*sizeof(*vec));
    
    meanx = means[j];
    SAFE_FOR_SIMD
    for (k=0; k<m; k++)
      vec[k] -= meanx;
    
    #pragma omp parallel for private(i, k, mi, meany, mmcp) if(m*n > OMP_MIN_SIZE)
    for (i=j; i<n; i++)
    {
      mi = m*i;
      
      meany = means[i];
      
      mmcp = 0.0;
      SAFE_SIMD
      for (k=0; k<m; k++)
        mmcp += vec[k] * (x[k + mi] - meany);
      
      cov[i + n*j] = mmcp * denom_cov;
    }
  }
  
  free(vec);
  free(means);
  
  return 0;
}



// ---------------------------------------------
//  Interface
// ---------------------------------------------

int coop_pcor_mat_inplace(const int m, const int n, const double *restrict x, double *restrict cor)
{
  int check;
  
  check = co_mat_inplace(m, n, x, cor);
  if (check) return check;
  
  cosim_fill(n, cor);
  coop_symmetrize(n, cor);
  
  return 0;
}



int coop_covar_mat_inplace(const int m, const int n, const double *restrict x, double *restrict cov)
{
  int check;
  
  check = co_mat_inplace(m, n, x, cov);
  if (check) return check;
  
  coop_symmetrize(n, cov);
  
  return 0;
}