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

	int fp, erro, i, size=0;
	char *string1;
	char *string2;
	
	fp = open("/dev/seri", O_RDWR | O_NONBLOCK);
	if (fp<0)
	{
		perror ("ERROR OPENING ");
		return -1;
	}

	printf("Size of the bytes to write: ");
	scanf ("%d", &size);

	string1 = (char*)malloc(sizeof(char)*3000);
	
	for (i=0; i<3000; i++)
	{
		string1[i]='a';
	}

	printf("\n");

	//Como a primeira chamada é menor que a capacidade do kfifo (3000 < SIZE_KFIFO=8192) escreve
	erro=write(fp, string1, strlen(string1));
	if (erro<0) perror ("ERRO WRITING: ");

	printf ("\n\n");

	string2 = (char*)malloc(sizeof(char)*size);
	
	for (i=0; i<size; i++)
	{
		string2[i]='a';
	}

	//Caso a soma do conteúdo do Kfifo com a 2º chamada seja maior que a capacidade do kfifo (kfifo_len() + size > SIZE_KFIFO=8192) dá erro
	erro=write(fp, string2, strlen(string2));
	if (erro<0) perror ("ERRO READING: ");
	
	close(fp);
	
	return 0;

}
