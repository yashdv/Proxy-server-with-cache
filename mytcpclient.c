/* tcpclient.c */

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>


int main()

{

	int sock, bytes_recieved;
	int conditional;
	char ch;
	char date[1024];
	char command[1024];
	char send_data[1024],recv_data[1024];
	char url[1024];
	struct hostent *host;
	struct sockaddr_in server_addr;  

	//host = gethostbyname("10.1.39.205");
	        host = gethostbyname("127.0.0.1");

	while(1)
	{
		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			perror("Socket");
			exit(1);
		}

		server_addr.sin_family = AF_INET;     
		server_addr.sin_port = htons(5000);   
		server_addr.sin_addr = *((struct in_addr *)host->h_addr);
		bzero(&(server_addr.sin_zero),8); 

		if (connect(sock, (struct sockaddr *)&server_addr,
					sizeof(struct sockaddr)) == -1) 
		{
			perror("Connect");
			exit(1);
		}
		printf("Enter 'c' to type commands or 'r' to send requests: ");
		scanf("%c",&ch);
		if(ch == 'c'){
			printf("\nEnter command: ");
			getchar();
			gets(send_data);
		}
		else{
			printf("\nEnter 1 for a regular GET request and 2 for a conditional GET request: ");
			scanf("%d",&conditional);

			if(conditional == 2)
				printf("\nFor a conditional GET request enter the date in the following format: day, date month year hh:mm:ss GMT\n");

			printf("\nEnter the URL: ");
			scanf("%s",url);
			getchar();

			if(conditional == 2){
				printf("Enter the Last-Modified Date: ");
//				getchar();
				gets(date);
			}

			char req_part1[11] = "GET http://";
			char req_part2[10] = " HTTP/1.1";
			char req_part3[10] = "Host: ";
			char req_part4[100] = "If-Modified-Since: "; //Sun, 20 Sep 2009 22:54:51 GMT";

			strcpy(send_data,req_part1);
			strcat(send_data,url);
			strcat(send_data,req_part2);
			strcat(send_data,"\r\n");
			strcat(send_data,req_part3);
			strcat(send_data,url);
			if(conditional == 2){
				strcat(send_data,"\r\n");
				strcat(send_data,req_part4);
				strcat(send_data,date);
			}
			strcat(send_data,"\r\n\r\n");
		}
		printf("Request: %s\n",send_data);

		send(sock,send_data,strlen(send_data), 0);

		while(1){
			bytes_recieved=recv(sock,recv_data,1024,0);
			recv_data[bytes_recieved] = '\0';
			if(bytes_recieved <= 0)
				break;
			printf("%s " , recv_data);
			printf("\n_________%d__________\n",bytes_recieved,recv_data[bytes_recieved-1]);
		}

		close(sock);

	}   
	return 0;
}
