/* SO - Trabalho Pratico - 16/17 */
/* Baltasar Aroso & Andre Duarte */

#define SERI_DEVS 		4
#define START_ADDRESS 		0x3f8
#define SIZE 			0x08	//de 0x3f8 a 0x3ff
#define SIZE_KFIFO 		8192 	//numero de bytes lidos pelo cat duas vezes

#define IRQ			4	//defined interrupt request line
#define TIME			500	//500 jiffies = 5s porque vbox tem um Hz=100 -> 1jiffy=1/Hz=10ms

#define MAJORE 			0
#define MINORE			0

#define SERI_IOC_MAGIC  	'k'	//linux/hsi/hsi_char.h  HSI character device
#define SERI_IOC_MAX		4

#define SERI_IOC_LCR_INIT	_IOR(SERI_IOC_MAGIC, 1, int)
#define SERI_IOC_LCR_CONFIG	_IOW(SERI_IOC_MAGIC, 2, int)	
#define SERI_IOC_BR_INIT	_IOR(SERI_IOC_MAGIC, 3, int)
#define SERI_IOC_BR_CONFIG	_IOW(SERI_IOC_MAGIC, 4, int)

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

#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/poll.h>

MODULE_LICENSE("Dual BSD/GPL");




//IMPLEMENTACAO DAS FUNCOES//
irqreturn_t int_handler(int irq, void *dev_id);

int ioctl (struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);
static unsigned int poll(struct file *filp, poll_table *wait);

int seri_open(struct inode *inode, struct file *filp);

ssize_t seri_read(struct file *filp, char __user *buff, size_t count, loff_t *offp);
ssize_t seri_write(struct file *filp, const char __user *buff, size_t count, loff_t *offp);

int seri_release(struct inode *inode, struct file *filp);

static int seri_init(void);
static void seri_exit(void);




//INICIALIZACOES GLOBAIS//

dev_t dev;

typedef struct {
	struct cdev c_dev;
	int cnt;
	
	spinlock_t rxlock;    // lock receiver
	spinlock_t txlock;    // lock transmiter
	spinlock_t user_lock; // lock users	
	struct kfifo *rxfifo; // receiver fifo
	struct kfifo *txfifo; // transmiter fifo

	wait_queue_head_t rx;	// for IH synchron - receive
    	wait_queue_head_t tx;	// for IH synchron - transmit

} seri_dev;

seri_dev *seri_d = NULL;

int users = 0;

struct file_operations seri_fops = {
	.owner = THIS_MODULE,
	.open =	     seri_open,
	.release =   seri_release,
	.write =     seri_write,
	.read =	     seri_read,
	.ioctl =     ioctl,
	.poll =      poll,
	
};




//INICIO DAS FUNCOES//

/*int_handler*/

irqreturn_t int_handler(int irq, void *dev_id)
{
	unsigned char buffer = 0x00;
	unsigned char iir = 0;
	iir = inb(START_ADDRESS+UART_IIR); //Endereço base para verificação dos interrupts

	if ((iir & UART_IIR_RDI)) //enable receiving
	{
		buffer=inb(START_ADDRESS + UART_RX);
		kfifo_put(seri_d->rxfifo, &buffer, 1);
	
		wake_up_interruptible(&(seri_d->rx));
	}

	else if ((iir & UART_IIR_THRI) && (kfifo_len(seri_d->txfifo))) //enable transmitting && something to receive
	{
		if (kfifo_get(seri_d->txfifo, &buffer, 1))
		{
			outb(buffer, START_ADDRESS + UART_TX);

			wake_up_interruptible(&(seri_d->tx));
		}	
	}

	return IRQ_HANDLED;
}

/*ioctl*/

int ioctl (struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{

	int ok=0, retval=0;
	unsigned char lcr, br;

	//wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok( )
	if(_IOC_TYPE(cmd) != SERI_IOC_MAGIC) 
		return -ENOTTY;
	if(_IOC_NR(cmd) > SERI_IOC_MAX) 
		return -ENOTTY;


	//access_ok retorna 1 -> sucesso e 0 -> falhanço
	if(_IOC_DIR(cmd) & _IOC_READ)
		ok=access_ok(VERIFY_WRITE, (void __user*) arg, _IOC_SIZE(cmd));

	else if(_IOC_DIR(cmd) & _IOC_WRITE)
		ok=access_ok(VERIFY_READ, (void __user*) arg, _IOC_SIZE(cmd));
	
	if (!ok) return -EFAULT;


	switch (cmd)
	{

		case (SERI_IOC_LCR_INIT):
			lcr = inb(START_ADDRESS + UART_LCR);

			retval=lcr; //coloca o valor de lcr em retval
			break;

		case (SERI_IOC_LCR_CONFIG):
			if (!(__get_user(lcr, (unsigned long __user*) arg)))
			{
				outb(lcr, START_ADDRESS + UART_LCR);
				break;
			}
			else return -EFAULT; //retorna 0->sucesso e -EFAULT->falhanco
			break;

		case (SERI_IOC_BR_INIT):
			lcr = inb(START_ADDRESS + UART_LCR);

			lcr |= UART_LCR_DLAB; //ativa o DLAB para ler o bitrate
			outb(lcr, START_ADDRESS + UART_LCR);

			br = inb(START_ADDRESS+UART_DLL); //lê bitrate

			lcr &= ~UART_LCR_DLAB; //desativa o DLAB
			outb(lcr, START_ADDRESS + UART_LCR);

			retval = br; //coloca o valor de br em retval
			break;

		case(SERI_IOC_BR_CONFIG):
			if(!(__get_user(br, (unsigned long __user*) arg)))
			{
				lcr = inb(START_ADDRESS + UART_LCR);

				lcr |= UART_LCR_DLAB; //ativa o DLAB para mudar o bitrate
				outb(lcr, START_ADDRESS + UART_LCR);
				
				//ambos <256 logo so mudamos o DLL, DLM->0
				outb(br, START_ADDRESS+UART_DLL);
				outb(0x00,START_ADDRESS+UART_DLM);

				lcr &= ~UART_LCR_DLAB; //desativa o DLAB
				outb(lcr, START_ADDRESS + UART_LCR);
	 		}
			else return -EFAULT; //retorna 0->sucesso e -EFAULT->falhanco
	 		break;
	}
	return retval;
}

/*select*/

static unsigned int poll(struct file *filp, poll_table *wait)
{
	seri_dev *poll_dev = filp->private_data;
	unsigned int mask = 0;
	 
	poll_wait(filp, &poll_dev->rx, wait);
	poll_wait(filp, &poll_dev->tx, wait);
    
	if (kfifo_len(poll_dev->rxfifo) > 0)
		mask |= POLLIN | POLLRDNORM;	/*readable*/

	if (kfifo_len(poll_dev->rxfifo) < SIZE_KFIFO)
		mask |= POLLOUT | POLLWRNORM;	/*writable*/

	return mask;
}


/*read*/

ssize_t seri_read(struct file *filp, char __user *buff, size_t count, loff_t *offp){
	
	int len=0, res=1;
	char *string;

	string = (char*) kmalloc(count, GFP_KERNEL);

	//o tamanho do kfifo vai aumentar ate count - leitura completa

	while((len=kfifo_len(seri_d->rxfifo))<count)
	{
		//lab5 - 3.3 - O_NONBLOCK//
		if(filp->f_flags & O_NONBLOCK)
		{
			if(!len)
			{
				kfree(string);
				printk(KERN_ALERT "!!! EXIT FROM READING -> NONBLOCK!!!\n");
				return -EAGAIN;
			}
		}
		
		//espera que esteja apto a receber para poder ler o caracter via UART
		res=wait_event_interruptible_timeout(seri_d->rx, kfifo_len(seri_d->rxfifo)>len, TIME); //return=0 se fim da leitura

		if(!res)
		{
			if (!len) printk(KERN_ALERT "!!! DONE !!!\n");
			break;
		}

		//lab5 - 3.5 - Ctrl-C//
		if(res==-ERESTARTSYS)
		{
			printk(KERN_ALERT "!!! EXIT FROM READING -> Ctrl C !!!\n");
			return -ERESTARTSYS;
		}
	}
	
	len=kfifo_len(seri_d->rxfifo);

	if (kfifo_get(seri_d->rxfifo, string, len) != len)
	{
		printk(KERN_ALERT "!!! ERROR KFIFO_GET READING !!!\n");
		kfree(string);
		return -1;
	}

	if ((inb(START_ADDRESS+UART_LSR) & UART_LSR_DR & UART_LSR_OE))
	{
		printk(KERN_ALERT "!!! ERROR OVERRUN !!!\n");
		kfree(string);
		return -EIO;
	}

	if (copy_to_user(buff, string, len))
	{
		printk(KERN_ALERT "!!! ERROR COPY_TO_USER !!!\n");
		kfree(string);
		return -EAGAIN;
	}

	kfree(string);

	return len;
}


/*write*/

ssize_t seri_write(struct file *filp, const char __user *buff, size_t count, loff_t *offp){

    long int chars_sent = 0, chars_left = count;

	int res = 0, pos = 0;
	char *string;
	char um;

	string = (char*) kmalloc(count+1, GFP_KERNEL);

	if (copy_from_user(string, buff, strlen(buff))) {
		printk(KERN_ALERT "!!! ERROR COPY_FROM_USER !!!\n");
		kfree(string);
		return -EAGAIN;
	}
	string[count] = '\0';

	while(chars_left > 0) {
		//lab5 - 3.3 - O_NONBLOCK//
		if(filp->f_flags & O_NONBLOCK) {
			if(kfifo_len(seri_d->txfifo) == SIZE_KFIFO) {
				kfree(string);
				printk(KERN_ALERT "!!! EXIT FROM WRITING -> NONBLOCK!!!\n");
				return -EAGAIN;
			}
		}
		

		//espera que esteja apto a transmitir para poder escrever o caracter via UART
		res = wait_event_interruptible_timeout(seri_d->tx, kfifo_len(seri_d->txfifo)<SIZE_KFIFO, TIME);
		//testa se há espaço para escrita no kfifo

		//lab5 - 3.5 - Ctrl-C//
		if(res == -ERESTARTSYS) {
			kfree(string);
			printk(KERN_ALERT "!!! EXIT FROM WRITING !!!\n");
			return -ERESTARTSYS;
		} else {
			if(res == 0) {
				printk(KERN_ALERT "!!! TIMEOUT !!!\n");
				return -1;

			}
		}

		chars_sent = kfifo_put(seri_d->txfifo, (string+pos), chars_left);
		pos += chars_sent;
		chars_left -= chars_sent;

		if(inb(START_ADDRESS + UART_LSR) & UART_LSR_THRE) { //se ... inicia o envio
			kfifo_get(seri_d->txfifo, &um, 1);
			outb(um, START_ADDRESS+UART_TX);
		}

		if(!chars_left) {
			printk(KERN_ALERT "!!! UPLOADED %d BYTES !!!\n", count);
		}
	}

	if ((inb(START_ADDRESS+UART_LSR) & UART_LSR_DR & UART_LSR_OE)) {
		printk(KERN_ALERT "!!! ERROR OVERRUN !!!\n");
		kfree(string);
		return -EIO;
	}

	kfree(string);
	
	return (count-chars_left);
}


/*open*/

int seri_open(struct inode *inode, struct file *filp)
{
	filp->private_data = container_of(inode->i_cdev,seri_dev, c_dev);

	//lab5 - 3.6 - apenas um "user" para aceder à porta serie//
	spin_lock(&(seri_d->user_lock));
	if(users)
	{
		printk(KERN_ALERT "!!! ACCESS DENIED - ONLY ONE ALLOWED USER !!!\n");
		return -1;
	}
	users++;
	spin_unlock(&(seri_d->user_lock));
	
	printk(KERN_ALERT "OPENED\n");
	
	nonseekable_open(inode, filp);

	return 0;
}

/*release*/

int seri_release(struct inode *inode, struct file *filp)
{
	//lab5 - 3.6 - apenas um "user" para aceder à porta serie//
	spin_lock(&(seri_d->user_lock));
	if(users)
	{
		users--;
	}
	spin_unlock(&(seri_d->user_lock));

	printk(KERN_NOTICE "RELEASED\n");

	return 0;
}




/*****************************************************************INIT****************************************************************************/

static int seri_init(void)
{
	int major = MAJORE;
	int minor = MINORE;

	unsigned char lcr = 0;
	unsigned char fcr = 0;
	unsigned char ier = 0;

	//GET MAJOR//
	if (major)
	{
		dev=MKDEV(MAJORE, MINORE);
		if (register_chrdev_region(dev, SERI_DEVS, "seri"))
		{
			printk(KERN_ALERT "!!! MAJOR ERROR !!!\n");
			return -1;
		}
	}
	else
	{
		if (alloc_chrdev_region(&dev, minor, SERI_DEVS, "seri"))
		{
			printk(KERN_ALERT "!!! MAJOR ERROR !!!\n");
			return -1;
		}
		major = MAJOR(dev);
	}
	//*********//
	

	//INICIALIZACAO DA STRUCT seri_d//

	seri_d=kmalloc(sizeof(seri_dev), GFP_KERNEL);
	cdev_init(&seri_d->c_dev, &seri_fops);
	seri_d->c_dev.ops = &seri_fops;
	seri_d->c_dev.owner = THIS_MODULE;

	init_waitqueue_head(&(seri_d->rx));
	init_waitqueue_head(&(seri_d->tx));

	spin_lock_init(&seri_d->rxlock);
	spin_lock_init(&seri_d->txlock);
	spin_lock_init(&seri_d->user_lock);

	seri_d->rxfifo =kfifo_alloc(SIZE_KFIFO, GFP_KERNEL, &(seri_d->rxlock));
 	seri_d->txfifo =kfifo_alloc(SIZE_KFIFO, GFP_KERNEL, &(seri_d->txlock));

	//*******************************//

	
	if(cdev_add(&seri_d->c_dev, dev, SERI_DEVS) < 0)
	{
		printk(KERN_ALERT "!!! ERROR CDEV !!!\n");
		return -1;
	}

	if (!request_region(START_ADDRESS, SIZE, "seri"))
	{
		printk (KERN_ALERT "!!! ERROR - PORTS OCCUPIED !!!\n");
		return -1;
	}

	if ((request_irq(IRQ, int_handler, 0, "seri", seri_d)))
	{
		printk(KERN_ALERT "!!! ERROR REQUEST_IRQ !!!\n");
		return -1;
	}

	//****Endereços****//

	//lcr ports//
	lcr= inb(START_ADDRESS+UART_LCR);
	lcr |= UART_LCR_WLEN8 | UART_LCR_EPAR | UART_LCR_PARITY | UART_LCR_STOP | UART_LCR_DLAB;
	outb(lcr, START_ADDRESS+UART_LCR); //definir os bits anteriores

	//bitrate//
	outb(UART_DIV_1200, START_ADDRESS+UART_DLL); //colocar os 1200bps
	outb(0x00, START_ADDRESS+UART_DLM);

	lcr &= ~UART_LCR_DLAB;
	outb(lcr, START_ADDRESS+UART_LCR); //voltar a meter DLAB a 0, colocando no modo receiver e transmitter

	//interrupts//
	ier = inb(START_ADDRESS+UART_IER);
	ier |= UART_IER_RDI | UART_IER_THRI;
	outb(ier, START_ADDRESS+UART_IER);

	//HW fifos - 3.7//
	fcr = inb(START_ADDRESS+UART_FCR);
	fcr |= UART_FCR_ENABLE_FIFO | UART_FCR_TRIGGER_MASK;
	outb(fcr, START_ADDRESS+UART_FCR);

	//*****************//

	printk(KERN_ALERT "Major number = %d\n", major);
	
	return 0;
}

/***********************************************************************EXIT********************************************************************/

static void seri_exit(void)
{

	//kfifo free//
	kfifo_free(seri_d->rxfifo);
	kfifo_free(seri_d->txfifo);

	//handler free//
	free_irq(IRQ,seri_d);

	//dev free//
	cdev_del(&seri_d->c_dev);
	unregister_chrdev_region(dev, SERI_DEVS);

	release_region(START_ADDRESS,SIZE);

	printk(KERN_NOTICE "JOB DONE - ALL FREE\n");
}

module_init(seri_init);
module_exit(seri_exit);
