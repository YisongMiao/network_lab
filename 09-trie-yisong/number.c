#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

void str_set(char* a, char* b){
   int i;
   for(i = 0; i < strlen(b); i++){
      *(a + i) = *(b + i);
   }
}

char * translate(char *current_key){
   char *output = NULL;
   output = (char *)malloc(4 * sizeof(char));
   if(2 == 2){
      if(strcmp(current_key, "0") == 0){
         printf("Good!\n");
         str_set(output, "1100");
      }
      if(strcmp(current_key, "1") == 0){
         str_set(output, "0011");
      }
      if(strcmp(current_key, "00") == 0){
         str_set(output, "1000");
      }
      if(strcmp(current_key, "01") == 0){
         str_set(output, "0100");
      }
      if(strcmp(current_key, "10") == 0){
         str_set(output, "0010");
      }
      if(strcmp(current_key, "11") == 0){
         str_set(output, "0001");
      }
   }
   return output;
}

int main(){
	int a = 5;
	int c = 6;
	int b = 2;
	int d = (int)ceil((double)a / (double)(b));
	int e = (int)ceil((double)c / (double)(b));
	printf("%d\n", d);
	printf("%d\n", e);
   printf("%s\n", translate("01"));
}