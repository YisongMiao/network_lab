#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

//it means that 
#define LEVEL_SIZE 2


struct trie_node
{
	struct trie_node *children[LEVEL_SIZE];
	int isEndOfWord;
};


struct trie_node * create_node(){
	struct trie_node * new_node = NULL;
	new_node = (struct trie_node *)malloc(sizeof(struct trie_node));
	if(new_node){
		new_node->isEndOfWord = 0;
		int i;
		for(i = 0; i < LEVEL_SIZE; i++){
			new_node->children[i] = NULL;
		}
	}

	return new_node;
}


void insert_node(struct trie_node *root, char * key){
	int level;
	int length = strlen(key);
	int index;

	struct trie_node *current_pointer = root;
	for(level = 0; level < length; level++){
		index = key[level] - '0';  //convert char to int
		if(!current_pointer->children[index]){
			current_pointer->children[index] = create_node();
		}
		//move the current pointer
		current_pointer = current_pointer->children[index];
	}
	current_pointer->isEndOfWord = 1;
	//mark as the end leaf
}


int search(struct trie_node *root, char * key){
	int level;
	int length = strlen(key);
	int index;

	struct trie_node *current_pointer = root;
	for(level = 0; level < length; level++){
		index = key[level] - '0';  //convert char to int
		//index = (int)key[level] - (int)'a';
		if(!current_pointer->children[index]){
			return 0;
			//can't find.
		}
		current_pointer = current_pointer->children[index];
	}
	return (current_pointer && current_pointer->isEndOfWord);
	//return 1;
}

int main(){
	printf("Hello!\n");
	//create root
	struct trie_node * root_node = create_node();
 
    char output[][32] = {"Not present in trie", "Present in trie"};

    //int i;
    //for (i = 0; i < 8; i++)
    //    insert_node(root_node, keys[i]);
    insert_node(root_node, "010");
    insert_node(root_node, "101");
    insert_node(root_node, "1011");

    printf("%s --- %s\n", "010", output[search(root_node, "010")] );
    printf("%s --- %s\n", "101", output[search(root_node, "101")] );
    printf("%s --- %s\n", "0", output[search(root_node, "0")] );
    printf("%s --- %s\n", "10", output[search(root_node, "10")] );
    printf("%s --- %s\n", "1011", output[search(root_node, "1011")] );
 
    return 0;
}
