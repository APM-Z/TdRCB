#ifndef TdRCB_SolBody_H
#define TdRCB_SolBody_H

#include "TdRCB.h"

typedef struct {        /* temperature-RCB fit: RCB = a*T^2 + b*T + c */
    int n;
    double sT, sR, sTT, sT3, sT4, sTR, sT2R, sRR;
    int nmax;           /* capacity of epoch series */
    gtime_t *time;      /* epoch times for residual output */
    double *T;          /* temperature (degC) */
    double *R;          /* solved RCB (m) */
} tfit_t;

void OutPut(FILE** fp, rcb_t* rcb, rscs_t* rscs, const obsd_t* obs, int n, int stat, sta_t* sta, const solopt_t* sopt);
void OutTempFit(FILE** fp, const prcopt_t* popt, const tfit_t acc[][NFREQ]);
void OutTempFitRes(const filopt_t* fopt, const prcopt_t* popt, const char* fitfilename, const tfit_t acc[][NFREQ]);
int  tfit_add(tfit_t* a, gtime_t time, double T, double R);
void tfit_free(tfit_t acc[][NFREQ]);
void OutLogElv(FILE** fp, double maskElv, char id[4], double rsElv);
void OutLogFrq(FILE** fp, char id[4], int nf, int f);
#endif
