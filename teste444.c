#include <stdio.h>

int main(){
    float f = 0.0f;
    sizeof(f);

    printf("Size of float: %zu bytes\n", sizeof(f));
    return 0;
}