/* server application */
 
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
 
int main(int argc, const char *argv[])
{
    printf("yes");
    int s, cs;
    struct sockaddr_in server, client;
     
    // Create socket
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Could not create socket");
		return -1;
    }
    printf("Socket created");
    
    // Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(8888);
     
    // Bind
    if (bind(s,(struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("bind failed. Error");
        return -1;
    }
    printf("bind done");
     
    // Listen
    listen(s, 3);
     
    // Accept and incoming connection
    printf("Waiting for incoming connections...");
     
    // accept connection from an incoming client
    int c = sizeof(struct sockaddr_in);
    if ((cs = accept(s, (struct sockaddr *)&client, (socklen_t *)&c)) < 0) {
        perror("accept failed");
        return 1;
    }
    printf("Connection accepted");

    int msg_len = 0;
    char relative_location[100];
    if((msg_len = recv(cs, relative_location, sizeof(relative_location), 0)) > 0){
        relative_location[msg_len] = '\0';
        printf("%d Bytes are Received\n", msg_len);
        printf("Successfully Received relative_location: %s\n", relative_location);
    }
    else{
        printf("Failed to receive relative_location\n");
    }
    
    msg_len = 0;
    int start_point;
    if((msg_len = recv(cs, &start_point, sizeof(start_point), 0)) > 0){
        start_point = ntohl(start_point);
        printf("Successfully Received start_point: %d\n", start_point);
    }
    else{
        printf("Failed to receive start_point\n");
    }

    msg_len = 0;
    int end_point;
    if((msg_len = recv(cs, &end_point, sizeof(end_point), 0)) > 0){
        end_point = ntohl(end_point);
        printf("Successfullt Received end_point: %d\n", end_point);
    }
    else{
        printf("Failed to receive end_point\n");
    }

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL){
        fprintf(stdout, "Current working dir: %s\n", cwd);
    }

    char * war_and_peace_path;
    war_and_peace_path = strcat(cwd, "/");
    war_and_peace_path = strcat(war_and_peace_path, relative_location);
    printf("Absolute Location is:%s\n", war_and_peace_path);

    FILE *file = fopen(war_and_peace_path, "r");
    if(file == NULL){
        printf("Can't open file\n");
    }
    else{
        printf("Successfully Opened File\n");
    }

    int current_char_asc;
    int statistics[26];
    int i;
    for(i = 0; i < 26; i++){
        statistics[i] = 0;
    }

    printf("start_point and end_point are respectively:%d, %d\n", start_point, end_point);
    int current_position = 0;
    while((current_char_asc = fgetc(file)) != EOF){
        if(current_position < end_point && current_position > start_point - 1){
            if((current_char_asc > 64 && current_char_asc < 91)){
                statistics[current_char_asc - 65] += 1;
            }
            if((current_char_asc > 96 && current_char_asc < 123)){
                statistics[current_char_asc - 97] += 1;
            }
        }
        current_position += 1;
    }
    printf("current_position is:%d\n", current_position);
    for(i = 0; i < 26; i++){
        int to_send;
        to_send = htonl(statistics[i]);
        //write(cs, &to_send, sizeof(to_send));
        if(send(cs, &to_send, sizeof(to_send), 0) > 0){
            printf("Successfully send %dth count: %d\n", i, statistics[i]);
        }
        else{
            printf("Failed to send\n");
        }
    }
    return 0;
}
