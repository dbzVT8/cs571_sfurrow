#include <stdio.h>
#include <unistd.h>

/*
 * getpid - test the getpid system call
 * Usage: getpid
 *
 * Just calls getpid() and outputs the result
 */

int
main(void)
{
	pid_t pid = getpid();
    printf("Pid of this process is: %u\n",pid);
    _exit(0);
}
