#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#define STRIDE_SIZE 2
#define LEVEL_SIZE 4
#define BIT_ARRAY_ENTRY_EMPTY 999

struct leaf_node
{
	int num_port;
};

struct internal_node
{
	int bit_array[LEVEL_SIZE];
	struct leaf_node *children_leaf_node[LEVEL_SIZE]; //array of child leaf node
	struct internal_node *children_internal_node[LEVEL_SIZE]; //array of child leaf node
};


struct leaf_node * create_leaf_node(int no_port){
	struct leaf_node * new_leaf_node = NULL;
	new_leaf_node = (struct leaf_node*)malloc(sizeof(struct internal_node));
	if(new_leaf_node){
		new_leaf_node->num_port = no_port;
	}
	return new_leaf_node;
}


struct internal_node * create_root_node(){
	struct internal_node * new_internal_node = NULL;
	new_internal_node = (struct internal_node*)malloc(sizeof(struct internal_node));
	if(new_internal_node){
		int i;
		for(i = 0; i < LEVEL_SIZE; i++){
			new_internal_node->bit_array[i] = BIT_ARRAY_ENTRY_EMPTY;
			new_internal_node->children_leaf_node[i] = NULL;
			new_internal_node->children_internal_node[i] = NULL;
		}
	}
	return new_internal_node;
}


struct internal_node * create_internal_node(int no_port){
	struct internal_node * new_internal_node = NULL;
	new_internal_node = (struct internal_node*)malloc(sizeof(new_internal_node));
	if(new_internal_node){
		int i;
		for(i = 0; i < LEVEL_SIZE; i++){
			new_internal_node->bit_array[i] = 0;
			new_internal_node->children_leaf_node[i] = create_leaf_node(no_port);
			printf("i: %d, num_port: %d\n", i, new_internal_node->children_leaf_node[i]->num_port);
			new_internal_node->children_internal_node[i] = NULL;
		}
	}
	return new_internal_node;
}


//yeah, my ubuntu don't support str_set...
void str_set(char* a, char* b){
   int i;
   for(i = 0; i < strlen(b); i++){
      *(a + i) = *(b + i);
   }
}


char * translate(char *input_key){
   char *output = NULL;
   output = (char *)malloc(LEVEL_SIZE * sizeof(char));
   if(STRIDE_SIZE == 2){
      if(strcmp(input_key, "0") == 0){
         str_set(output, "1100");
      }
      if(strcmp(input_key, "1") == 0){
         str_set(output, "0011");
      }
      if(strcmp(input_key, "00") == 0){
         str_set(output, "1000");
      }
      if(strcmp(input_key, "01") == 0){
         str_set(output, "0100");
      }
      if(strcmp(input_key, "10") == 0){
         str_set(output, "0010");
      }
      if(strcmp(input_key, "11") == 0){
         str_set(output, "0001");
      }
   }
   return output;
}


void insert_node(struct internal_node * root_node, char * key, int no_port){
	int level;
	int length = (int)ceil((double)strlen(key) / (double)(STRIDE_SIZE));
	//for example, when str is 7 long, stride size is 2, then the length is 4
	char* getkey = malloc((LEVEL_SIZE + 1) * sizeof(char));
	memcpy(getkey, key, strlen(key));
	getkey[strlen(key)] = '\0';
	printf("key: %s\n", key);
	//char* current_key = malloc(4 * sizeof(char));
	//char* translate(current_key) = malloc(4 * sizeof(char));
	struct internal_node * current_node = root_node;
	char* current_key;
	for(level = 0; level < length; level++){
		current_key = NULL;
		current_key = malloc((LEVEL_SIZE + 1) * sizeof(char));
		int offset1, copy_length;
		offset1 = level * STRIDE_SIZE;
		if((offset1 + STRIDE_SIZE) < strlen(key) + 1){
			copy_length = STRIDE_SIZE;
		}
		else{
			copy_length = strlen(key) - offset1;
		}
		memcpy(current_key, getkey + offset1, copy_length);
		current_key[LEVEL_SIZE] = '\0';
		printf("------Current Key-------: %s\n", current_key);
		//by far, we already got the current key, then we find the index in the bit_array
		//printf("ya: %s\n", translate(current_key));

		int j;
		for(j = 0; j < LEVEL_SIZE; j++){
			//printf("kk:%d\n", translate(current_key)[j] - '0');
			if(translate(current_key)[j] - '0' == 1){  //means here's gonna be an entry
				printf("level: %d, j: %d\n", level, j);
				if(current_node->bit_array[j] == BIT_ARRAY_ENTRY_EMPTY){  //means target empty!
					if(level + 1 != length){  //means not the end!
						printf("11\n");
						current_node->children_internal_node[j] = create_root_node();
						current_node->bit_array[j] = 1;
						current_node = current_node->children_internal_node[j];
					}
					else{  //means reach the end already
						printf("12\n");
						current_node->children_leaf_node[j] = create_leaf_node(no_port);
						current_node->bit_array[j] = 0;
						//reach end already, no new current_node!
					}
				}
				else if(current_node->bit_array[j] == 1){  //means target internal node!
					printf("2\n");
					current_node = current_node->children_internal_node[j];
					if(level + 1 == length){//means it reach end!
						printf("22\n");
						int u;
						for(u = 0; u < LEVEL_SIZE; u++){
							if(current_node->bit_array[u] == BIT_ARRAY_ENTRY_EMPTY){
								printf("%d\n", u);
								current_node->bit_array[u] = 0;
								current_node->children_leaf_node[u] = create_leaf_node(no_port);
							}
						}
					}
				}
				else if(current_node->bit_array[j] == 0){  //means target leaf node!
					if(level + 1 != length){  //means not the end! so leaf push
						printf("31\n");
						printf("j: %d\n", j);
						int current_port = current_node->children_leaf_node[j]->num_port;
						current_node->children_leaf_node[j] = NULL;
						//current_node->children_internal_node[j] = create_internal_node(current_port);
						current_node->bit_array[j] = 1;
						current_node->children_internal_node[j] = create_root_node();
						int u;
						for(u = 0; u < LEVEL_SIZE; u++){
							current_node->children_internal_node[j]->bit_array[u] = 0;
							current_node->children_internal_node[j]->children_leaf_node[u] = create_leaf_node(current_port);
						}
						printf("CACA\n");
						current_node = current_node->children_internal_node[j];
					}
					else{
						printf("32\n");
						current_node->children_leaf_node[j]->num_port = no_port;
						//reach end already, no new current_node!
						printf("32level: %d, j: %d, current_port: %d\n", level, j, no_port);
					}
				}
			}
		}
		free(current_key);
		free(translate(current_key));

	}
	printf("-----Insertion Done!-----\n");
}


int search_node(struct internal_node * root_node, char * key){
	int level;
	int length = (int)ceil((double)strlen(key) / (double)(STRIDE_SIZE));
	//for example, when str is 7 long, stride size is 2, then the length is 4
	char* getkey = malloc((LEVEL_SIZE + 1) * sizeof(char));
	memcpy(getkey, key, strlen(key));
	getkey[strlen(key)] = '\0';
	//char* current_key = malloc(4 * sizeof(char));
	//char* translate(current_key) = malloc(4 * sizeof(char));
	struct internal_node * current_node = root_node;
	char* current_key;
	for(level = 0; level < length; level++){
		current_key = NULL;
		current_key = malloc((LEVEL_SIZE + 1) * sizeof(char));
		int offset1, copy_length;
		offset1 = level * STRIDE_SIZE;
		if((offset1 + STRIDE_SIZE) < strlen(key) + 1){
			copy_length = STRIDE_SIZE;
		}
		else{
			copy_length = strlen(key) - offset1;
		}
		memcpy(current_key, getkey + offset1, copy_length);
		current_key[LEVEL_SIZE] = '\0';
		//by far, we already got the current key
		//then we find the index in the bit_array
		//note: will only match one 
		//printf("Current searching key: %s\n", current_key);
		int j;
		for(j = 0; j < LEVEL_SIZE; j++){
			if(translate(current_key)[j] - '0' == 1){  //means here's gonna be an entry
				if(current_node->children_leaf_node[j]){
					//reach leaf node!
					//printf("Hey!\n");
					//printf("level: %d, j: %d, port: %d\n", level, j, current_node->children_leaf_node[j]->num_port);
					if(level + 1 == length){
						return current_node->children_leaf_node[j]->num_port;
					}
					else{
						return 0;
					}
				}
				else if(current_node->children_internal_node[j]){
					current_node = current_node->children_internal_node[j];
				}
				else{
					return 0;
				}
			}
		}
	}
	return 0;
}


int main(){
	printf("Hello!\n");
	//create root
	struct internal_node * root_node = create_root_node();
	printf("LaLa\n");

    //insert_node(root_node, "11", 9);
    insert_node(root_node, "1100", 8);
    insert_node(root_node, "11", 8);
    //insert_node(root_node, "1110", 7);

    printf("The port for %s node is: %d\n", "1100", search_node(root_node, "1100"));
    printf("The port for %s node is: %d\n", "1101", search_node(root_node, "1101"));
    printf("The port for %s node is: %d\n", "1111", search_node(root_node, "1111"));
    printf("The port for %s node is: %d\n", "1110", search_node(root_node, "1110"));
    printf("The port for %s node is: %d\n", "111011", search_node(root_node, "111011"));
    printf("The port for %s node is: %d\n", "11", search_node(root_node, "11"));
    printf("The port for %s node is: %d\n", "00", search_node(root_node, "00"));
    return 0;
}


