#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int main()
{

	int fp1, fp2;
	
	fp1 = open("/dev/seri", O_RDWR);
	if (fp1<0)
	{
		perror ("ERROR OPENING ");
		return -1;
	}
	
	fp2 = open("/dev/seri", O_RDWR);
	if (fp2<0)
	{
		perror ("ERROR OPENING ");
		return -1;
	}
	
	close(fp1);
	close(fp2);
	
	return 0;

}

