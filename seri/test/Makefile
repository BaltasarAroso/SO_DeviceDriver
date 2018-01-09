all: nonblock ioctl ctrlC users poll

nonblock: nonblock.c
	gcc -o nonblock nonblock.c

ioctl: ioctl.c
	gcc -o ioctl ioctl.c

ctrlC: ctrlC.c
	gcc -o ctrlC ctrlC.c

users: users.c
	gcc -o users users.c

poll: poll.c
	gcc -o poll poll.c

clean:
	rm -rf nonblock ioctl ctrlC users
