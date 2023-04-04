/* ------------------------------------------------------------------------- */
/* xml_lookup.cpp - generic caldef xml file parser                           */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 2005 Peter Milne, D-TACQ Solutions Ltd
 *                      <Peter dot Milne at D hyphen TACQ dot com>

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


#include "local.h"

#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "popt.h"
#include "tinyxml.h"

#define VERID "$RCSfile: xml_lookup.cpp,v $ $Revision: 1.7 $ B1017\n"
#define CONFIG_FILE "/etc/cal/caldef.xml"

#include "libgen.h"

static void usage(void)
{
	printf("%s\n", VERID);
	printf(
"usage:\n"
"XML file parser\n"
"get.caldef. caldef.field.spec\n"
"get.modelspec model.field.spec\n");
}

int acq200_debug;
int G_verbose;

#define MAXARGS 20

class Arglist {
	static const char** split(const char* fieldspec, int* nfields) {
		return 0;
	}

	const char* _argspec;
	char* args;
	char** argv;
	int argc;
public:
	Arglist(const char* argspec, const char* delim) :
		_argspec(argspec) {
		
		args = new char[strlen(argspec)+1];
		argc = 0;
		argv = new char* [MAXARGS];
		strcpy(args, argspec);
		
		argv[0] = args;
		argc = 1;

		for (char* cp = argv[0] = args; 
		     (cp = strpbrk(cp, delim)); ){
			assert(argc < MAXARGS);
		
			*cp = '\0';
			argv[argc++] = ++cp;
		}
		
	}

	const char* getArg(int iarg) const {
		return argv[iarg];
	}
	int getArgc(void) const {
		return argc;
	}
	const char* getArgspec(void) const {
		return _argspec;
	}
};

class Query {
	Arglist fields;
	TiXmlDocument* doc;
protected:
	TiXmlNode* getChildNode() const {
		TiXmlNode* child = 0;

		for (int iarg = 0; iarg != fields.getArgc(); ++iarg){
			const char* tag = fields.getArg(iarg);
			TiXmlNode* parent = child==0? doc: child;
			if (tag[0] == '@'){
				const char* attr = parent->ToElement()->Attribute(++tag);
				if (attr){
					printf("%s\n", attr);
				}else{
					fprintf(stderr, "ERROR, attr %s not found\n", tag);
				}
				return 0;
			}else{
				child = parent->FirstChild(fields.getArg(iarg));
			}

			if (!child){
				fprintf(stderr, 
					"ERROR: field \"%s\" not found\n",
				        fields.getArg(iarg));
				return 0;
			}
		}
		return child;
	}
public:
	Query(TiXmlDocument* _doc, const char* fieldspec) :
		fields(fieldspec, "./"), doc(_doc) {
		
	}
	void lookup() const {
		TiXmlNode* child = getChildNode();
		if (!child){
			exit(-1);
		}
		TiXmlText* container = child->FirstChild()->ToText();

		if (container){
			if (G_verbose){
				printf( "%s => %s\n",
					fields.getArgspec(), 
					container->Value());
			}else{
				printf("%s\n", container->Value());
			}
		}else{
			fprintf(stderr, "ERROR: tree lookup failed\n");
			exit(-1);
		}
	}
};

class SetValue : public Query {

public:
	SetValue(TiXmlDocument* _doc, const char* fieldspec) :
		Query(_doc, fieldspec)	
	{}

	int set(const char *value) {
		TiXmlNode* child = getChildNode();
		if (!child){
			return -1;
		}
		TiXmlText& new_text = *new TiXmlText(value);
		if (child->ReplaceChild(child->FirstChild(), new_text)){
			return strlen(value);
		}else{
			return -2;
		}
	}
};

int main(int argc, const char *argv[])
{
	const char *config_file = CONFIG_FILE;
	const char *verb = basename((char *)argv[0]);
	int set_value = 0;

        struct poptOption opt_table[] = {
		{ "write",      'w', POPT_ARG_NONE,   0,   'w' },
		{ "verbose",    'v', POPT_ARG_INT,    &G_verbose, 0 },
		{ "debug",      'd', POPT_ARG_INT,    &acq200_debug, 0 },
		{ "config",     'c', POPT_ARG_STRING, &config_file,  0 },
		{ "help2",      '2', POPT_ARG_NONE,   0,             '2'},
		POPT_AUTOHELP
		{}
	};
	poptContext context = poptGetContext(
                argv[0], argc, argv, opt_table, 0 );

	int rc;
	while ((rc = poptGetNextOpt(context)) > 0){
		switch(rc){
		case 'w':
			set_value = 1;
			break;
		case '2':
			usage();
			exit(0);
		}
	}

	const char** args = poptGetArgs(context);
	int nargs;

	for (nargs = 0; args && args[nargs]; nargs++){
		;
	}

	if (G_verbose) printf("new TiXmlDocument(%s)\n", config_file);

	if (G_verbose > 2 )return 0;

	TiXmlDocument* doc = new TiXmlDocument(config_file);

	assert(doc);

	if (G_verbose) printf("doc %p config_file %s\n",
			      doc, config_file);

	if (!doc->LoadFile()){
		fprintf(stderr, "ERROR: file %s not valid\n", config_file);
		exit(-1);
	}
	/* handle modifed tree structure - later versions have
         * XML-conformant single top level root 
	 */
	TiXmlElement* root = doc->RootElement();
	assert(root);

	const char* ACQROOT;

	if (strcmp("ACQ", root->Value()) == 0){
		ACQROOT = "ACQ/";
	}else{
		ACQROOT = "";
		err("WARNING: deprecated caldef file please modify to include"
		    " single root \"ACQ\"\n");
	}

	char basefield[80] = {};


	if (strcmp(verb, "get.caldef") == 0){
		strcpy(basefield, ACQROOT);
		strcat(basefield, "AcqCalibration/");
	}else if (strcmp(verb, "get.modelspec") == 0){
		strcpy(basefield, ACQROOT);
		strcat(basefield, "ModelSpec/");
	}
		
	char field[128];	

	if (set_value){
		if (nargs == 2){
			strcpy(field, basefield);
			SetValue setValue(doc, strcat(field, args[0]));

			int rc = setValue.set(args[1]);
			if (rc < 0){
				err("Failed to set value");
				return rc;
			}else{
				doc->SaveFile(config_file);
				return 0;
			}
		}else{
			err("write: key value");
			return -1;
		}
	}
	for(int iarg = 0; iarg != nargs; ++iarg){
		strcpy(field, basefield);		
		Query query(doc, strcat(field, args[iarg]));

		query.lookup();
	}

	return 0;
}

