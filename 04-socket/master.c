/* client application */

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
 
int main(int argc, char *argv[])
{
    printf("good\n");
    FILE * fp;
    fp = fopen("/home/yisongmiao/networking/04-socket/workers.conf", "r");
    if (fp == NULL){
        exit(EXIT_FAILURE);
    }
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL){
        fprintf(stdout, "Current working dir: %s\n", cwd);
    }
    char * war_and_peace_path;
    war_and_peace_path = strcat(cwd, "/");
    war_and_peace_path = strcat(war_and_peace_path, argv[1]);
    printf("The absolute path is:%s\n", war_and_peace_path);

    FILE * war_and_peace_fp;
    war_and_peace_fp = fopen(war_and_peace_path, "r");
    fseek(war_and_peace_fp, 0, SEEK_END);
    int lengthOfFile = ftell(war_and_peace_fp);
    printf("The length is:%d\n", lengthOfFile);

    char *line = NULL;
    char lines[100][100];
    size_t len = 0;
    int line_no = 0;
    while ((getline(&line, &len, fp)) != -1) {
        //lines[line_no] = line;
        strcpy(lines[line_no], line);
        line_no += 1;
    }
    printf("good\n");


    int i, j, current_number;
    int recv_statistics[line_no][26];
    for(i = 0; i < line_no; i++){
        for(j = 0; j < 26; j++){
            recv_statistics[i][j] = 0;
        }
    }

    //Connect to all the workers, and send the required messages.
    int sock[line_no];
    struct sockaddr_in worker;
    int start_point, end_point;
    for(i = 0; i < line_no; i++){
        if(i == 0){
            start_point = 0;
            end_point = lengthOfFile / (float) line_no;
            end_point = (int) floor(end_point);
        }
        else if(i != line_no - 1){
            start_point = end_point;
            end_point = lengthOfFile / (float) line_no;
            end_point = (int) floor(end_point);
        }
        else{
            start_point = end_point;
            end_point = lengthOfFile;
        }
        printf("Current Start Point:%d, Current End Point:%d\n", start_point, end_point);

        //Create socket
        sock[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (sock[i] == -1) {
            printf("Could not create socket");
        }
        printf("Socket created\n");
         
        worker.sin_addr.s_addr = inet_addr(lines[i]);
        worker.sin_family = AF_INET;
        worker.sin_port = htons( 8888 );
        printf("We are currently trying to connect to %s\n", lines[i]);
    
        //Connect to remote worker
        if (connect(sock[i], (struct sockaddr *)&worker, sizeof(worker)) < 0) {
            perror("connect failed. Error");
            return 1;
        }
        
        printf("Successfully Connected to: %s\n", lines[i]);

        //send relative location, (relative location is argv[1])
        if(send(sock[i], argv[1], strlen(argv[1]), 0) < 0){
            printf("Failed to Send Relative Location");
            return 1;
        }
        else{
            printf("Successfully sent relative location:%s\n", argv[1]);
        }


        int start_point_network_byte_order = htonl(start_point);
        if(send(sock[i], &start_point_network_byte_order, sizeof(int), 0) < 0){
            printf("Failed to Send Start Point\n");
        }
        else{
            printf("Successfully sent start point:%d\n", start_point_network_byte_order);
        }

        int end_point_network_byte_order = htonl(end_point);
        if(send(sock[i], &end_point_network_byte_order, sizeof(int), 0) < 0){
            printf("Failed to Send End Point\n");
        }
        else{
            printf("Successfully sent End point:%d\n", end_point_network_byte_order);
        }
    }

    //receive the answer from each worker
    for(i = 0; i < line_no; i++){
        for(j = 0; j < 26; j++){
            if(recv(sock[i], &current_number, sizeof(current_number), 0) > 0){
                recv_statistics[i][j] += ntohl(current_number);
                printf("Successfully received from %s:%d\n", lines[line_no], ntohl(current_number));
            }
            else{
                printf("Failed to receive\n");
            }
        }
    }

    //aggregate the result
    int statistics_result[26];
    for(i = 0; i < 26; i++){
        statistics_result[i] = 0;
    }
    for(i = 0; i < line_no; i++){
        for(j = 0; j < 26; j++){
            //printf("%d, %d\n", j, recv_statistics[i][j]);
            statistics_result[j] += recv_statistics[i][j]; 
        }
    }

    //print the result
    for(i = 0; i < 26; i++){
        printf("%c %d\n", (i + 97), statistics_result[i]);
    }

    return 0;
}
