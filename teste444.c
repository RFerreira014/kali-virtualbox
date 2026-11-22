#include <stdio.h>

int main(){
    double f = 0.0f;
    double pi = 3.14159265358979323846;
    double euller = 2.71828182845904523536;
    char grade = 'A';
    char symbol = '!';
    char name[] = "Rui Ferreira";


    printf("Size of double: %zu bytes\n", sizeof(f));
    printf("Value of pi: %0.12f\n", pi);
    printf("Value of euller: %0.12f\n", euller);
    printf("Value of grade: %c\n", grade);
    printf("numeric Value of grade: %d\n", grade);
    printf("Value of symbol: %c\n", symbol);
    printf("numeric Value of symbol: %d\n", symbol);
    printf("the size of symbol is: %zu bytes\n", sizeof(symbol) );
    printf("the size of grade is: %zu bytes\n", sizeof(grade) );
    printf("o meu nome é %s\n", name);
    return 0;
}