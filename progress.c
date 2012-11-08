#include <stdio.h>

void main() {
int i;
for(i=0;i<=100;i++) {
        printf("Progress %d\%",i);
        printf("\r");
        fflush(stdout);
        sleep(1);
        }
}
