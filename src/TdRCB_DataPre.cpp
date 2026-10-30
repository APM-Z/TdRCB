#include "TdRCB.h"        
#include "TdRCB_SolHead.h"
#include "TdRCB_SolBody.h"


/* constants/global variables */
static pcvs_t pcvss={0};        /* satellite antenna parameters */
static pcvr_t pcvsr={0};        /* receiver antenna parameters */
static obs_t obss={0};          /* observation data */
static nav_t navs={0};          /* navigation data */
static sta_t stas;              /* station infomation */

/* set antenna parameters */
static void setpcv(gtime_t time, const prcopt_t* popt, nav_t* nav, const pcvs_t* pcvs, const pcvr_t* pcvr, sta_t* sta)
{
    pcv_t* pcv;
    int i, j;
    char id[64];
    for (i = 0; i < MAXSAT; i++) {
        if (!(satsys(i + 1, NULL) & popt->navsys)) continue;
        if (!(pcv = searchpcv(i + 1, time, pcvs))) {
            satno2id(i + 1, id);
            continue;
        }
        nav->pcvs[i] = *pcv;
    }
    for (i = 0; i < 1; ++i) {
        for (j = 0; j < pcvr->n; ++j) {
            if (!strcmp(sta[i].antsno, pcvr->pcv[j].type)) {
                sta[i].pcvr = pcvr->pcv[j];
            }
        }
    }
}

/* free obs and nav data */
static void freeobsnav(obs_t *obs, nav_t *nav)
{
    free(obs->data); obs->data=NULL; obs->n =obs->nmax =0;
    free(nav->eph ); nav->eph =NULL; nav->n =nav->nmax =0;
    free(nav->geph); nav->geph=NULL; nav->ng=nav->ngmax=0;
    free(nav->seph); nav->seph=NULL; nav->ns=nav->nsmax=0;
}

/* input obs data */
static int inputobs(FILE*fp,int index,obs_t *obs,double *ver,int *tsys,char tobs[][NUMSYS][MAXOBSTYPE][4], const prcopt_t* popt, char* file)
{
	gtime_t ts={0},te={0};
	int n;
	double tint=0.0;
	obs->n=0;
	tint=0.0;
    readrnxobs(fp, ts, te, tint, NULL, 1, ver[index], &tsys[index], tobs[index], obs, NULL, popt);
	n=sortobs(obs);
	return n;
}
/* initialize control */
static void init(rcb_t *rcb, const prcopt_t *opt)
{
    sol_t  sol0={{0}};
    ssat_t ssat0={0};
    int i;
	rcb->opt=*opt;
    for (i=0;i<MAXSAT*MAXRCV;i++) {
        rcb->ssat[i]=ssat0;
    }
    
    for (i=0;i<NSYS;++i) {
        rcb->rsat[i]=-1;
        rcb->basis_freq[i]=0;
    }
    rcb->sol=sol0; 
    rcb->temp=0.0;
    rcb->tempok=0;
    rcb->nx=rtknx(opt); 
    rcb->tt=0.0;
    rcb->F=(unsigned char *)calloc(sizeof(unsigned char),rcb->nx); memset(rcb->F,0,(unsigned char)rcb->nx);
    rcb->x=zeros(rcb->nx,1);
    rcb->P=zeros(rcb->nx,rcb->nx);
}
/* free rtk control */
static void confree(rcb_t *rcb)
{
    rcb->nx=0;
    free(rcb->F);  rcb->F =NULL;
    free(rcb->x ); rcb->x =NULL;
    free(rcb->P ); rcb->P =NULL;
}

/* read nav data */
static int readnavdata(char* infile, nav_t* nav)
{
    FILE* fp;
    char type = ' ';
    if (!(fp = fopen(infile, "r"))) {
        printf("open nav file error: %s\n", infile);
        return 0;
    }
        
    int sys, tsys = TSYS_GPS;
    double ver;
    char tobs[NUMSYS][MAXOBSTYPE][4] = { {""} };
    /* read rinex header */
    if (!readrnxh(fp, &ver, &type, &sys, &tsys, NULL, nav, NULL)) return 0;
    /* read rinex body */
    switch (type) {
    case 'N': return readrnxnav(fp, NULL, ver, sys, nav);
    case 'G': return readrnxnav(fp, NULL, ver, SYS_GLO, nav);
    case 'H': return readrnxnav(fp, NULL, ver, SYS_SBS, nav);
    case 'J': return readrnxnav(fp, NULL, ver, SYS_QZS, nav);
    case 'L': return readrnxnav(fp, NULL, ver, SYS_GAL, nav);
    default:return 0;
    }
}
/* process nav */
static int readnav(char* infile, nav_t* nav, const prcopt_t* popt)
{
    int stat;
    stat = readnavdata(infile, nav);
    if (nav->n <= 0 && nav->ng <= 0 && nav->ns <= 0) {
        return 0;
    }
    uniqnav(nav, popt);
    return stat;
}

/* read optional temperature csv and fill a 24 h / 30 s / 2880-epoch series.
 * columns: YY MM DD HH MI SS TEM. blank TEM is a gap and is interpolated. */
extern int readtemp(const char *file, temp_t *temp)
{
    FILE *fp;
    char line[1024], *fld[16], *p;
    gtime_t *ts = NULL;
    double *vs = NULL;
    int ns = 0, cap = 0, i, i0, i1, n, nfill = 0;
    gtime_t t0, t1;

    memset(temp, 0, sizeof(temp_t));
    if (!file || !file[0]) return 0;
    if (!(fp = fopen(file, "r"))) {
        printf("temperature file skipped (cannot open): %s\n", file);
        return 0;
    }
    while (fgets(line, sizeof(line), fp)) {
        int nf = 0, yy, mm, dd, hh, mi;
        double ss, tem, ep[6];
        gtime_t t;
        for (p = line; *p; p++) if (*p == '\r' || *p == '\n') *p = '\0';
        if (line[0] == '\0') continue;
        fld[nf++] = line;
        for (p = line; *p && nf < 16; p++) {
            if (*p == '\t' || *p == ',') {
                *p = '\0';
                fld[nf++] = p + 1;
            }
        }
        if (nf < 7 || !isdigit((unsigned char)fld[0][0])) continue;
        if (fld[6][0] == '\0') continue;
        yy = atoi(fld[0]); mm = atoi(fld[1]); dd = atoi(fld[2]);
        hh = atoi(fld[3]); mi = atoi(fld[4]); ss = atof(fld[5]); tem = atof(fld[6]);
        if (yy < 1980 || mm < 1 || mm > 12 || dd < 1 || dd > 31) continue;
        ep[0] = yy; ep[1] = mm; ep[2] = dd; ep[3] = hh; ep[4] = mi; ep[5] = ss;
        t = epoch2time(ep);
        if (ns >= cap) {
            int ncap = cap ? cap * 2 : 4096;
            gtime_t *nt = (gtime_t *)realloc(ts, ncap * sizeof(gtime_t));
            double *nv = (double *)realloc(vs, ncap * sizeof(double));
            if (!nt || !nv) {
                free(nt ? nt : ts);
                free(nv ? nv : vs);
                fclose(fp);
                printf("temperature file: out of memory\n");
                return 0;
            }
            ts = nt;
            vs = nv;
            cap = ncap;
        }
        ts[ns] = t;
        vs[ns] = tem;
        ns++;
    }
    fclose(fp);
    if (ns < 2) {
        free(ts); free(vs);
        printf("temperature: not enough samples in %s\n", file);
        return 0;
    }
    t0 = ts[0];
    t1 = ts[0];
    for (i = 1; i < ns; i++) {
        if (timediff(ts[i], t0) < 0.0) t0 = ts[i];
        if (timediff(ts[i], t1) > 0.0) t1 = ts[i];
    }
    n = (int)(timediff(t1, t0) / TEMP_DT + 0.5) + 1;
    if (n != TEMP_NEPOCH) {
        free(ts); free(vs);
        printf("temperature: expected %d epochs (24 h, 30 s), got %d\n", TEMP_NEPOCH, n);
        return 0;
    }
    for (i = 0; i < TEMP_NEPOCH; i++) {
        temp->time[i] = timeadd(t0, i * TEMP_DT);
        temp->val[i] = 0.0;
        temp->ok[i] = 0;
    }
    for (i = 0; i < ns; i++) {
        double dt = timediff(ts[i], t0);
        int idx = (int)floor(dt / TEMP_DT + 0.5);
        if (idx < 0 || idx >= TEMP_NEPOCH) continue;
        if (fabs(dt - idx * TEMP_DT) > TEMP_DT * 0.5) continue;
        temp->val[idx] = vs[i];
        temp->ok[idx] = 1;
    }
    free(ts);
    free(vs);
    for (i0 = 0; i0 < TEMP_NEPOCH && !temp->ok[i0]; i0++);
    for (i1 = TEMP_NEPOCH - 1; i1 >= 0 && !temp->ok[i1]; i1--);
    if (i0 < i1) {
        int L = i0, k = i0 + 1;
        while (k < i1) {
            int R;
            if (temp->ok[k]) { L = k; k++; continue; }
            for (R = k + 1; R <= i1 && !temp->ok[R]; R++);
            for (; k < R; k++) {
                double a = (double)(k - L) / (double)(R - L);
                temp->val[k] = temp->val[L] * (1.0 - a) + temp->val[R] * a;
                temp->ok[k] = 2;
                nfill++;
            }
            L = R;
        }
    }
    temp->n = TEMP_NEPOCH;
    temp->nfill = nfill;
    printf("temperature: %d epochs, 24 h, 30 s, interpolated %d\n", temp->n, nfill);
    return 1;
}

/* write fit equations and per-band residuals, then free epoch series */
static void emittempfit(const prcopt_t *popt, const filopt_t *fopt, char *fitfilename, tfit_t acc[NSYS][NFREQ])
{
    FILE *fp = NULL;
    if (creatTempFit(&fp, fopt, popt, fitfilename)) {
        OutTempFit(&fp, popt, acc);
        fclose(fp);
    }
    OutTempFitRes(fopt, popt, fitfilename, acc);
    tfit_free(acc);
}

/* process */
static void process(const prcopt_t *popt, const solopt_t *sopt, Ifile infile,int len, const filopt_t* fopt)
{
    
    char type=' ',tobs[60][NUMSYS][MAXOBSTYPE][4]={{{""}}};
    sol_t sol={0};rcb_t rcb;obs_t obs={0};
	char str[25];
	double ver[60];
    int i, nobs, n, prn, sys, tsys[60] = { TSYS_GPS };
    int k,judge=0;
    double ep[6] = { 0.0 };
    FILE* fp[3] = { NULL };
    int first_run = 1;
    char outfilename[256], profilename[256], logfilename[256], fitfilename[256];
    temp_t *tdat = NULL;
    const char *tpath;
    tfit_t acc[NSYS][NFREQ];
    memset(acc, 0, sizeof(acc));


    char sysstr[16] = "";
    // Build system string based on navigation systems
    if (popt->navsys & 1) strcat(sysstr, "G");   // GPS
    if (popt->navsys & 4) strcat(sysstr, "R");   // GLONASS  
    if (popt->navsys & 8) strcat(sysstr, "E");   // GALILEO
    if (popt->navsys & 16) strcat(sysstr, "J");  // QZSS
    if (popt->navsys & 32) strcat(sysstr, "C");  // BDS
    // Use default value if no systems are selected
    if (strlen(sysstr) == 0) {
        strcpy(sysstr, "Nosystem");
    }
    /* Create output files */
    if (popt->CONTINUE == 0) {
        sprintf(outfilename, "%s_%s_%d_%s_RCB", popt->staname, sysstr, popt->nf, popt->DOY);
        sprintf(profilename, "%s_%s_%d_%s_Inf", popt->staname, sysstr, popt->nf, popt->DOY);
        sprintf(logfilename, "%s_%s_%d_%s_Sat", popt->staname, sysstr, popt->nf, popt->DOY);
        sprintf(fitfilename, "%s_%s_TempFit", popt->staname, popt->DOY);
        if (!creatFile(fp, fopt, popt, sopt, outfilename, profilename, logfilename)) {
            return;
        }
    }
    else {
        sprintf(outfilename, "%s_%s_%d_Continues_RCB", popt->staname, sysstr, popt->nf);
        sprintf(profilename, "%s_%s_%d_Continues_Inf", popt->staname, sysstr, popt->nf);
        sprintf(logfilename, "%s_%s_%d_Continues_Sat", popt->staname, sysstr, popt->nf);
        sprintf(fitfilename, "%s_Continues_TempFit", popt->staname);
        if (!creatFile(fp, fopt, popt, sopt, outfilename, profilename, logfilename)) {
            return;
        }
    }


    init(&rcb, popt);

    tpath = fopt->temp;
    while (*tpath == ' ' || *tpath == '\t') tpath++;
    if (tpath[0]) {
        if (!(tdat = (temp_t *)calloc(1, sizeof(temp_t))) || !readtemp(tpath, tdat)) {
            free(tdat);
            tdat = NULL;
        }
    }

    for (k = 0; k < len; k++) {
        /* read nav */
        navs.eph = NULL; navs.n = navs.nmax = 0;
        navs.geph = NULL; navs.ng = navs.ngmax = 0;
        navs.seph = NULL; navs.ns = navs.nsmax = 0;
        if (!readnav(infile.rnxfile[k], &navs, popt)) {
            printf("read nav file error: %s\n", infile.rnxfile[k]);
            if (tdat) emittempfit(popt, fopt, fitfilename, acc);
            free(tdat);
            return;
        }
        /* read obs header */
        FILE* fpobs;
        if (!(obs.data = (obsd_t*)malloc(sizeof(obsd_t) * MAXOBS))) {
            printf("alloc obs buffer error\n");
            if (tdat) emittempfit(popt, fopt, fitfilename, acc);
            free(tdat);
            return;
        }
        if (!(fpobs = fopen(infile.obsfile[k], "r"))) {
            printf("open obs file error: %s\n", infile.obsfile[k]);
            free(obs.data);
            if (tdat) emittempfit(popt, fopt, fitfilename, acc);
            free(tdat);
            return;
        }
        memset(&stas, 0, sizeof(stas));
        if (!readrnxh(fpobs, &ver[k], &type, &sys, &tsys[k], tobs[k], &navs, &stas)) {
            printf("read obs header error: %s\n", infile.obsfile[k]);
            fclose(fpobs);
            free(obs.data);
            if (tdat) emittempfit(popt, fopt, fitfilename, acc);
            free(tdat);
            return;
        }

        while ((nobs = inputobs(fpobs, k, &obs, ver, tsys, tobs, popt, infile.obsfile[k])) > 0) {
            for (i = n = 0; i < nobs; i++) {
                if (((sys = satsys(obs.data[i].sat, &prn)) & popt->navsys) &&
                    popt->exsats[obs.data[i].sat - 1] != 1) {
                    obs.data[n++] = obs.data[i];
                }
            }
            if (n <= 0) continue;
            rcb.temp = 0.0;
            rcb.tempok = 0;
            if (tdat && tdat->n == TEMP_NEPOCH) {
                double dt = timediff(obs.data[0].time, tdat->time[0]);
                int idx = (int)floor(dt / TEMP_DT + 0.5);
                if (idx >= 0 && idx < TEMP_NEPOCH && tdat->ok[idx]) {
                    rcb.temp = tdat->val[idx];
                    rcb.tempok = 1;
                }
            }
            {
                int rcb_ready = (rcb.opt.Initial != 0);
                int sfit, ffit;
                static const int sysbit[5] = { SYS_GPS, SYS_GLO, SYS_GAL, SYS_QZS, SYS_CMP };
                if (!calrcb(fp, &rcb, obs.data, n, &navs, &stas, sopt)) {
                    judge = 1;
                    break;
                }
                if (rcb_ready && rcb.tempok && rcb.sol.stat == SOLQ_FLOAT) {
                    int nf = popt->nf < NFREQ ? popt->nf : NFREQ;
                    for (sfit = 0; sfit < NSYS && sfit < 5; sfit++) {
                        if (!(popt->navsys & sysbit[sfit])) continue;
                        for (ffit = 0; ffit < nf; ffit++) {
                            tfit_add(&acc[sfit][ffit], rcb.sol.time, rcb.temp, rcb.sol.rcb[sfit][ffit]);
                        }
                    }
                }
            }
            time2str(rcb.sol.time, str, 0);
            if (rcb.TqLOM > 1E5) {
                judge = 1;
                printf("%s TqLOM=%.2f   Observation quality may be poor\n",str, rcb.TqLOM);
                break;
            }

            if (rcb.sol.stat == SOLQ_FLOAT) {
                printf("%s %s ", str, "FLOAT");
                printf("%6.3f %6.3f %6.3f %8.3f %6.3f %8.3f %6.3f %6.3f %6.3f %6.3f %8.3f %6.3f %6.3f %8.3f %6.3f %6.3f %6.3f %6.3f %6d %10.3f\n",
                    rcb.sol.rcb[0][0], rcb.sol.rcb[0][1], rcb.sol.rcb[0][2], 
                    rcb.sol.rcb[1][0], rcb.sol.rcb[1][1],
                    rcb.sol.rcb[2][0], rcb.sol.rcb[2][1], rcb.sol.rcb[2][2], rcb.sol.rcb[2][3], rcb.sol.rcb[2][4],
                    rcb.sol.rcb[3][0], rcb.sol.rcb[3][1], rcb.sol.rcb[3][2],
                    rcb.sol.rcb[4][0], rcb.sol.rcb[4][1], rcb.sol.rcb[4][2], rcb.sol.rcb[4][3], rcb.sol.rcb[4][4],
                    rcb.sol.ns, rcb.TqLOM);
            }
            else if (rcb.sol.stat == SOLQ_SINGLE) {
                printf("%s %s ", str, "SINGLE");
                printf("%10.4f %10.4f %10.4f %10.4f %10.4f %10.4f %10.4f %10.4f %10.4f %10.4f %10.4f %10.4f %10.4f %10.4f %10.4f %10.4f %10.4f %10.4f %6d %10.3f\n",rcb.sol.rcb[0][0], rcb.sol.rcb[0][1], rcb.sol.rcb[0][2], 
                    rcb.sol.rcb[1][0], rcb.sol.rcb[1][1],
                    rcb.sol.rcb[2][0], rcb.sol.rcb[2][1], rcb.sol.rcb[2][2], rcb.sol.rcb[2][3], rcb.sol.rcb[2][4],
                    rcb.sol.rcb[3][0], rcb.sol.rcb[3][1], rcb.sol.rcb[3][2],
                    rcb.sol.rcb[4][0], rcb.sol.rcb[4][1], rcb.sol.rcb[4][2], rcb.sol.rcb[4][3], rcb.sol.rcb[4][4],
                    rcb.sol.ns, rcb.TqLOM);
            }
        }
        if (!judge) {
            fclose(fpobs);
            free(obs.data);
        }
        else {
            break;
        }
    }
    if (tdat) emittempfit(popt, fopt, fitfilename, acc);
    free(tdat);

    for (i = 0; i < 3; i++) {
        if (fp[i] != NULL) {
            fclose(fp[i]);
        }
    }
    confree(&rcb);
}
extern int postpos(prcopt_t *popt, const solopt_t *sopt, filopt_t filopt)
{
    Ifile ifiles;
    if (!initIfile(&ifiles)) return 0;
    if (!getpath(&ifiles,popt,filopt)) return 0;
    process(popt, sopt, ifiles, ifiles.idx[0], &filopt);
    freeobsnav(&obss, &navs);
    freeIfile(&ifiles);
    printf("ending!\n");
    return 1;
}
