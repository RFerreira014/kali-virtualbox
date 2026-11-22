#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int id = fork();
    int fork();

    printf("Hello process %d\n", id);

   // if (id == 0) {
   //     printf("hello from child process %d\n", getppid());
    //} else {
   //// }

    return 0;
}