/*
 * Xilinx bitstream loader / inspector
 *
 *
 * Copyright (C) 2013 D-TACQ Solutions
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

#define usage "\
     \nUsage: xiloader if=<file> [mode=load,quiet,info]\
     \n\nLoad a bitstream into the Zynq FPGA fabric\n\
     \n   if=FILE        Read from FILE\
     \n   of=FILE        Write to FILE\
     \n   mode=info      Don't load file, just print header info\
     \n                  overrides quiet and load\
     \n   mode=quiet     Don't print header info when loading\
     \n   mode=load      load bitstream into FPGA fabric\
     \n   mode=loadraw   load bitstream bytewise\
     \n"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

/* valid on Zynq */
#define FPGA_PORT "/dev/xdevcfg"


#ifdef CLEVER_RICHARD
#include <arm_neon.h>


/* Do it with ARM_ASM */
#define BYTESWAP(val)					\
     ({ __asm__ __volatile__ (				\
	       "eor     r3, %1, %1, ror #16\n\t"	\
	       "bic     r3, r3, #0x00FF0000\n\t"	\
	       "mov     %0, %1, ror #8\n\t"		\
	       "eor     %0, %0, r3, lsr #8"		\
	       : "=r" (val)				\
	       : "0"(val)				\
	       : "r3", "cc"				\
	       ); val; })


/* Do it fast with THUMB2 */
#define BYTESWAP(val)				\
     ({ __asm__ __volatile__ (			\
	       "rev     %0, %0"			\
	       : "=r" (val)			\
	       : "0" (val)			\
	       ); val; })

#else

typedef unsigned u32;

static inline u32 bs(u32 num) {
     u32 swapped = num>>24 & 0x000000ff | // move byte 3 to byte 0
	  num<<8  & 0x00ff0000 | // move byte 1 to byte 2
	  num>>8  & 0x0000ff00 | // move byte 2 to byte 1
	  num<<24 & 0xff000000;  // byte 0 to byte 3
     return swapped;
}

#define BYTESWAP(val)	bs(val)

#endif

enum {
     FLAG_INFO     = 1 << 0,
     FLAG_QUIET    = 1 << 1,
     FLAG_LOAD     = 1 << 2,
     FLAG_LOAD_RAW = 1 << 3,
};

enum {
     OP_if,
     OP_of,
     OP_mode,
     OP_mode_info,
     OP_mode_quiet,
     OP_mode_load,
};

static const uint8_t ident_string[] = {
     0x00,0x09,0x0f,0xf0,0x0f,0xf0,0x0f,
     0xf0,0x0f,0xf0,0x00,0x00,0x01,0x61,
     0x00
};
static const char *keywords[] = {
     "if", "of", "mode", 0
};
static const char *mode_words[] = {
     "info", "quiet", "load", "loadraw", 0
};


int index_in_strings(const char *strings[], const char *key)
{
     int idx = 0;

     for (; strings[idx]; ++idx) {
	  if (strcmp(strings[idx], key) == 0) {
	       return idx;
	  }
     }
     return -1;
}

/* Command Line Argument Parsing*/
struct {
     int flags;
     const char *infile;
     const char *outfile;
} Z;

#define flags   (Z.flags   )
#define infile  (Z.infile  )
#define outfile (Z.outfile )


void show_usage(void)
{
     printf(usage);
     exit(1);
}

int is_file(const char* fname)
{
     struct stat sb;

     if (stat(fname, &sb) == -1){
	  perror(infile);
	  return 0;
     }else{
	  return 1;
     }
}
void cli(int argc, char* argv[] )
{
     int ii;

     outfile = FPGA_PORT;

     switch(argc){
     case 1:
	  show_usage();
     case 2:
	  if (is_file(argv[1])){
	       infile = argv[1];
	       flags |= FLAG_INFO;
	       return;	
	  }
     default:
	  ;
     }	

     for (ii = 1; ii < argc; ++ii){
	  int what;
	  char *val;
	  char *arg = argv[ii];
	  what = index_in_strings(keywords, arg);
	  if (arg[0] == '-' && arg[1] == '-' && arg[2] == '\0') continue;

	  val = strchr(arg, '=');
	  if (val == NULL) show_usage();
	  *val = '\0';
	  what = index_in_strings(keywords, arg);
	  if (what < 0) show_usage();
	  /* *val = '='; - to preserve ps listing? */
	  val++;


	  switch(what){
	  case OP_mode:
	       while (1) {
		    /* find ',', replace them with NUL so we can use val for
		     * index_in_strings() without copying.
		     * We rely on val being non-null, else strchr would fault.
		     */
		    arg = strchr(val, ',');
		    if (arg) *arg = '\0';
		    what = index_in_strings(mode_words, val);

		    if (what < 0){
			 printf(usage);
			 exit(1);
		    }

		    flags |= (1 << what);
		    if (!arg)	break;
		    val = arg + 1; /* skip this keyword and ',' */
	       }
	       break;
	  case OP_if:
	       infile = val;
	       break;
	  case OP_of:
	       outfile = val;
	       break;
	  }
     }
}




/** @@todo: what if if=- (stdin: no size ... stream to variable size bucket */
int getdata(void** buffer)
{
     void* buf;
     int len;
     int nread;
     struct stat sb;
     FILE *ifp;

     if (infile == 0){
	  printf("ERROR:infile not specified\n");
	  exit(1);
     }
     if (stat(infile, &sb) == -1){
	  perror(infile);
	  exit(1);
     }
     len = sb.st_size;

     if (len < 1024){
	  printf("ERROR: length too short\n");
	  exit(1);
     }

     ifp = fopen(infile, "r");
     if (!ifp){
	  perror(infile);
	  exit(1);
     }
     buf = malloc(len);
     nread = fread(buf, 1, len, ifp);
     if (nread != len){
	  printf("ERROR: read %d != len %d\n", nread, len);
	  exit(1);
     }
     *buffer = buf;
     return len;
}

int triplet(uint8_t * cursor, uint8_t t1, uint8_t t2, uint8_t t3)
{
     return cursor[0]==t1 && cursor[1]==t2 && cursor[2]==t3;
}


	
void print_header(uint8_t *header, int eoh_location)
{
     int ii = sizeof(ident_string);

     printf("Xilinx Bitstream header.\n");
     printf("%-25s : %x\n", "built with tool version", header[ii]);
     printf("%-25s : ", "generated from filename");
/*                      123456789012345678901234 */
     while(++ii < eoh_location){
	  if (header[ii] == 0x3B){
	       break;
	  }else{
	       printf("%c", header[ii]);
	  }
     }
     printf("\n");

     for (; ii < eoh_location; ii++){
	  if (triplet(header+ii, 0x00, 0x62, 0x00)){
	       ii += 4;
	       break;
	  }
     }

     printf("%-25s : ", "part");
     for (; ii < eoh_location; ii++){
	  if (triplet(header+ii, 0x00, 0x63, 0x00)){
	       ii += 3;
	       printf("\n");
	       printf("%-25s : ", "date");
	  }else if (triplet(header+ii, 0x00, 0x64, 0x00)){
	       ii += 3;
	       printf("\n");
	       printf("%-25s : ", "time");
	  }else if (triplet(header+ii, 0x00, 0x65, 0x00)){
	       break;
	  }else{
	       int cx = header[ii];
	       if (isprint(cx)){
		    printf("%c", cx);
	       }
	  }
     }
     printf("\n");
     printf("%-25s : %d\n", "bitstream data starts at", eoh_location);
}

void load_bitstream(uint32_t* bitstream, int nbytes)
/* byte swap it and dump it into the FPGA */
{
     int bit_words = nbytes/sizeof(uint32_t);
     FILE *ofp = fopen (outfile, "w");
     if (!ofp){
	  perror(outfile);
	  exit(1);
     }

     if (!(flags & FLAG_LOAD_RAW)){
	  int ii;
	  for (ii = 0; ii <= bit_words; ii++) {
	       bitstream[ii] = BYTESWAP(bitstream[ii]);
	  }
     }

     fwrite(bitstream, 1, nbytes, ofp);
     fclose(ofp);
}

main(int argc, char* argv[])
{
     int ii;
     int eoh_location = 0;
     void* read_buffer;
     int in_size;
     uint32_t stream_size;
     uint8_t *header;

     cli(argc, argv);

     in_size = getdata(&read_buffer);
     header = (uint8_t *)read_buffer;

     for (ii = 0; ii < sizeof(ident_string)-1; ii++) {
	  if  (header[ii] == ident_string[ii]){
	       continue;
	  }else{
	       printf("Could not find Xilinx bitstream header.\n");
	       exit(1);
	  }
     }

     for (ii = 0; ii < 256; ii++) {
	  if ( triplet(header+ii, 0x00, 0x65, 0x00)){
	       stream_size = (header[ii+3] << 16)|
		    (header[ii+4] <<  8)|
		    (header[ii+5]);
	       eoh_location = in_size-stream_size;
	       break;
	  }
     }

     if (eoh_location == 0){	     
	  printf("ERROR:eoh_location NOT FOUND\n");
	  exit(1);
     }

     if ( (flags & FLAG_INFO) || (~flags & FLAG_QUIET) ){	     
	  print_header(header, eoh_location);
	  if (flags & FLAG_INFO){
	       exit(0);
	  }
     }

     if ((flags & FLAG_LOAD) || (flags & FLAG_LOAD_RAW)  ) {
	  load_bitstream((uint32_t*)(header+eoh_location), 
			 in_size - eoh_location);
     }
     exit(0);
}
