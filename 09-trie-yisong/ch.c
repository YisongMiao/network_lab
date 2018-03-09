#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

int main(){
	int i;
	for(i = 0; i < 3; i++){
		char * a = NULL;
		a = malloc(i * sizeof(char));
		printf("Good!\n");
	}
}