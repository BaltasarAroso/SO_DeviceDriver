/* SO - Trabalho Pratico - 16/17 */
/* Baltasar Aroso & Andre Duarte */

#define SERP_DEVS 	4
#define START_ADDRESS 	0x3f8
#define SIZE 		0x08	//de 0x3f8 a 0x3ff

#define MAJORE 		0
#define MINORE		0

#include "serial_reg.h"
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/aio.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/delay.h>

MODULE_LICENSE("Dual BSD/GPL");




//IMPLEMENTACAO DAS FUNCOES//
int serp_open(struct inode *inode, struct file *filp);
ssize_t serp_read(struct file *filep, char __user *buff, size_t count, loff_t *offp);
ssize_t serp_write(struct file *filep, const char __user *buff, size_t count, loff_t *offp);
int serp_release(struct inode *inode, struct file *filp);
static int serp_init(void);
static void serp_exit(void);




//INICIALIZACOES GLOBAIS//

dev_t dev;

typedef struct {
	struct cdev c_dev;
	int cnt;
} serp_dev;

serp_dev *serp_d = NULL;

struct file_operations serp_fops = {
	.owner = THIS_MODULE,
	.open =	     serp_open,
	.release =   serp_release,
	.write =     serp_write,
	.read =	     serp_read,
	
};

unsigned char lcr = 0;




//INICIO DAS FUNCOES//

/*read*/

ssize_t serp_read(struct file *filep, char __user *buff, size_t count, loff_t *offp){
	
	int i, cont=0;
	char *string;

	string = (char*) kmalloc(count, GFP_KERNEL);

	for (i=0; i<count; i++)
	{
		while ((inb(START_ADDRESS+UART_LSR) & UART_LSR_DR) == 0)
		{
			msleep_interruptible(10);
			cont++;
			if (cont==200) 	//termina 2s depois de não obter dados
			{
				if (!i) printk(KERN_ALERT "!!! TOO LATE !!!\n");
				else printk(KERN_ALERT "!!! READ FILE !!!\n");
				goto send;
			}
		}
		//ler um caracter via UART pelo receiver caso esteja com dados
		string[i]=inb(START_ADDRESS+UART_RX);

		cont=0;
	}
	
	send:

	if ((inb(START_ADDRESS+UART_LSR) & UART_LSR_DR & UART_LSR_OE))
	{
		printk(KERN_ALERT "!!! ERROR OVERRUN !!!\n");
		kfree(string);
		return -EIO;
	}

	
	if (copy_to_user(buff, string, count))
	{
		printk(KERN_ALERT "!!! ERROR COPY_TO_USER !!!\n");
		kfree(string);
		return -EAGAIN;
	}

	kfree(string);
	
	return i;
}


/*write*/

ssize_t serp_write(struct file *filep, const char __user *buff, size_t count, loff_t *offp){

	int i, cont=0;
	char *string;

	string = (char*) kmalloc(count, GFP_KERNEL);

	if (copy_from_user(string, buff, count))
	{
		printk(KERN_ALERT "!!! ERROR COPY_FROM_USER !!!\n");
		kfree(string);
		return -EAGAIN;
	}

	for (i=0; i<count; i++)
	{
		while ((inb(START_ADDRESS+UART_LSR) & UART_LSR_THRE) == 0)
		{
			msleep_interruptible(10);
			cont++;
			if (cont==200)
			{
				if (!i) printk(KERN_ALERT "!!! TOO LATE !!!\n");
				else printk(KERN_ALERT "!!! UPLOADED FILE !!!\n");
				goto send;
			}
		}
		//escrever um caracter via UART pelo transmitter caso ele esteja vazio
		outb(string[i], START_ADDRESS+UART_TX);

		cont=0;
	}

	send:

	if ((inb(START_ADDRESS+UART_LSR) & UART_LSR_DR & UART_LSR_OE))
	{
		printk(KERN_ALERT "!!! ERROR OVERRUN !!!\n");
		kfree(string);
		return -EIO;
	}

	kfree(string);
	
	return i;
}


/*open*/

int serp_open(struct inode *inode, struct file *filp)
{
	filp->private_data = container_of(inode->i_cdev,serp_dev, c_dev);
	
	printk(KERN_ALERT "OPENED\n");
	
	nonseekable_open(inode, filp);

	return 0;
}

/*release*/

int serp_release(struct inode *inode, struct file *filp)
{
	printk(KERN_NOTICE "RELEASED\n");

	return 0;
}




/*****************************************************************INIT****************************************************************************/

static int serp_init(void)
{
	int major = MAJORE;
	int minor = MINORE;

	//GET MAJOR//
	if (major)
	{
		dev=MKDEV(MAJORE, MINORE);
		if (register_chrdev_region(dev, SERP_DEVS, "serp"))
		{
			printk(KERN_ALERT "!!! MAJOR ERROR !!!\n");
			return -1;
		}
	}
	else
	{
		if (alloc_chrdev_region(&dev, minor, SERP_DEVS, "serp"))
		{
			printk(KERN_ALERT "!!! MAJOR ERROR !!!\n");
			return -1;
		}
		major = MAJOR(dev);
	}
	//*********//

	
	//INICIALIZACAO DA STRUCT serp_d//

	serp_d=kmalloc(sizeof(serp_dev), GFP_KERNEL);
	cdev_init(&serp_d->c_dev, &serp_fops);
	serp_d->c_dev.ops = &serp_fops;
	serp_d->c_dev.owner = THIS_MODULE;

	//*******************************//

	
	if(cdev_add(&serp_d->c_dev, dev, SERP_DEVS) < 0)
	{
		printk(KERN_ALERT "!!! ERROR CDEV !!!\n");
		return -1;
	}

	if (!request_region(START_ADDRESS, SIZE, "serp"))
	{
		printk (KERN_ALERT "!!! ERROR - PORTS OCCUPIED !!!\n");
		return -1;
	}


	//****Endereços****//

	//lcr ports//
	lcr = UART_LCR_WLEN8 | UART_LCR_EPAR | UART_LCR_PARITY | UART_LCR_STOP | UART_LCR_DLAB;
	outb(lcr, START_ADDRESS+UART_LCR); //definir os bits anteriores

	/*//lcr bitrate//
	lcr = UART_DIV_1200;
	outb(lcr, START_ADDRESS); //colocar os 1200bps
	lcr &= ~UART_LCR_DLAB;
	outb(lcr, START_ADDRESS+UART_LCR); //voltar a meter DLAB a 0, colocando no modo receiver e transmitter*/

	//bitrate//
	outb(UART_DIV_1200, START_ADDRESS+UART_DLL); //colocar os 1200bps
	outb(0x00, START_ADDRESS+UART_DLM);

	lcr &= ~UART_LCR_DLAB;
	outb(lcr, START_ADDRESS+UART_LCR); //voltar a meter DLAB a 0, colocando no modo receiver e transmitter

	//disabling interrupt//
	if (inb(START_ADDRESS+UART_IER)) outb(0x00, START_ADDRESS+UART_IER); //colocar o interrupt enable a 0

	//*****************//



	printk(KERN_ALERT "Major number = %d\n", major);
	
	return 0;
}

/***********************************************************************EXIT********************************************************************/

static void serp_exit(void)
{
	cdev_del(&serp_d->c_dev);
	unregister_chrdev_region(dev, SERP_DEVS);

	release_region(START_ADDRESS,SIZE);

	printk(KERN_NOTICE "JOB DONE - ALL FREE\n");
}

module_init(serp_init);
module_exit(serp_exit);
