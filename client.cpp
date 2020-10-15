//Author : KIRAN THAPA

#include <iostream>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <time.h>
#include "packet.h"
#include "packet.cpp"
#include <vector>
#include <sys/time.h>
#include <typeinfo>
#include <string>
#include <cstring>

void printUsage();
int hostnameToIp(char *hostname , char *ip);

int main(int argc, char* argv[]){

    if (argc < 4){
        printUsage();
        exit(EXIT_FAILURE);
    }
    //create a UDP socket 
    int sockfd;
    int PORT = atoi(argv[2]);
    char *hostname = argv[1];
    char IP[100];
    struct sockaddr_in serverAddr;
    
    //convert the hostname to network understandable  IP address
    hostnameToIp(hostname , IP);

    //create udp socket file descriptor
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        fprintf(stderr, "Error: Failed creating socket \n");
        exit(EXIT_FAILURE);
    }

    //clear serverAddr structure before filling the actual information
    memset(&serverAddr, 0, sizeof serverAddr);

    //Filling server Information
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr =  INADDR_ANY;

    //create a timeout for udp socket
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        fprintf(stderr, "Error: Failed to set timeout options \n");
        exit(EXIT_FAILURE);
    }

    //read the text from file and start building packets to send
    fstream readFile(argv[3]);
    char ch;
    int payloadIndex = 0;
    char payload[30];
    int type = 1;
    int seqnum = 0;

    //clean payload 
    memset(&payload, 0, 30);

    int windowSize = 0;
    vector<string> fileData;

    while (readFile >> noskipws >> ch)
    {
        payload[payloadIndex] = ch;
        payloadIndex++;
        if (payloadIndex == 30 || readFile.peek() == EOF){
            //push the payload data to fileData
            fileData.push_back(payload);

            //clear payload and reset payloadIndex
            payloadIndex = 0;
            memset(&payload, 0, 30);

        }
    }

    int totalData = fileData.size();
    int startWindow = 0;
    int endWindow = 7;
    int current = 0;
    int ackSeqnum = 0;

    //create File
	char clientSeqFilename [] = "clientseqnum.log";
    char clientAckFilename [] = "clientack.log";

	fstream clientSeqnumFile;
    fstream clientAckFile;
	
    clientSeqnumFile.open(clientSeqFilename, std::ofstream::out | std::ofstream::trunc);
    clientAckFile.open(clientAckFilename, std::ofstream::out | std::ofstream::trunc);
	//if the  file does not exit create a new one
	if (!clientSeqnumFile)
	{
		clientSeqnumFile.open(clientSeqFilename, ios::app);
	}

    if (!clientAckFile)
	{
		clientAckFile.open(clientAckFilename, ios::app);
	}

    while (true){
        //#TOP
        TOP:
        while(windowSize < endWindow){
             char data[fileData[current].size() +1];
             std::copy(fileData[current].begin(), fileData[current].end(), data);
             if(current + 1 == totalData){
                type = 3;
            }

            //instantiate packet object
            packet clientPacket(type, seqnum, fileData[current].length(), data);
            char *spacket = new char[strlen(data)]; // TODO delete the spacket from heap once done with it
            
            //serialize the packet to send it to server
            clientPacket.serialize(spacket);
                
            sendto(sockfd, (const char*)spacket, strlen(spacket), MSG_CONFIRM, (const struct sockaddr *)&serverAddr, sizeof serverAddr);

            //write received data to file
	        clientSeqnumFile.seekg(0, std::ios::end);
	        clientSeqnumFile << seqnum << std::endl;
            //increment seqnum if less than our window size and if windowSize is max i.e 7 RESET windowsize
            if (seqnum == 7){
                seqnum = 0;
            }else{
                seqnum++;
            }
            //increment the windowSize by One
            current++;
            windowSize++;
        }
        //sstart ack if correct acks ,GoTO TOP and send one DATA as we got one ACK {send next current = endWindow,  fileData[endWindow + 1] => endWindow += 1, startWindow += 1}
        //if not ack , GOTO TOP  => {current = startWindow}
        char ackBuffer[8];
        socklen_t serverLen;
        int receivedAckLen;
        serverLen = sizeof(serverAddr);
        ACKS:
        if (receivedAckLen = recvfrom(sockfd, (char *)ackBuffer, 8, MSG_WAITALL, (struct sockaddr*)&serverAddr, &serverLen) > 0){
            ackBuffer[5] = '\0';
            int intAckSeqnum = ackBuffer[2] - 48;
            int intAckType = ackBuffer[0] - 48;
            if (intAckSeqnum != ackSeqnum){
                //std::cout << "Error: Wrong Acks sequence Number, resend the all the unACKs packets and GOTO TOP" << std::endl;
                current = startWindow;
                goto TOP;
            }

            //write received data to file
	        clientAckFile.seekg(0, std::ios::end);
	        clientAckFile << intAckSeqnum << std::endl;

            current = endWindow;
            endWindow += 1;
            startWindow += 1;

            //if EOF ack received break
            if (intAckType == 2){
                //std::cout << "received EOF" << std::endl;
                break;
            }
        
            if (ackSeqnum == 7){
                ackSeqnum = 0;
            }else{
                ackSeqnum++;
            }

           if (startWindow + 1 == totalData){
               goto ACKS;
           }else{
                goto TOP;
           }
               
            // std::cout << "Received ACKS , now GOTO TOP to send once more packet" << std::endl;
        }else{
           // std::cout << "Did not receive ACKS and Now GOTO TOP" << std::endl;
            //std::cout << "current : " << current << " windowSize:  " << windowSize << " startWindow: " << startWindow << " endWindow: " << endWindow << std::endl;
            current = startWindow;
            windowSize = startWindow;
            //std::cout << "current : " << current << " windowSize:  " << windowSize << " startWindow: " << startWindow << " endWindow: " << endWindow << std::endl;
         
            goto TOP;
        }
       
    }

    //close the inputfile file descriptor
    readFile.close();
    clientSeqnumFile.close();
    clientAckFile.close();
   
    close(sockfd);
    return 0;
}

//print usage if wrong arguments is passed to command line arguments
void printUsage(){
    std::cout << "Usage: ./client IPAddress Port fileName" << std::endl;
}

//convert the hostname to network understadable network IP address
int hostnameToIp(char * hostname , char* ip)
{
	struct hostent *host;
	struct in_addr **addr_list;
	int i;
		
	if ( (host = gethostbyname( hostname ) ) == NULL) 
	{
		// get the host info
		herror("gethostbyname");
		return 1;
	}

	addr_list = (struct in_addr **) host->h_addr_list;
	
	for(i = 0; addr_list[i] != NULL; i++) 
	{
		//Return the first one;
		strcpy(ip , inet_ntoa(*addr_list[i]) );
		return 0;
	}
	
	return 1;
}