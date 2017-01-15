/* launch program as a daemon */
#include <unistd.h>

int main(int argc, char* argv[])
{
	if (argc > 1){
		daemon(0, 0);
		execv(argv[1], argv+1);
		return -1;		/* doesn't happen */
	}else{
		return 1;
	}
}



