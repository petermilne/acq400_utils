/* convert file system tree to xml */

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


FILE *fout;

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

void printRecordKargsVstdin(int ii, const char **args)
{	
	char *inbuf = malloc(4096);
	ii =0;

	while(fgets(inbuf, 4096, stdin) != 0 && args[ii] != NULL){

		fprintf(fout, "\t<acqData id=\"%d\" n=\"%s\">\n", 
						ii, args[ii]);
		fprintf(fout, "\t\t<v>%s\t\t</v>\n", chomp(inbuf));
		fprintf(fout, "\t</acqData>\n");
		++ii;
	}
	free(inbuf);
}

void printRecordStdinValue(int id, const char* name)
{
	char *inbuf = malloc(4096);
	char *l1 = inbuf;
	char *l2;
	
	while ((l2 = fgets(l1, 4096, stdin)) != 0){
		l1 = l2 + strlen(l2);
	}

	fprintf(fout, "\t<acqData id=\"%d\" n=\"%s\">\n", id, name);
	fprintf(fout, "\t\t<v>%s\t\t</v>\n", inbuf);
	fprintf(fout, "\t</acqData>\n");
	free(inbuf);
}
void printRecordSubProc(int id, const char* name, const char* proc)
{
	FILE *fp = popen(proc, "r");
        char *inbuf = malloc(4096);
        char *l1 = inbuf;
        char *l2;

	if (fp != 0){
	        while ((l2 = fgets(l1, 4096, fp)) != 0){
        	        l1 = l2 + strlen(l2);
	        }
		pclose(fp);
	}else{
		sprintf(inbuf, "command \"%s\" failed", proc);
	}

        fprintf(fout, "\t<acqData id=\"%d\" n=\"%s\">\n", id, name);
        fprintf(fout, "<v>%s</v>\n", inbuf);
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
		fprintf(fout, "\t\t<v>%s</v>\n", value);
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
