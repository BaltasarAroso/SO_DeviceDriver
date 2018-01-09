#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>

int main()
{
	char *str1;
	char str2[200] = "\nString que esperou pela ativacao do poll para poder ser escrita\n";

	int timeout = -1; //poll() blocks until a requested event occurs or until the call is interrupted (timeout in ms)
	int res=0, i, size=0;

	struct pollfd select[1];

	select->fd = open("/dev/seri", O_RDWR | O_NONBLOCK);
	
	if (select->fd == -1)
	{
		perror("ERROR OPENING - ");
		return -1;
	}

	printf("Size of the bytes to read in str1: ");
	scanf ("%d", &size);

	str1 = (char*)malloc(sizeof(char)*size);
	
	for (i=0; i<size; i++)
	{
		str1[i]='a';
	}

	select->events = POLLOUT; //Normal data (priority band equals 0) may be written without blocking

	//Escreve a primeira string//
	if (write(select->fd, str1, size) < 0 ) perror("ERROR WRITING - ");

poll:
	res = poll(select, 1, timeout);
	if (res > 0) printf("\nSuccess calling poll\n");
	if (res == 0) printf("The call timed out and no file descriptors have been selected.\n");

	//ERRORS FROM POLL//
	if (res==EAGAIN)
	{
		printf("The allocation of internal data structures failed but a subsequent request may succeed.\n");
		goto poll;
	}
	if (res==EINTR)
	{
		printf("A signal was caught during poll().\n");
		goto poll;
	}
	if (res==EINVAL)
	{
		printf("The nfds argument is greater than {OPEN_MAX}, or one of the fd members refers to a STREAM or multiplexer that is linked (directly or indirectly) downstream from a multiplexer.\n");
		goto poll;
	}
	

	//Confirma a recepcao dos dados e escreve caso já seja possível//
	if (select->events & select->revents) printf("READY TO WRITE!!!\n\n");
	else perror("ERROR POLLING EVENTS - ");
	
	//Escreve a segunda string//
	if (write(select->fd, str2, strlen(str2)) < 0 ) perror("ERROR WRITING - ");
	
	close(select->fd);

	return 0;
}

