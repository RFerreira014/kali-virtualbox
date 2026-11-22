#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    pid_t id = fork();

    if (id < 0) {
        perror("fork");
        return 1;
    }

    printf("Hello world from PID: %d, PPID: %d\n", getpid(), getppid());

    if (id == 0) {
        printf("Hello from child: %d\n", getpid());
    } else {
        printf("Hello from dad: %d\n", getpid());
    }

    return 0;
}
