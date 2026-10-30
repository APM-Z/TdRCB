#include "TdRCB_SolBody.h"

/* solution option to field separator */
static const char* opt2sep(const solopt_t* opt)
{
	if (!*opt->sep) return " ";
	else if (!strcmp(opt->sep, "\\t")) return "\t";
	return opt->sep;
}

/* output solution as the form of x/y/z-ecef */
static int outecef_float(uint8_t* buff, const char* s, const sol_t* sol, const prcopt_t* opt, sta_t* sta, const solopt_t* sopt)
{
	const char* sep = opt2sep(sopt);
	char* p = (char*)buff;
	double x = 0.0, y = 0.0, z = 0.0;
	x = sol->rr[0];
	y = sol->rr[1];
	z = sol->rr[2];
	if (opt->usebds == 0 || opt->usebds == 2) {
		p += sprintf(p, "%s%s%14.4f%s%14.4f%s%14.4f%s%4d%s%8.4f%s%9.4f%s%8.4f%s%8.4f%s%10.4f%s%8.4f%s%11.4f%s%9.4f%s%10.4f%s%9.4f%s%9.4f%s%12.4f%s%8.4f%s%8.4f%s%9.4f%s%11.4f%s%11.4f%s%12.4f%s%10.4f",
			s, sep, x, sep, y, sep, z, sep, sol->ns, sep, sol->dop[0], sep,
			sol->rcb[0][0], sep, sol->rcb[0][1], sep, sol->rcb[0][2], sep,//gps
			sol->rcb[1][0], sep, sol->rcb[1][1], sep,//glo
			sol->rcb[2][0], sep, sol->rcb[2][1], sep, sol->rcb[2][2], sep, sol->rcb[2][3], sep, sol->rcb[2][4], sep,//gal
			sol->rcb[3][0], sep, sol->rcb[3][1], sep, sol->rcb[3][2], sep, //qzss
			sol->rcb[4][0], sep, sol->rcb[4][1], sep, sol->rcb[4][2], sep, sol->rcb[4][3], sep, sol->rcb[4][4]);//bds
	}
	else if (opt->usebds == 1) {
		p += sprintf(p, "%s%s%14.4f%s%14.4f%s%14.4f%s%4d%s%8.4f%s%9.4f%s%8.4f%s%8.4f%s%10.4f%s%8.4f%s%11.4f%s%9.4f%s%10.4f%s%9.4f%s%9.4f%s%12.4f%s%8.4f%s%8.4f%s%9.4f%s%11.4f%s%11.4f",
			s, sep, x, sep, y, sep, z, sep, sol->ns, sep, sol->dop[0], sep,
			sol->rcb[0][0], sep, sol->rcb[0][1], sep, sol->rcb[0][2], sep,
			sol->rcb[1][0], sep, sol->rcb[1][1], sep,//glo
			sol->rcb[2][0], sep, sol->rcb[2][1], sep, sol->rcb[2][2], sep, sol->rcb[2][3], sep, sol->rcb[2][4], sep,//gal
			sol->rcb[3][0], sep, sol->rcb[3][1], sep, sol->rcb[3][2], sep, //qzss
			sol->rcb[4][0], sep, sol->rcb[4][1], sep, sol->rcb[4][2]);
	}
	p += sprintf(p, "\n");
	return (int)((p - (char*)buff));
}

void OutPut(FILE** fp, rcb_t* rcb, rscs_t* rscs, const obsd_t* obs, int n, int stat, sta_t* sta, const solopt_t* sopt)
{
	double ep[6] = { 0.0 };
	const prcopt_t* opt = &rcb->opt;
	int i, j, sat = 0, sys = 0;
	rcb->sol.time_filter = rcb->sol.time;
	/* valid satellites */
	rcb->sol.ns = 0;
	for (int i = 0; i < NSYS; i++) {
		rcb->sol.sys_ns[i] = 0;
	}
	for (i = 0; i < n && i < MAXOBS; ++i) {
		if ((sat = obs[i].sat) <= 0) continue;
		if (rcb->cvsat[sat - 1] != 0) {
			sys = satsys(sat, NULL);
			++rcb->sol.ns;
			++rcb->sol.sys_ns[sys2num(sys) - 1];
		}
	}
	/*initialize*/
	gtime_t time;
	int timeu = 2;
	char s[64], id[4];
	uint8_t buff[8192];
	uint8_t* p = buff;
	const char* sep = opt2sep(sopt);
	time = rcb->sol.time;
	time2str(time, s, timeu);
	/*time*/
	p = buff;
	p += sprintf((char*)p, "%s", s);
	p += sprintf((char*)p, "\n");
	fwrite(buff, (int)(p - buff), 1, fp[0]);
	/*output float position data*/
	p = buff;
	int bytes_written = outecef_float(p, s, &rcb->sol, opt, sta, sopt);
	p += bytes_written;
	fwrite(buff, (int)(p - buff), 1, fp[1]);
	/*satellie and receiver info*/
	for (i = 0; i < n && i < MAXOBS; ++i) {
		/*check*/
		sat = obs[i].sat;
		if (!rcb->cvsat[sat - 1]) continue;
		sys = sys2num(satsys(sat, NULL)) - 1;
		/*out satellite*/
		/*satellite position and satellite clock*/
		p = buff;
		satno2id(sat, id);
		p += sprintf((char*)p, "%s%s%15.4f%s%15.4f%s%15.4f%s%15.4f%s", id, sep, rscs[i].rs[0], sep, rscs[i].rs[1], sep, rscs[i].rs[2], sep, CLIGHT * rscs[i].dts[0], sep);
		fwrite(buff, (int)(p - buff), 1, fp[0]);
		/*ionosphere*/
		p = buff;
		p += sprintf((char*)p, "%15.4f%s", rcb->sol.ion[sat - 1], sep);
		fwrite(buff, (int)(p - buff), 1, fp[0]);
		/*ambiguity*/
		p = buff;
		for (j = 0; j < NFREQ; ++j) p += sprintf((char*)p, "%15.4f%s", rcb->sol.amb[sat - 1][j], sep);
		fwrite(buff, (int)(p - buff), 1, fp[0]);
		/*elevation*/
		p = buff;
		p += sprintf((char*)p, "%15.4f%s%15.4f%s", rscs[i].azel[1] * R2D, sep, rscs[i].azel[0] * R2D, sep);
		fwrite(buff, (int)(p - buff), 1, fp[0]);
		/*residuals*/
		p = buff;
		for (j = 0; j < NFREQ; ++j) {
			p += sprintf((char*)p, "%15.4f%s", rcb->sol.resc[sat - 1][j], sep);
		}
		p += sprintf((char*)p, "%s", sep);
		for (j = 0; j < NFREQ; ++j) {
			p += sprintf((char*)p, "%15.4f%s", rcb->sol.resp[sat - 1][j], sep);
		}
		fwrite(buff, (int)(p - buff), 1, fp[0]);
		/*SCB*/
		p = buff;
		for (j = 2; j < NFREQ; ++j) {
			p += sprintf((char*)p, "%15.4f%s", rcb->sol.scb[sat - 1][j], sep);
		}
		p += sprintf((char*)p, "\n");
		fwrite(buff, (int)(p - buff), 1, fp[0]);
	}
}

void OutLogElv(FILE** fp, double maskElv, char id[4], double rsElv) {
	char buff[8192], * p = buff;
	p = buff;
	p += sprintf(p, "Deleter Sat for lower %.1f degree:%s(Ele:%.1f)\n", maskElv, id, rsElv);
	fwrite(buff, (int)(p - buff), 1, fp[2]);
}

void OutLogFrq(FILE** fp, char id[4], int nf, int f) {
	char buff[8192], * p = buff;
	p = buff;
	p += sprintf(p, "Deleter Sat for lack observations, need freq %d:%s(Frq:%d)\n", nf, id, f);
	fwrite(buff, (int)(p - buff), 1, fp[2]);
}

static const char* tfit_band(int sys, int f, int usebds)
{
	static const char* gps[] = { "G_P1","G_P2","G_P5","G_F4","G_F5" };
	static const char* glo[] = { "R_R1","R_R2","R_R3","R_F4","R_F5" };
	static const char* gal[] = { "GAL_E1","GAL_E5a","GAL_E5b","GAL_E6","GAL_E5" };
	static const char* qzs[] = { "J_P1","J_P2","J_P5","J_F4","J_F5" };
	static const char* bds3[] = { "BDS3_B1I","BDS3_B3I","BDS3_B2b","BDS3_B1C","BDS3_B2a" };
	static const char* bds2b[] = { "BDS2_B1I","BDS2_B2I","BDS2_B2b","BDS2_F4","BDS2_F5" };
	static const char* bdsboth[] = { "BDS_B1I","BDS_B3I","BDS3_B2b","BDS3_B1C","BDS3_B2a" };
	static const char* unk[] = { "F1","F2","F3","F4","F5" };
	const char** tab = unk;
	if (f < 0 || f >= 5) return "F?";
	if (sys == 0) tab = gps;
	else if (sys == 1) tab = glo;
	else if (sys == 2) tab = gal;
	else if (sys == 3) tab = qzs;
	else if (sys == 4) {
		if (usebds == 1) tab = bds2b;
		else if (usebds == 2) tab = bdsboth; /* shared B1I/B3I */
		else tab = bds3;
	}
	return tab[f];
}

/* solve RCB = a*T^2 + b*T + c by normal equations. coef = {a,b,c} */
static int tfit_quad(const tfit_t* a, double coef[3])
{
	double A[3][4], piv, fct, tmp;
	int i, j, k, p;
	if (a->n < 3) return 0;
	A[0][0] = a->sT4; A[0][1] = a->sT3; A[0][2] = a->sTT; A[0][3] = a->sT2R;
	A[1][0] = a->sT3; A[1][1] = a->sTT; A[1][2] = a->sT;  A[1][3] = a->sTR;
	A[2][0] = a->sTT; A[2][1] = a->sT;  A[2][2] = (double)a->n; A[2][3] = a->sR;
	for (k = 0; k < 3; k++) {
		p = k;
		for (i = k + 1; i < 3; i++) if (fabs(A[i][k]) > fabs(A[p][k])) p = i;
		if (fabs(A[p][k]) < 1e-8) return 0;
		if (p != k) {
			for (j = k; j < 4; j++) { tmp = A[k][j]; A[k][j] = A[p][j]; A[p][j] = tmp; }
		}
		piv = A[k][k];
		for (i = k + 1; i < 3; i++) {
			fct = A[i][k] / piv;
			for (j = k; j < 4; j++) A[i][j] -= fct * A[k][j];
		}
	}
	for (k = 2; k >= 0; k--) {
		coef[k] = A[k][3];
		for (j = k + 1; j < 3; j++) coef[k] -= A[k][j] * coef[j];
		coef[k] /= A[k][k];
	}
	return 1;
}

/* append one epoch to fit sums and residual series */
int tfit_add(tfit_t* a, gtime_t time, double T, double R)
{
	double T2 = T * T;
	if (a->n >= a->nmax) {
		int ncap = a->nmax ? a->nmax * 2 : 1024;
		gtime_t* nt = (gtime_t*)realloc(a->time, (size_t)ncap * sizeof(gtime_t));
		double* nT = (double*)realloc(a->T, (size_t)ncap * sizeof(double));
		double* nR = (double*)realloc(a->R, (size_t)ncap * sizeof(double));
		if (!nt || !nT || !nR) {
			free(nt ? nt : a->time);
			free(nT ? nT : a->T);
			free(nR ? nR : a->R);
			a->time = NULL; a->T = NULL; a->R = NULL;
			a->nmax = 0;
			return 0;
		}
		a->time = nt;
		a->T = nT;
		a->R = nR;
		a->nmax = ncap;
	}
	a->time[a->n] = time;
	a->T[a->n] = T;
	a->R[a->n] = R;
	a->sT += T;
	a->sR += R;
	a->sTT += T2;
	a->sT3 += T2 * T;
	a->sT4 += T2 * T2;
	a->sTR += T * R;
	a->sT2R += T2 * R;
	a->sRR += R * R;
	a->n++;
	return 1;
}

void tfit_free(tfit_t acc[][NFREQ])
{
	int s, f;
	for (s = 0; s < NSYS; s++) {
		for (f = 0; f < NFREQ; f++) {
			free(acc[s][f].time);
			free(acc[s][f].T);
			free(acc[s][f].R);
			acc[s][f].time = NULL;
			acc[s][f].T = NULL;
			acc[s][f].R = NULL;
			acc[s][f].nmax = 0;
		}
	}
}

/* temperature-RCB quadratic equation, one line per estimated frequency */
void OutTempFit(FILE** fp, const prcopt_t* popt, const tfit_t acc[][NFREQ])
{
	static const int sysbit[5] = { SYS_GPS, SYS_GLO, SYS_GAL, SYS_QZS, SYS_CMP };
	static const char* sysname[5] = { "GPS","GLONASS","Galileo","QZSS","BDS" };
	char buff[8192], eq[160], * p;
	int s, f, nf;
	if (!fp || !fp[0]) return;
	nf = popt->nf;
	if (nf > NFREQ) nf = NFREQ;
	if (nf < 0) nf = 0;
	for (s = 0; s < NSYS && s < 5; s++) {
		if (!(popt->navsys & sysbit[s])) continue;
		for (f = 0; f < nf; f++) {
			const tfit_t* a = &acc[s][f];
			const char* band = tfit_band(s, f, popt->usebds);
			double coef[3], aa, bb, cc, spr, spp, sst, ssr, r2;
			p = buff;
			if (!tfit_quad(a, coef)) {
				p += sprintf(p, "%-12s %-12s %5d  not fitted (need 3 epochs and temperature variation)\n", sysname[s], band, a->n);
				fwrite(buff, (int)(p - buff), 1, fp[0]);
				continue;
			}
			aa = coef[0]; bb = coef[1]; cc = coef[2];
			spr = aa * a->sT2R + bb * a->sTR + cc * a->sR;
			spp = aa * aa * a->sT4 + bb * bb * a->sTT + cc * cc * a->n
				+ 2.0 * aa * bb * a->sT3 + 2.0 * aa * cc * a->sTT + 2.0 * bb * cc * a->sT;
			ssr = a->sRR - 2.0 * spr + spp;
			sst = a->sRR - a->sR * a->sR / a->n;
			if (sst > 1e-16) r2 = 1.0 - ssr / sst;
			else r2 = 0.0;
			sprintf(eq, "RCB = %.6e*T^2 %+.6e*T %+.6e", aa, bb, cc);
			p += sprintf(p, "%-12s %-12s %5d %20.6e %20.6e %20.6e %8.4f  %s\n",
				sysname[s], band, a->n, aa, bb, cc, r2, eq);
			fwrite(buff, (int)(p - buff), 1, fp[0]);
		}
	}
}

/* fit residual = solved RCB - model RCB; all estimated system/bands in one file */
void OutTempFitRes(const filopt_t* fopt, const prcopt_t* popt, const char* fitfilename, const tfit_t acc[][NFREQ])
{
	static const int sysbit[5] = { SYS_GPS, SYS_GLO, SYS_GAL, SYS_QZS, SYS_CMP };
	static const char* sysname[5] = { "GPS","GLONASS","Galileo","QZSS","BDS" };
	char path[1024], buff[512], tstr[64], *p;
	int s, f, i, nf, nband = 0;
	double coef[NSYS][NFREQ][3];
	unsigned char ok[NSYS][NFREQ];
	FILE* fp;

	if (!fopt || !popt || !fitfilename || !acc) return;
	nf = popt->nf;
	if (nf > NFREQ) nf = NFREQ;
	if (nf < 0) nf = 0;
	memset(ok, 0, sizeof(ok));

	for (s = 0; s < NSYS && s < 5; s++) {
		if (!(popt->navsys & sysbit[s])) continue;
		for (f = 0; f < nf; f++) {
			if (!tfit_quad(&acc[s][f], coef[s][f])) continue; /* not estimated: skip */
			if (!acc[s][f].time || !acc[s][f].T || !acc[s][f].R || acc[s][f].n <= 0) continue;
			ok[s][f] = 1;
			nband++;
		}
	}
	if (nband <= 0) return;

	snprintf(path, sizeof(path), "%s%s.tres", fopt->outFile_OUT, fitfilename);
	if (!(fp = fopen(path, "w"))) {
		printf("open fit residual file error: %s\n", path);
		return;
	}
	fprintf(fp, "%s Temperature-RCB fit residual\n", COMMENTH);
	fprintf(fp, "%s residual = RCB_solved - RCB_model,  RCB_model = a*T^2 + b*T + c\n", COMMENTH);
	fprintf(fp, "%s Only estimated system/bands are listed\n", COMMENTH);
	for (s = 0; s < NSYS && s < 5; s++) {
		if (!(popt->navsys & sysbit[s])) continue;
		for (f = 0; f < nf; f++) {
			if (!ok[s][f]) continue;
			fprintf(fp, "%s %-10s %-12s  RCB = %.6e*T^2 %+.6e*T %+.6e  (N=%d)\n",
				COMMENTH, sysname[s], tfit_band(s, f, popt->usebds),
				coef[s][f][0], coef[s][f][1], coef[s][f][2], acc[s][f].n);
		}
	}
	fprintf(fp, "%s\n", COMMENTH);
	fprintf(fp, "%-19s %-10s %-12s %12s %14s %14s %14s\n",
		"Time(GPST)", "System", "Band", "T(degC)", "RCB(m)", "RCBfit(m)", "residual(m)");

	for (s = 0; s < NSYS && s < 5; s++) {
		if (!(popt->navsys & sysbit[s])) continue;
		for (f = 0; f < nf; f++) {
			const tfit_t* a;
			const char* band;
			double aa, bb, cc, Rfit, res;
			if (!ok[s][f]) continue;
			a = &acc[s][f];
			band = tfit_band(s, f, popt->usebds);
			aa = coef[s][f][0]; bb = coef[s][f][1]; cc = coef[s][f][2];
			for (i = 0; i < a->n; i++) {
				Rfit = aa * a->T[i] * a->T[i] + bb * a->T[i] + cc;
				res = a->R[i] - Rfit;
				time2str(a->time[i], tstr, 1);
				p = buff;
				p += sprintf(p, "%-19s %-10s %-12s %12.4f %14.6f %14.6f %14.6f\n",
					tstr, sysname[s], band, a->T[i], a->R[i], Rfit, res);
				fwrite(buff, (size_t)(p - buff), 1, fp);
			}
		}
	}
	fclose(fp);
}
