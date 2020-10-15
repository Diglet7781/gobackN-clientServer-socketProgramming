//Author : KIRAN THAPA

#include <iostream>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <arpa/inet.h>
#include <fstream>
#include "packet.h"
#include "packet.cpp"
#include <unistd.h>


void printUsage();
int main(int argc, char* argv[]){

    if (argc < 3){
        printUsage();
        exit(EXIT_FAILURE);
    }

    //open a UDP socket port and listen for data
    int sockfd;
    sockaddr_in serverAddr, clientAddr; //structure to hold server and client information
    int udpPort = atoi(argv[1]);
    //char * outputFilename = argv[2];

    //create udp socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        fprintf(stderr, "Error: Failed creating socket \n");
        exit(EXIT_FAILURE);
    }

    //clear server and client structure before filling the actual information
    memset(&serverAddr, 0, sizeof serverAddr);
    memset(&clientAddr, 0, sizeof clientAddr);

    //Fill the server information
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(udpPort);

    //bind the socket with the server address
    int bindSocket = bind(sockfd, (const struct sockaddr*) &serverAddr, sizeof serverAddr);
    if (bindSocket < 0){
        fprintf(stderr, "Error: Failed binding socket \n");
        exit(EXIT_FAILURE);
    }

    //create File to store dataReceived
	char *outputFilename = argv[2];
    char arrivalFilename [] = "arrival.log";

	fstream outputfile;
    fstream arrivalfile;
	
    outputfile.open(outputFilename, std::ofstream::out | std::ofstream::trunc);
    arrivalfile.open(arrivalFilename, std::ofstream::out | std::ofstream::trunc);
	//if the  file does not exit create a new one
	if (!outputfile)
	{
		outputfile.open(outputFilename, ios::app);
	}

    if (!arrivalfile)
	{
		arrivalfile.open(arrivalFilename, ios::app);
	}


    int receivedLen;
    socklen_t clientLen;
    clientLen = sizeof(clientAddr);
    int dataCount = 0;
    int type = 1;
    int expectedSeqnum = 0;
    while (type != 3){
        char buffer[37];
        //receive the packet from the client
        receivedLen = recvfrom(sockfd, (char *)buffer, 37, MSG_WAITALL, (struct sockaddr*)&clientAddr, &clientLen);
        buffer[receivedLen] = '\0';
 
        //instantiate the packet object and deserailise the data
        packet serverPacket(buffer[0], buffer[2], buffer[4], buffer);

        //Deserialize received packet 
        serverPacket.deserialize(buffer);

        //std::cout << "REceived DATA ........" << std::endl; 
        //Test if the received data is correct or not
        //serverPacket.printContents();
        
        //if receivedPacket seqnum match the expectedSeqnum
        if (serverPacket.getSeqNum() == expectedSeqnum && serverPacket.getType() == 1){
            //set type = 0, length = 0, data = NULL and send the ack, seqnum = expectedSeqnum, update expectedSeqnum += 1, send Ack
            
            //instantiate packet object
            packet serverAckPacketToClient(0, expectedSeqnum, 0, serverPacket.getData());
            char *ackSpacket = new char[4]; // TODO delete the spacket from heap once done with it
            
            //serialize the packet to send it to server
            serverAckPacketToClient.serialize(ackSpacket);
                
            sendto(sockfd, (const char*)ackSpacket, strlen(ackSpacket), MSG_CONFIRM, (const struct sockaddr *)&clientAddr, sizeof clientAddr);
            
            //std::cout << "Here we send an ACK" << std::endl;

            if(expectedSeqnum == 7){
                expectedSeqnum = 0;
            }else{
                expectedSeqnum++;
            }

            //write received data to file
	        outputfile.seekg(0, std::ios::end);
	        outputfile << serverPacket.getData();
            arrivalfile.seekg(0,  std::ios::end);
            arrivalfile << serverPacket.getSeqNum() << std::endl;
        }

        //if EOF packet then break
        if(serverPacket.getType() == 3){
            //send type = 2, seqnum = expectedSeqnum, length = 0, data = NULL
            type = 3;
            //instantiate packet object
            packet clientPacket(2, expectedSeqnum, 0, NULL);
            char *spacket = new char[10]; // TODO delete the spacket from heap once done with it
            
            //serialize the packet to send it to server
            clientPacket.serialize(spacket);
            outputfile.seekg(0, std::ios::end);
	        outputfile << serverPacket.getData();
            arrivalfile.seekg(0,  std::ios::end);
            arrivalfile << serverPacket.getSeqNum() << std::endl; 
            sendto(sockfd, (const char*)spacket, strlen(spacket), MSG_CONFIRM, (const struct sockaddr *)&clientAddr, sizeof clientAddr);
           
            break;
        }
    }


    //close files
    outputfile.close();
    arrivalfile.close();
    close(sockfd);

    return 0;
}

//print usage if wrong arguments is passed to command line arguments
void printUsage(){
    std::cout << "Usage: ./server Port fileName" << std::endl;
}
