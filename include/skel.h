#ifndef __SKEL_H__
#define __SKEL_H__

/* UNIX version */

#define INIT()			( program_name = \
						strrchr( argv[ 0 ], '/' ) ) ? \
						program_name++ : \
						( program_name = argv[ 0 ] )
#define EXIT(s)			exit( s )
#define CLOSE(s)		if ( close( s ) ) error( 1, errno, \
						"close failed" )
#define set_errno(e)	errno = ( e )
#define isvalidsock(s)	( ( s ) >= 0 )

typedef int SOCKET;

extern int G_verbose;
extern int G_use_seek; /* for testing: use seek on device */

#define SKT_BUFLEN (4096*16)
#endif  /* __SKEL_H__ */
