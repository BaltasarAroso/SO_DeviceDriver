#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "serial_reg.h"

#define SERI_IOC_MAGIC  	'k'

#define SERI_IOC_LCR_INIT	_IOR(SERI_IOC_MAGIC, 1, int)
#define SERI_IOC_LCR_CONFIG	_IOW(SERI_IOC_MAGIC, 2, int)

#define SERI_IOC_BR_INIT	_IOR(SERI_IOC_MAGIC, 3, int)
#define SERI_IOC_BR_CONFIG	_IOW(SERI_IOC_MAGIC, 4, int)

int main()
{
	int fp, x=0;
	unsigned char lcr = 0;
	unsigned char br = 0;

	fp = open("/dev/seri", O_RDWR);

	if (fp<0)
	{
		perror ("ERROR OPENING ");
		return -1;
	}

Wordlength:
	printf ("Choose the Wordlength: ");
	scanf ("%d", &x);
	switch (x)
	{
		case (5): lcr |= UART_LCR_WLEN5; break;
		case (6): lcr |= UART_LCR_WLEN6; break;
		case (7): lcr |= UART_LCR_WLEN7; break;
		case (8): lcr |= UART_LCR_WLEN8; break;

		default: printf ("\nERROR - TRY AGAIN\n\n"); goto Wordlength; break;
	}


Parity:
	printf ("Choose the Parity (1 - even | 2 - odd | 3 - none): ");
	scanf ("%d", &x);
	switch (x)
	{
		case 1: lcr |= UART_LCR_PARITY|UART_LCR_EPAR; break;
		case 2: lcr |= UART_LCR_PARITY; break;
		case 3: break;

		default: printf ("\nERROR - TRY AGAIN\n\n"); goto Parity; break;
	}

Stop_Bits:
	printf ("Choose the number of Stop Bits: ");
	scanf ("%d", &x);
	switch (x)
	{
		case 1: break;
		case 2: lcr |= UART_LCR_STOP; break;

		default: printf ("\nERROR - TRY AGAIN\n\n"); goto Stop_Bits; break;
	}

Bitrate:
	printf ("Choose the Bitrate (1200 | 9600): ");
	scanf ("%d", &x);
	switch (x)
	{
		case 1200: br = UART_DIV_1200; break;
		case 9600: br = UART_DIV_9600; break;

		default: printf ("\nERROR - TRY AGAIN\n\n"); goto Bitrate; break;
	}

	//Envio dos dados pretendidos//
	ioctl(fp, SERI_IOC_LCR_CONFIG, &lcr);
	ioctl(fp, SERI_IOC_BR_CONFIG, &br);

	//Recepcao do que foi implementado//
	lcr=ioctl(fp,SERI_IOC_LCR_INIT);
	br=ioctl(fp,SERI_IOC_BR_INIT);

	printf ("\nRESULTS OF THE SETUP:\n\n");	

	if ((lcr & UART_LCR_WLEN8)==UART_LCR_WLEN5) printf("Wordlength = 5\n");
	else {
		if ((lcr & UART_LCR_WLEN8)==UART_LCR_WLEN6) printf("Wordlength = 6\n");
		else {
			if ((lcr & UART_LCR_WLEN8)==UART_LCR_WLEN7) printf("Wordlength = 7\n");
			else {
				if ((lcr & UART_LCR_WLEN8)==UART_LCR_WLEN8) printf("Wordlength = 8\n");
			}
		}
	}

	if ((lcr & (UART_LCR_PARITY|UART_LCR_EPAR))==(UART_LCR_PARITY|UART_LCR_EPAR)) printf("Parity Even\n");
	else {
		if ((lcr & (UART_LCR_PARITY|UART_LCR_EPAR))==UART_LCR_PARITY) printf("Parity Odd\n");
		else printf("No parity\n");
	}

	if (lcr & UART_LCR_STOP) printf("Stop Bits = 2\n");
	else printf("Stop Bits = 1\n");

	if (br & UART_DIV_1200) printf("Bitrate = 1200\n");
	else if (br & UART_DIV_9600) printf("Bitrate = 9600\n");

	printf ("\n--- END ---\n\n");

	close(fp);

	return 0;
}
