/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2013 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>
    http://www.d-tacq.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of Version 2 of the GNU General Public License
    as published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                */
/* ------------------------------------------------------------------------- */

/** @file fs2xml convert file system tree to xml
 * Refs:
*/

/*
<?xml version="1.0" encoding="UTF-8"?>
<acqDataXML>
   <acqInfo>
	<host>$host</host>
	<uptime>$uptime</uptime>
	<date>$date</date>
   </acqInfo> 
   <acqData id="1" n="Voltage" v="10"/>
   <acqData id="2" n="SFP Status" v="none"/>
</acqDataXML>

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "local.h"

#include "popt.h"

#define MAXBUF 8192		/* JL said 4096 was not enough */

FILE *fout;

int row_limit = 40;

char* readCommand(const char* cmd, char* buf, int maxbuf)
{
	FILE* fp = popen(cmd, "r");
	char *ans = fgets(buf, maxbuf, fp);
	pclose(fp);
	if (ans){
		return chomp(ans);
	}else{
		return ans;
	}
}
char* getHostname(char* buf, int maxbuf) {
	return readCommand("hostname", buf, maxbuf);
}

char* getUptime(char * buf, int maxbuf) {
	return readCommand("uptime", buf, 10);
}

char* getDate(char *buf, int maxbuf) {
	return readCommand("date", buf, maxbuf);
}

void printInfo() {
	char buf[80];

	fprintf(fout, "\t<acqInfo>\n");
	fprintf(fout, "\t\t<host>%s</host>\n", getHostname(buf, 80));
	fprintf(fout, "\t\t<date>%s</date>\n", getDate(buf, 80));
	fprintf(fout, "\t\t<uptime>%s</uptime>\n", getUptime(buf, 80));
	fprintf(fout, "\t</acqInfo>\n");
}
void printHeader() {
	fprintf(fout, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	fprintf(fout, "<acqDataXML>\n");
	printInfo();
}

char* getLine(const char* path)
{
	FILE *fp = fopen(path, "r");
	if (fp == 0){
		return "not readable";
	}else{
		static char buf[128];
		fgets(buf, sizeof(buf), fp);
		fclose(fp);
		return buf;
	}
}
void printRecord(int id, const char* path)
{
	fprintf(fout, "\t<acqData id=\"%d\" n=\"%s\" v=\"%s\"/>\n",
	       id, path, getLine(path));
}

void printV(const char* vdata)
{
	fprintf(fout, "\t\t<v><![CDATA[%s]]>\t\t</v>\n", vdata);
}

void printV1(void)
{
	fprintf(fout, "\t\t<v><![CDATA[");
}
void printV2(void)
{
	fprintf(fout, "]]>\t\t</v>\n");
}
void printRecordKargsVstdin(int ii, const char **args)
{	
	char *inbuf = malloc(MAXBUF);
	ii =0;

	while(fgets(inbuf, MAXBUF, stdin) != 0 && args[ii] != NULL){

		fprintf(fout, "\t<acqData id=\"%d\" n=\"%s\">\n", 
						ii, args[ii]);
		printV(chomp(inbuf));
		fprintf(fout, "\t</acqData>\n");
		++ii;
	}
	free(inbuf);
}

void printRecordStdinValue(int id, const char* name)
{
	char *inbuf = malloc(MAXBUF);
	char **rows = calloc(250, sizeof(char*));
	char **col2 = 0;
	char **col1 = 0;
	char *l1 = inbuf;
	char *l2;
	int rem = MAXBUF;
	int row = 0;
	int break_hint = 0;
	
	while (rem > 0 && (l2 = fgets(l1, rem, stdin)) && chomp(l2)){
		int ll = strlen(l2) + 1;	/* include the '\0' */
		if (break_hint){
			col2 = &rows[row];
			break_hint = 0;
		}
		if (ll == 1){
			break_hint = 1;
		}
		rows[row++] = l2;

		l1 = l2 + ll;
		rem -= ll;
	}
	if ((row&1) != 0){
		rows[row++] = "\0";
	}
	col1 = &rows[0];

	fprintf(fout, "\t<acqData id=\"%d\" n=\"%s\">\n", id, name);
	printV1();
	if (row_limit == 0 || row < row_limit){
		for (int outrow = 0; outrow < row; ++outrow){
			fprintf(fout, "%s\n", col1[outrow]);
		}
	}else{
		int outmax = row/2;
		if (col2 == 0){
			col2 = &rows[outmax];
		}
		
		for (int outrow = 0; outrow < outmax; ++outrow){
			fprintf(fout, "%40s %40s\n", col1[outrow], col2[outrow]);
		}
	}
	printV2();
	fprintf(fout, "\t</acqData>\n");
	free(inbuf);
}
void printRecordSubProc(int id, const char* name, const char* proc)
{
	FILE *fp = popen(proc, "r");
        char *inbuf = malloc(MAXBUF);
        char *l1 = inbuf;
        char *l2;

	if (fp != 0){
		int rem = MAXBUF;
	        while (rem > 0 && (l2 = fgets(l1, rem, fp)) != 0){
			int ll = strlen(l2);
        	        l1 = l2 + ll;
			rem -= ll;
	        }
		pclose(fp);
	}else{
		sprintf(inbuf, "command \"%s\" failed", proc);
	}

        fprintf(fout, "\t<acqData id=\"%d\" n=\"%s\">\n", id, name);
	printV(inbuf);
        fprintf(fout, "\t</acqData>\n");
        free(inbuf);
}

void printRecordPair(int id, const char* pair)
{
	char key[128];
	char value[128];
	int np = sscanf(pair, "%s %s", key, value);

	if (np == 2){
		fprintf(fout, "\t<acqData id=\"%d\" n=\"%s\">\n", id, key);
		printV(value);
		fprintf(fout, "\t</acqData>\n");
	}
}

void printFooter() {
	fprintf(fout, "</acqDataXML>\n");
}

enum MODE {
	M_NOTHING,
	M_FILENAMES_ON_STDIN,	/* k=fname v=$(cat fname)	*/
	M_FILENAMES_ARGV,	/* k=$1 v=$(cat $1) ..		*/
	M_KVP_ARGV,		/* k v [k v] ...		*/
	M_K_ARG_V_STDIN,	/* k arg, v=stdin		*/
	M_K_ARGS_V_STDIN,	/* k $1 v=stdin1, k $2...       */
	M_PROC			/* k arg v=$(arg)		*/
} mode = M_NOTHING;

static char *outfile;
static const char* key;
static char* snippetfile;

static void insertSnippet(void)
{
	if (snippetfile != 0){
		char buf[132];
		FILE* fp = fopen(snippetfile, "r");
		if (!fp){
			perror(snippetfile);
		}else{
			while(fgets(buf,132,fp)){
				fputs(buf, fout);
			}
			fclose(fp);
		}
	}
}
const char** processArgs(int argc, const char* argv[])
{
	static struct poptOption opt_table[] = {
		{ "outfile", 'o', POPT_ARG_STRING, &outfile,	'o' },
		{ "snippet", 's', POPT_ARG_STRING, &snippetfile, 0 },
		{ "kvp",     0,   POPT_ARG_NONE,   0,		'k' },
		{ "key",     'k', POPT_ARG_STRING, &key,	'Y' },
		{ "keys",    'K', POPT_ARG_NONE,   0,           'K' },
		{ "proc",    'p', POPT_ARG_NONE,   0,		'p' },
		POPT_AUTOHELP
		POPT_TABLEEND		
	};
	fout = stdout;
	poptContext opt_context =
                poptGetContext(argv[0], argc, argv, opt_table, 0);
	int rc;

	if (getenv("ROW_LIMIT")){
		row_limit = atoi(getenv("ROW_LIMIT"));
	}

	while ((rc = poptGetNextOpt(opt_context)) > 0){
		switch(rc){
		case 'o':
			fout = fopen(outfile, "w");
			if (fout == 0){
				perror(outfile);
				exit(1);
			}
			break;
		case 'k':
			mode = M_KVP_ARGV;
			break; 
		case 'K':
			mode = M_K_ARGS_V_STDIN;
			break;
		case 'Y':
			mode = M_K_ARG_V_STDIN;
			break;
		case 'p':
			mode = M_PROC;
			break;	
		}
	}
	if (mode == M_NOTHING){
		key = poptPeekArg(opt_context);
		if (key == NULL){
			mode = M_FILENAMES_ON_STDIN;
		}else{
			mode = M_FILENAMES_ARGV;
		}
	}
	return poptGetArgs(opt_context);
}

int main(int argc, const char* argv[]){
	int ii = 1;
	char line[128];
	const char** args = processArgs(argc, argv);
	char* pext;

	printHeader();

	insertSnippet();

	switch(mode){
	case M_FILENAMES_ON_STDIN:
		while (fgets(line, 128, stdin)){
			printRecord(ii++, chomp(line));			
		}
		break;
	case M_FILENAMES_ARGV:
		for (ii = 0; args[ii] != NULL; ++ii){
			printRecord(ii+1, args[ii]);
		}
		break;
	case M_KVP_ARGV:
		while (fgets(line, 128, stdin)){
                        printRecordPair(ii++, chomp(line));
               	}
		break;
	case M_K_ARGS_V_STDIN:
		printRecordKargsVstdin(ii, args);
		break;
	case M_K_ARG_V_STDIN:
		printRecordStdinValue(ii, key);
		break;
	case M_PROC:
		for (ii = 0; args[ii] != NULL; ++ii){
			if (ii&1){
				printRecordSubProc(ii/2, args[ii-1], args[ii]);
			}
		}	
		break;	
	}

	printFooter();

	if (outfile && (pext = strstr(outfile, ".tmp")) != 0){
		char* tfile = line;
		strcpy(tfile, outfile);
		*pext = '\0';
		rename(tfile, outfile);
	}
}
