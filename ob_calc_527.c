/*****************************************************************************
 *
 * File: ob_calc_527.c   - calculate best fit clock for ICS527 device
 *
 * $RCSfile: ob_calc_527.c,v $
 *
 * Copyright (C) 2003 D-TACQ Solutions Ltd
 * not to be used without owner's permission
 *
 * Description:
 *
 * $Id: ob_calc_527.c,v 1.2 2008/11/30 12:31:42 pgm Exp $
 * $Log: ob_calc_527.c,v $
 * Revision 1.2  2008/11/30 12:31:42  pgm
 * correct calc for 300kHz rule
 *
 * Revision 1.1  2008/11/13 14:49:29  pgm
 * *** empty log message ***
 *
 *
 *
\*****************************************************************************/
 
#include "local.h"
#include <stdlib.h>

#include <errno.h>

#define USAGE "[opts] <freq kHz> </dev/dtacq/ob_clock>\n$Revision: 1.2 $"

#define REVID "$Revision: 1.2 $ Build 1001"

#include "popt.h"

/*
#define def ob_clock_def
	return sprintf(buf, "%d %d %d %d %d %d",
		   def.demand, def.actual,
		       def.FDW, def.RDW, def.R, def.Sx);
#undef def

 */

int acq200_debug;

extern const char* core_rev;

static struct OB_CLOCK_DEF {
	int demand;
	int actual;
	int FDW;
	int RDW;
	int R;
	int Sx;
} def;


int f_in_khz = 2000;

const char* obdevice = "/dev/dtacq/ob_clock"; 

struct poptOption opt_table[] = {
	{ "fin",	'f', POPT_ARG_INT,	&f_in_khz, 0   },
	{ "output",	'o', POPT_ARG_STRING,	&obdevice, 'o' },
	{ "verbose",	'd', POPT_ARG_INT,	&acq200_debug, 0},
	{ "version",	'v', POPT_ARG_NONE,	0, 'v'},
	POPT_AUTOHELP
	POPT_TABLEEND
};


#define RMAX 127
#define CLKDIV(d) ((d)-1)	       

/*
 *
	Output Frequency = 33.333 MHz  x FDW+2/RDW+2
	The value for RDW must also meet the following criteria
	300 kHz < Input Frequency/RDW+2
 */

#define XDW_VALID(xdw) (IN_RANGE(xdw, 0, RMAX))

/* 300kHz < Input/(RDW+2) */
#define OP_RANGE_SPEC	300

/* turns out this isn't achievable with Input > 4M, so allow relaxed spec: */
#define OP_RANGE_SPEC2  1200

/* previous incarnation was running > 1.33 MHz, and it was OK :-) */

static int find_best_ratio()
{
	int f_pll = def.demand;
	int rdw2;
	int fdw2;
	int op_range = OP_RANGE_SPEC;

	do {
		int valid = XDW_VALID(f_in_khz/op_range -2);
		dbg(2, "f:%d op:%d XDW_VALID:%d", f_in_khz, op_range, valid);
		if (valid){
			break;
		}
		op_range++;
	} while(1);

	struct Best {
		int rdw2;
		int fdw2;
		int error;
		int valid;
	} best = {0};

	/* now choose largest RDW */

search_value:
	for (rdw2 = f_in_khz/op_range; XDW_VALID(rdw2-2); ++rdw2){
		fdw2 = f_pll * rdw2 / f_in_khz;
	
		dbg(3, "op_range %d rdw2 %d fdw2 %d pll_actual %d %s", 
		    op_range, rdw2, fdw2, 
		    f_in_khz * fdw2/rdw2,
		    XDW_VALID(fdw2-2)? "OK": "NOT VALID");

		if (XDW_VALID(fdw2-2)){
			int pll_actual = f_in_khz * fdw2/rdw2;

			if (pll_actual <= f_pll){
				int _error = f_pll - pll_actual;
				if (!best.valid || _error < best.error){

					dbg(2, "best: fdw2:%d rdw2:%d error:%d",
					    fdw2, rdw2, _error);

					best.valid = 1;
					best.error = _error;
					best.rdw2 = rdw2;
					best.fdw2 = fdw2;
				}
			}
		}else{
			if (fdw2 -2 > RMAX){
				if (best.valid){
					break;
				}else if (op_range++ < OP_RANGE_SPEC2){
/*
					info("downgrade op_range to %d",
					     op_range);
*/
					goto search_value;
				}else{
					if (!best.valid){
						err("Input freq too high");
					}
					break;
				}
			}
		}
	}

	if (best.valid){
		def.actual = f_in_khz * best.fdw2/best.rdw2;
		def.FDW = best.fdw2 - 2;
		def.RDW = best.rdw2 - 2;
		info("op_range %d", op_range);		
	}else{
		err("BEST value not found - please change fin");
		return -1;
	}

	dbg(1, "demand:%d actual:%d error:%d (%d %%)",
	    def.demand, def.actual, def.actual-def.demand,
	    (100*(def.actual - def.demand))/def.demand);

	return 0;
}

static int ics527_calc_settings(void)
{
	if (def.demand < 10000){
		def.Sx = 0x2;
	}else if (def.demand < 18000){
		def.Sx = 0x1;
	}else if (def.demand < 37000){
		def.Sx = 0x1;
	}else if (def.demand < 75000){
		def.Sx = 0x0;
	}else{
		def.Sx = 0x3;
	}
	def.R = 1; /* CLKDIV(div); THERE IS NO DIVIDER */

	return find_best_ratio();		
}

static void ics527_print_settings(FILE *file)
{
	fprintf(file, "%d %d %d %d %d %d\n",
		   def.demand, def.actual,
		       def.FDW, def.RDW, def.R, def.Sx);
}

int main(int argc, const char* argv[])
{
	int rc;
	poptContext opt_context =
		poptGetContext( argv[0], argc, argv, opt_table, 0 );
	const char* f_set;

	while ((rc = poptGetNextOpt(opt_context)) > 0){
		switch(rc){
		case 'v':
			fprintf(stderr,"%s %s %s\n", argv[0], REVID, USAGE);
			return 1;
		}
	}

	
	f_set = poptGetArg(opt_context);

	if (f_set){
		if (strcmp(obdevice, "-") == 0 || 
		    strcmp(obdevice, "stdout") == 0){
			;
		}else{
			fclose(stdout);
			stdout = fopen(obdevice, "w");
			if (stdout == 0){
				fprintf(stderr, "ERROR: fopen %s\n", obdevice);
				return -errno;
			}
		}
		def.demand = strtoul(f_set, 0, 0);
		if (def.demand < 4000 || def.demand > 120000){
			fprintf(stderr, "ERROR: OOR\n");
			return -1;
		}
		if (ics527_calc_settings() == 0){
			if (acq200_debug){
				ics527_print_settings(stderr);
			}			
			ics527_print_settings(stdout);
		}else{
			fprintf(stderr, "ERROR: not feasible\n");
		}
	}else{
		fprintf(stderr, "%s %s %s\n", argv[0], REVID, USAGE);
	}

	return 0;
}
