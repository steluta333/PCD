/**
 * @file client
 * @author Steluta Talpau
 * @date 2017-10-29
 *
 * @brief Server for OSUE exercise 1B `Battleship'.
 */
 //trateaza ctrl+c

// IO, C standard library, POSIX API, data types:
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

// Assertions, errors, signals:
#include <assert.h>
#include <errno.h>
#include <signal.h>

// Time:
#include <time.h>

// Sockets, TCP, ... :
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>

// stuff shared by client and server:
#include "common.h"
#define _FREE 0
#define _NO_SHIP -1
#define _SHIP 1

int line;//line of the next position to hit
int colomn;//colomn of the next position to hit
int game_matrix[10][10];// matrix where we keep the information about the coordinates that we already tried
// Static variables for things you might want to access from several functions:
static const char *port_to_connect =DEFAULT_PORT; //port to connect
static char *prog_name=NULL; //program name
static const char *host_name=DEFAULT_HOST; //host

// Static variables for resources that should be freed before exiting:
static struct addrinfo *ai = NULL;      // addrinfo struct
static int sockfd = -1;                 // socket file descriptor
//static int connfd = -1;                 // connection file descriptor

/**@brief: prints the usage of the programm; this function is called
*when wrong arguments have been given
*/
void print_usage(){
    printf("usage:%s [-h HOSTNAME] [-p PORT]\n",prog_name);

}

/**@brief:his function parses command line arguments and it's mandatory to be called.
*It also saves the program in the prog_name variable
*@details:if the function can receive as arguments -p and -h( at least 0 arguments, at most both)
*case in which variables port_to_connect(in case of-p)  and host_name(in case of -h) will be updated
*@parameters: argc -number of arguments received by main
*            *argv[]-list of those arguments
*/
void parse_command_line(int argc, char *argv[]){
    int option;
    int flag=0;
    prog_name=(char*) malloc(sizeof(argv[0]));
    if(prog_name==NULL){
        fprintf(stderr,"Memory could not be allocated\n");
        exit(EXIT_FAILURE);
    }
    prog_name=argv[0];
    while((option=getopt(argc,argv,"h:p:"))!=-1){
        if(option=='h'&& flag==0){
            host_name=optarg;
            flag=1;
        }
        else if(option=='p' && flag==1){
            port_to_connect=optarg;
            flag=2;
        }
        else if(option=='?'){
            //fprintf(stderr,"%c\n",optopt);
            print_usage();
            exit(EXIT_FAILURE);
        }
        else{
            print_usage();
            exit(EXIT_FAILURE);
        }

    }

}
/**@brief:returns the value of the bit on pozition poz, in the character a
*as an integer
*@parameters: an integer position, and a character
*/
int get_bit_poz(int poz, char a){
    char mask=0;
    mask=1<<poz;
    if((a&mask)){
        return 1;
    }
    return 0;
}

/**@brief:returns the xor value of the bits [0,6] from a byte
*@parameters: char a, the byte whom we want to find the parity of bits [0,6]
*/
int return_parity(char a){
    int xor_value=get_bit_poz(0,a);
    for(int i=1; i<=6; i++){
        xor_value=xor_value^get_bit_poz(i,a);
    }
    return xor_value;
}
/**@brief:encodes the values of the line and collumn on the bits[0,6] of a byte,
*then adds the parity bit on the 7 th position
*@parameters: line and colomn of the position which represents the next hit
*/
char encode_message(int line, int col){
    unsigned char aux=0;
    char aux_line=line;
    char aux_col=col;
    char coord=aux_col+aux_line*10;
    int parity=return_parity(coord);
    if(parity==0){
        //printf("parity 0\n");
        return coord;
    }
    //printf("parity 1\n");
    aux=(1<<7)|coord;
    return aux;

}

typedef struct { //point structure, wich stores an x and y coordinate
	int x;
	int y;
} point;


/**@brief:updates the global variables line and colomn with the coordinates of the point next to be hit
*returns an encoded message (as a char) of those coordinates
*/
char pick_coordinates_to_hit(){
	printf("Tabla curenta de joc");
	printf("\n");
    for (int i = 0; i < 10; ++i) {
        for (int j = 0; j < 10; ++j) {
            printf("%d ", game_matrix[i][j] + 1);
        }
        printf("\n");
    }
	
	int x,y; // we want to make sure that the coordinates to be picked will be okay in range [1 10]! don't forget to substract 1 from each coordinate
	printf("Insert the row and colomn of the position to attack\n");
	scanf("%d%d", &x, &y);
	while((x<0 || x>10)||(y<0 || y>10)){
		printf("Coordinates introduced are invalid.\n");
		printf("Insert the row and colomn of the position to attack\n");
		scanf("%d%d", &x, &y);
	}
	
    line=x-1;
    colomn=y-1;
    printf("ATACAM %d %d\n", x, y);
    return encode_message(line,colomn);

}
/**@brief:returns the value of the hit variable sent by the server
*@parameters: a 1 byte message received from the server
*/
int get_hit(char buffer){
    char mask=0;
    mask=((1<<2)-1)&buffer;
    return mask;
}

/**@brief:returns the value of the status variable sent by the server
*@parameters: a 1 byte message received from the server
*/
int get_status(char buffer){
    char mask=0;
    mask=(((1<<4)-1)&buffer)>>2;
    //printf("%d\n",mask);
    return mask;

}


void sighandler(int signum) {
	printf("Caught signal %d, coming out...\n", signum);
	printf("Client stopped\n");	
	shutdown (sockfd,2);
	close(sockfd);
	exit(1);
}




int main(int argc, char *argv[])
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, sighandler);
    parse_command_line(argc, argv);
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int res = getaddrinfo(host_name, port_to_connect, &hints, &ai); //host la null
    if (res != 0) {
        fprintf(stderr, "Getaddrinfo error \n");
        exit(EXIT_FAILURE);
    }

    sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if(sockfd==-1){
        fprintf(stderr,"Could not create socket\n");
        exit(EXIT_FAILURE);
    }

    int connection_status=connect(sockfd,ai->ai_addr,ai->ai_addrlen);
    if(connection_status!=0){
        fprintf(stderr,"Could not connect to the server\n");
        exit(EXIT_FAILURE);
    }
    char buffer; // variable where the  message from server will be saved and where the message to be send will be saved
    int bytes_written; //bytes written by write functions; error checking variable
    int status=0; //game running status
    int bytes_read=0; // bytes read by the read function; error checking variable
    int hit=0;
    int rounds=0; //number of rounds played
    while(status==0){
        rounds=rounds+1;

        buffer=pick_coordinates_to_hit(); //create buffer, pick coordinates
        //printf("Sent to server%d\n",buffer);
        bytes_written=write(sockfd,&buffer,sizeof(buffer));
        if(bytes_written==-1 || bytes_written==0){
            fprintf(stderr,"Could not write in the socket. Server was disconnected\n");
            exit(EXIT_FAILURE);
        }
        bytes_read=read(sockfd,&buffer,sizeof(buffer));
        if(bytes_read==-1 || bytes_read==0){
            fprintf(stderr,"Could not read message from server. Server was disconnected\n");
            exit(EXIT_FAILURE);
        }
	
        //printf("Received from server%d, round %d\n",buffer,rounds);
        status=get_status(buffer); // tells the client if the game is running/ error has occured/ game is over
        printf("status:%d\n", status);
        hit=get_hit(buffer); //hit tells the client if a ship has been hit and also tells if it has been sunk.
        if(hit==0){
            game_matrix[line][colomn]=_NO_SHIP;
        }
        else{
            game_matrix[line][colomn]=_SHIP;
        }

        //printf("hit:%d\n", hit);
    }
    //printf("Status at the end %d , hit %d\n",status,hit);
	
    if(status==1 && hit==3 && rounds>10){
        printf("Game over.You have succesfully sunk all the ships in %d\n",rounds);
    }
    else if(status==1 ){
        printf("Game over.Maximum number of rounds reached \n");
    }
    else if(status==2){
        fprintf(stderr,"Error: The last message contained an invalid parity bit\n");
    }
    else if(status==3){
        printf("Error: The last message contained an invalid coordinate\n");
    }

    //free(prog_name);
    close(sockfd);
    return 0;
}
