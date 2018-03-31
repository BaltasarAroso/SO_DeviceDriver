# SO_Device Driver
Serial Port Device Driver implemented both with polling and interrupt methods.

## Project Tasks
In Linux device drivers can be implemented as kernel modules, which are object code files that can be loaded (and unloaded) dynamically by the Linux kernel, either at boot time or later. This makes it possible to install a device driver without having to recompile the kernel or even reboot the operating system.

This project was divided in 4 tasks:
- The goal of the first task is to develop a basic **"char device driver"** that in the second task will become a simple device driver for the standard serial port of a PC.
- In the second task we've developed a basic device driver (DD) that relies on polling. The use of polling severely limits the maximum bit-rate that can be used. (``serp`` for **ser**ial, **p**olled mode)

- The UART is able to generate interrupts on some events, and by proper programming it can be used for interrupt driven I/O, which allows for a more efficient use of the processor. So in the third task, we used interrupt driven I/O. Because, the control flow, and also the data flow, in an interrupt driven DD is very different from those in a DD that uses polling, you've developed a separate device driver, whose name should be ``seri``, for interrupt driven I/O.

- Although the 2rd and the 3th tasks guided us through the implementation of a basic DD for the serial port, completing both tasks would not be enough to achieve a complete DD. We've needed to implement the **enhancements**.

The following is a list of some improvements to the seri DD:
- Error Handling
- Minimize global variables.
- Eliminate race conditions.
- Honor the O_NONBLOCK flag.
- Add ioctl operations to configure communication parameters.
- Allow for interrupting a process blocked inside a read() syscall.
- Add access control to prevent more than one "user" to access the serial port.
- Add FIFO support.
- Add support for the select() syscall.
