#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/errno.h>

int main()
{

	int fp, i;

    	ssize_t erro = 0;
   	char *string=NULL;
	char c;
	
	string = (char*)malloc(sizeof(char)*200);
   
    	fp = open("/dev/seri", O_RDWR);

    	if(fp<0)
    	{
    		printf("!!! ERROR OPENING THE FILE !!!\n");
    		return -1;
    	}
	
	printf("Pressione Ctr-C quando desejar que termine a leitura\n");
	
	erro = read(fp, string, 200);
	
	printf("Ficheiro lido:\n%s\n", string);

	free(string);
	
	close(fp);
	
	return 0;

}

