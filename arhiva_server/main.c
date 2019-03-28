/**
 * @file server
 * @author Steluta Talpau
 * @date 2017-10-29
 *
 * @brief Server for OSUE exercise 1B `Battleship'.
 */

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
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>
#include <fcntl.h>

// stuff shared by client and server:
#include "common.h"
//int remaining_moves[MAX_CLIENTS+1]; //how many moves the client i is still allowed to do

int game_matrix[10][10];
// Static variables for things you might want to access from several functions:
static const char *port = DEFAULT_PORT; // the port to bind to
static char *prog_name=NULL;  //the program name
static const char *host_name=DEFAULT_HOST; //the host

// Static variables for resources that should be freed before exiting:
static struct addrinfo *ai = NULL;      // addrinfo struct
static int sockfd = -1;                 // socket file descriptor
//static int connfd = -1;                 // connection file descriptor

int maxfd = 1;
/**
*@brief: This function prints the usage of the program and
*it's called in case of a wrong usage
*/
void print_usage(){
    printf("usage%s [-p PORT] SHIP1 SHIP2 SHIP3 SHIP4 SHIP5 SHIP6\n",prog_name);

}

/**@brief:This function parses command line arguments and it's mandatory to be called.
*It also saves the program in the prog_name variable
 *@details: It also modifies  port from default port if  one is speciffied in arguments
 * and writes the name of the program prog_name  in variable
 *@parameters: argc -number of arguments received by main
 *            *argv[]-list of those arguments
*/
 void  parse_command_line(int argc,char *argv[], char ship_positions[][6]){
    prog_name=(char*) malloc(sizeof(argv[0]));
    if(prog_name==NULL){
        fprintf(stderr,"Memory could not be allocated\n");
        exit(EXIT_FAILURE);
    }
    prog_name=argv[0];
    int iter=1;
    int arg_counter=0;
    int option=getopt(argc,argv,"p:");
    if(option=='p'){
        port=optarg;
        iter=3;
    }
    else if(option=='?'){
        print_usage();
        exit(EXIT_FAILURE);
    }
    for(; iter<argc;iter++){
        strcpy(ship_positions[arg_counter],argv[iter]);
        arg_counter+=1;
    }

    //printf("%d\n",arg_counter);
    if(arg_counter!=6){
        fprintf(stderr,"Not enough coordinates\n");
        print_usage();
        exit(EXIT_FAILURE);
    }

}

/**@brief:Checks the syntax of the ship positions and verifies if the coordinates
* are inside the map
*@parameters: matrix of characters where every row represents a ship coordinate
*The function only iterates through the matrix
*/
void check_sintax_ship_coordinates(char ship_positions[][6]){
    for(int i=0; i<=5; i++){
        if(strlen(ship_positions[i])!=4){
            fprintf(stderr,"Wrong syntax for ship coordinates: %s\n", ship_positions[i]);
            exit(EXIT_FAILURE);
        }
        for(int j=0; j<4; j++){
            if(j%2==0 && !(ship_positions[i][j]>='A' && ship_positions[i][j] <='J')){
                fprintf(stderr,"Coordinates out of the map: %s\n", ship_positions[i]);
                exit(EXIT_FAILURE);
            }
            else if(j%2==1 && !isdigit(ship_positions[i][j])){
                fprintf(stderr,"Coordinates out of the map: %s\n", ship_positions[i]);
                exit(EXIT_FAILURE);
            }
        }
    }

}
/**@brief:function that returns minimum between 2 integers
*/
int min(int a, int b) {
    if (a < b)
        return a;
    return b;
}

/**@brief:checks wether a ship is placed accordingly(either vertically or horizontally)
*@details:returns the ship length is the ship is well placed, exits with error otherwise
*@parameters: a char vector representing a ship's coordinates
*/
int check_ship_placement(char coordinate[]){
    int length=0;
    if(coordinate[0]==coordinate[2]){
        length=abs(coordinate[1]-coordinate[3]) + 1;
    }
    else if(coordinate[1]==coordinate[3]){
        length=abs(coordinate[0]-coordinate[2]) + 1;
    }
    if(length>=2 && length<=4){
        return length;
    }


    fprintf(stderr,"Ships must be aligned either horizontally or vertically and must have length betwen 2 and 4 : %s\n", coordinate);
    exit(EXIT_FAILURE);

}

/**@brief:marks the presence of a ship in the game matrix by placing the ship_nr on the positions
*that are ocuppied by that particular ship
*@parameters:game_matrix a 10x10 matrix full of 0, except the positions that are occupied by the ships
*           length length of the ship we are going to mark as present on the matrix
*           line-line of the first position occupied by the ship
*           col- colomn of the first position occupied by the ship
*           c-'l' if the ship is placed on a single line, 'c' otherwise
*/
void mark_ship(int game_matrix[][10],int length, int line, int col, char c,int ship_nr){
    if(c=='l'){
        for(int i=line; i<line+length;i++){
            game_matrix[i][col]=ship_nr;
        }
        return;
    }
    for(int j=col; j<col+length;j++){
        game_matrix[line][j]=ship_nr;
    }
    //check_if_ship_join_with_another(game_matrix[][10], length, line, col, c,ship_nr);

}
/**@brief:Prints a warning if there are no exactly 1 ship of length 4,3 ships of lenghts 3
*and 2 ships of length 2
*@parameters:vector of integers, where on every position [1,6] we have the number of
* lengths of the battleships
*/
void check_length_of_ships(int ship_status[]){
    int count_vector[6]={0,0,0,0,0,0};
    for(int i=1; i<7; i++){
        count_vector[ship_status[i]]+=1;
    }
    if(count_vector[4]!=1){
        fprintf(stderr,"You should have exactly 1 battleship, not %d",count_vector[4]);
        exit(EXIT_FAILURE);
    }
    if(count_vector[3]!=3){
        fprintf(stderr,"You should have exactly 3 cruisers, not %d",count_vector[3]);
        exit(EXIT_FAILURE);
    }
    if(count_vector[2]!=2){
        fprintf(stderr,"You should have exactly 2 destroyers, not %d",count_vector[2]);
        exit(EXIT_FAILURE);
    }


}

/**@brief:places every ship on the game matrix
*@details:every ship is assigned a ship number, from 1 to 6 and this number is
*written ar every coordinate occupied by that particular ship in the game_matrix
*@parameters:game_matrix a 10x10 matrix full of 0, except the positions that are occupied by the ships
*           ship_position matrix of characters where every row represents a ship coordinate
*           ship_status- vector that stores the unsunk length of a ship, starting from the position 1, to position 6
*           (so that the number assigned in the matrix to every ship to correspond in the ship_status)
*/
void populate_game_matrix(int game_matrix[][10],char ship_position[][6],int ship_status[]){
    int length;
    int line;
    int col;
    int ship_nr=0;
    for(int i=0; i<6; i++){
        length=check_ship_placement(ship_position[i]);
        //printf("1length of ship :%d\n",length);
        ship_nr+=1;
        ship_status[i+1]=length;
        //printf("ship status %d :%d\n",i+1,ship_status[i+1]);
        if(ship_position[i][0]==ship_position[i][2]){
            col=ship_position[i][0]-'A';
            line=min(ship_position[i][1]-'0',ship_position[i][3]-'0');
            mark_ship(game_matrix,length,line,col,'l',ship_nr);
            //ship_status[i]=length;
        }
        else if(ship_position[i][1]==ship_position[i][3]){
            line=ship_position[i][1] - '0';
            col=min(ship_position[i][0]-'A', ship_position[i][2]-'A');
            mark_ship(game_matrix,length,line,col,'c',ship_nr);

        }
    }
    /*for(int i=1;i<7;i++){
        printf("length of ship %d : %d\n",i, ship_status[i]);
    }*/
    //check_length_of_ships(ship_status);

}
/**@details:debugging purpose
*/
void print_matrix(int game_matrix[][10]){
    for(int i=0; i<10; i++){
        for(int j=0; j<10; j++){
            printf("%d",game_matrix[i][j]);
        }
        printf("\n");
    }
}

/**@brief:returns the bits from 0 to 6 as an integer
*@parameters:the message to be decoded by server
*/
 int get_coord(char a){
    char mask=0;
    mask=(1<<7)-1;
    return a&mask;

}
/**@brief:verifies if the coordinates can be found in game_matrix
*@details: returns sum of colomn+10*line if coordinates are valid, 0 otherwise
*@parameters:the message to be decoded by server
*/
int check_valid_coordinates(char a){
    int sum=get_coord(a);
    int col=sum%10;
    int line=sum/10;
    //printf("%d %d\n", col, line);
    if(col>=0 && col<=9 && line>=0 && line <=9){
        return sum;
    }
    return -1;


}
/**@brief:encodes status and hit in 8 bits, where hit is the bits 0 and 1
*and status is the bits 2 and 3
*@details:returns the corresponding byte
*@parameters:hit tells the client if a ship has been hit and also tells if it has been sunk.
*The field can take the following values:
*0-nothing hit
*1-a ship was hit (but not sunk
*2-a ship was hit and sunk, but there are other ships
*3-the last ship was sunk (the client wins)
*       status field notifies the client of the game status and any errors contained in its last message
*0-game is running
*1-game over (all ships sunk or maximum number of rounds reached)
*2-error: The last message contained an invalid parity bit
*3-error: The last message contained an invalid coordinate
*
*/
char create_message_to_send(int status, int hit){
    char aux=0;
    status=status<<2;
    aux=aux|status|hit;
    return aux;

}

/**@brief:returns 0 if there is no unsunk ship, 1 otherwise
*@parameters: a vector of integers of length 7, where on position  1 we have the number of unsunk squares
*of the first ship, on position 2 nd the number of unsunk squares of the 2 nd ship, and so on
*/
int exist_unsunk_ships(int ship_status[]){
    for(int i=1; i<7; i++){
        if(ship_status[i]>0){
            return 1;
        }
    }
    return 0;

}
/**@brief: returns the hit position: 0 if nothing was hit, 1 if a ship was hit and not sunk,
*2 if a ship was hit and sunk,but there are other ships and 3 if the las ship was sunk
*@parameters: coordinates sum=line*10+colomn, game matrix= matrix where free or sunk position
*are marked as 0, and positions tu sunk are marked with integers from 1 to 6
*ship status=vector where we store the number of unsunk squares of the ship on the position[1,6]
*where the index is the numbe of teh ship
*/
int check_ship_hit(int coordinates_sum, int game_matrix[][10],int ship_status[]){
    int col=coordinates_sum%10;
    int line=coordinates_sum/10;
    int ship_nr=game_matrix[line][col];
    /*for(int i=1; i<7; i++){
        printf("ship status %d\n",ship_status[i]);
    }*/
    if(ship_nr==0 || coordinates_sum == -1){
        return 0;
    }
    game_matrix[line][col]=0;
     ship_status[ship_nr]-=1;
    if(ship_status[ship_nr]==0 && exist_unsunk_ships(ship_status)==1){
        return 2;
    }
    if(ship_status[ship_nr]==0 && exist_unsunk_ships(ship_status)==0){
        return 3;
    }
    else{
        return 1;
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

/**@brief: realizes a xor with every bit from [0 6] from the character a,
*and then checks if the result matches the bit on position 7. If it does,
*the parity property is sattisfied, otherwise no
*/
int check_parity(char a){

    int xor_value=get_bit_poz(0,a);

    for(int i=1; i<=6; i++){
        xor_value=xor_value^get_bit_poz(i,a);

    }

    if(get_bit_poz(7,a)==xor_value){
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    char ship_positions[6][6]; // matrix where the coordinates will be saved;every row represents a ship coordinate
    parse_command_line(argc,argv,ship_positions);
    check_sintax_ship_coordinates(ship_positions);
    int ship_status[7]; //vector where we save how many squares a certain ship has (position 1 for the first ship and so on)
    populate_game_matrix(game_matrix,ship_positions,ship_status);
    //print_matrix(game_matrix);
    /*for(int i=0; i<6; i++){
        printf("%d",ship_status[i]);
    }
    printf("\n");*/

	
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int res = getaddrinfo(host_name, port, &hints, &ai);
    if (res != 0) {
        fprintf(stderr, "Getaddrinfo error \n");
        exit(EXIT_FAILURE);
    }

    sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if(sockfd==-1){
        fprintf(stderr,"Could not create socket\n");
        exit(EXIT_FAILURE);
    }


    int val = 1;
    res = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof val);
    if(res!=0){
        fprintf(stderr,"Setsocket error\n");
        exit(EXIT_FAILURE);
    }

    res = bind(sockfd, ai->ai_addr, ai->ai_addrlen);
    if(res!=0){
        fprintf(stderr,"Binding error\n");
        exit(EXIT_FAILURE);
    }

    res = listen(sockfd, 1);
    if(res!=0){
        fprintf(stderr,"Listening error\n");
        exit(EXIT_FAILURE);
    }
	int new;
	int nready; //nr of sockets in list
	fd_set master,copy; //list of file descriptors for the socket
	FD_ZERO(&master); //clears the master list
	FD_ZERO(&copy);
	FD_SET(sockfd, &master) ;//adds the server socket file descriptor to the list
	maxfd = sockfd;
	while(true){
		memcpy(&copy, &master, sizeof(master));		// copy is "copy" the info from master
		printf("running select()\n");
		nready = select(maxfd+1, &copy, NULL, NULL, NULL);
		if(nready == -1){
			fprintf(stderr, "Error return of select()\n");
			exit(EXIT_FAILURE);
		}
		printf("Nr of ready descriptor %d\n", nready);
		for(int i = 0; i<=maxfd && nready>0; i++){ //iterate through file descriptors

			if(FD_ISSET(i, &copy)){ // if the file descriptor is in the set
				nready-=1;
				if(i ==sockfd){
					printf("Trying to accept() new connection(s)\n");
					new = accept(sockfd, NULL, NULL); //returns the file descriptor of the first client in queue
					if(new == -1){
						fprintf(stderr, "Accepting a new client failed\n");
						exit(EXIT_FAILURE);
					}
					else{
						if (-1 == (fcntl(new, F_SETFD, O_NONBLOCK))){
							fprintf(stderr, "fcntl stuff\n");
							exit(EXIT_FAILURE);
						}
						FD_SET(new, &master); //add new descriptor in master list
						if(maxfd<new){
							maxfd = new;
						}
					}
					
				}
				else{
					printf("recv() data from one of descriptors(s)\n");
					char buffer; //bufer where we write the messages and where we store the messages to send
    				int bytes_received=0; //will take the value of bits received by rcv()
    				int coordinates_sum; //line*10+colomn
   					int hit;//hit tells the client if a ship has been hit and also tells if it has been sunk.
   					int status=0; //status 0=game running
					bytes_received=recv(i,&buffer,1,0);
       				if(bytes_received==-1){
            			fprintf(stderr,"Conection error\n");
           				 exit(EXIT_FAILURE);
        			}
					if(check_parity(buffer)==0){
         			   status=2; // Error: The last message contained an invalid parity bit
        			}
        			coordinates_sum=check_valid_coordinates(buffer);
        			if(coordinates_sum==-1){ //3-Error: The last message contained an invalid coordinate
            			status=3;
        			}
        			hit=check_ship_hit(coordinates_sum,game_matrix,ship_status);
        			if(hit==3 ){ //adauga conditie pt nr maxim de mutari
            			status=1; // if the las ship was sunk, or it was the last round, status= game over
						
        			}
        			buffer=create_message_to_send(status,hit);
        			//printf("Message to be sent by server %d\n",buffer);
       				int written_bits=write(i,&buffer,sizeof(buffer));
        			if(written_bits==-1){
            			fprintf(stderr,"Could not write in the socket\n");
            			exit(EXIT_FAILURE);
        			}
					if(status == 1){
						close(i);
						FD_CLR(i, &master);
					}
					
				}
					
			}
		
		}
	}	

	return 0;
   /* connfd = accept(sockfd, NULL, NULL);
    if(connfd==-1){
        fprintf(stderr,"Accepting error\n");
        exit(EXIT_FAILURE);
    }

    char buffer; //bufer where we write the messages and where we store the messages to send
    int bytes_received=0; //will take the value of bits received by rcv()
    int coordinates_sum; //line*10+colomn
    int hit;//hit tells the client if a ship has been hit and also tells if it has been sunk.
    int status=0; //status 0=game running
    for(int i=0; i<80; i++){
        printf("Runda %d\n",i);
        bytes_received=recv(connfd,&buffer,1,0);
        if(bytes_received==-1){
            fprintf(stderr,"Conection error\n");
            exit(EXIT_FAILURE);
        }
        if(check_parity(buffer)==0){
            status=2; // Error: The last message contained an invalid parity bit
        }
        coordinates_sum=check_valid_coordinates(buffer);
        if(coordinates_sum==-1){ //3-Error: The last message contained an invalid coordinate
            status=3;
        }
        hit=check_ship_hit(coordinates_sum,game_matrix,ship_status);
        if(hit==3 || i==79){
            status=1; // if the las ship was sunk, or it was the last round, status= game over
        }
        buffer=create_message_to_send(status,hit);
        //printf("Message to be sent by server %d\n",buffer);
        int written_bits=write(connfd,&buffer,sizeof(buffer));
        if(written_bits==-1){
            fprintf(stderr,"Could not write in the socket\n");
            exit(EXIT_FAILURE);
        }

        if(status==1){ //we make sure that we go out of the loop if the status becomes 1 in the last step
            break;
        }
    }
    //free(prog_name);
    close(sockfd);*/

}
