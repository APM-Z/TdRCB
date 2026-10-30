#ifndef TdRCB_SolHead_H
#define TdRCB_SolHead_H

#include "TdRCB.h"

#define NUMSYS      8
int creatFile(FILE** fp, const filopt_t* fopt, const prcopt_t* popt, const solopt_t* sopt, char* filename, char* profilename, char* logfilename);
int creatTempFit(FILE** fp, const filopt_t* fopt, const prcopt_t* popt, char* fitfilename);
#endif
