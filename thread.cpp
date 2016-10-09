#include<iostream>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<sys/wait.h>
#include<signal.h>
#include<pthread.h>
#include<semaphore.h>
#include<time.h>
#include<map>
#include<set>
#include<queue>
#include<utility>
#include<algorithm>
#include<string>
#include<sstream>
#include<fstream>
using namespace std;
#define PORT "5000"
#define BACKLOG 20

sem_t semtotal,semhit,semmiss,semlm,semfnum,semcachecnt,sem_m,semurl2t,semS,semurl2sz;
int FNUM = 1, HIT = 0, MISS = 0, TOTAL = 0, CACHECNT = 0, POLICY;

map< string,int > m;
map< int,string > mrev;
map< int,string> lastModi;

set< pair<int,int> > S;
map< int,int > url2time;
set< pair<int,int> > SS;
map< int,int > url2size;

ofstream log("Logfile");

void *get_in_addr(struct sockaddr *sa)	 // get sockaddr, IPv4 or IPv6:
{
	if (sa->sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa)->sin_addr);
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int str2num(char *s)
{
	int ans = 0;
	while(*s)
	{
		ans = 10*ans + *s - '0';
		s++;
	}
	return ans;
}

char *num2str(int x)
{
	char *y;
	int t = x,len = 0;
	while(t > 0)
	{
		len++;
		t /= 10;
	}
	y = (char *)malloc((len+2)*sizeof(char));
	y[len] = '\0';
	t = x;
	while(t > 0)
	{
		y[--len] = '0' + t%10;
		t /= 10;
	}
	return y;
}

int file_length(FILE *f)
{
	int pos,end;
	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);
	return end;
}

long int date2sec(string &s)
{
	char buf[1000];
	int l = s.size();
	for(int i=0; i<l; i++)
		buf[i] = s[i];
	buf[l] = '\0';
	struct tm tval;
	memset(&tval,0,sizeof(tval));
	char *p;
	if((p=strptime(buf,"%A, %d %b %Y %H:%M:%S %z",&tval))!=NULL){
		printf("Some Error\n");
	}
	strftime(buf,sizeof(buf),"%A, %d %b %Y %H:%M",&tval);
	//printf("%s\n",buf);
	//printf("%ld seconds since epoch \n",(long)mktime(&tval));
	return ((long)mktime(&tval));
}

void updatelastModi(string &s,int fnameHash, int a, int b)
{
	sem_wait(&semlm);
	lastModi[fnameHash] = s.substr(a,b);
	sem_post(&semlm);
}

int lookForLastModified(int fnameHash, char *data, int gotitFlag)
{
	if(gotitFlag == 1)
		return 1;
	
	string s(data);
	size_t found, foundGMT;
	if((found = s.find("Last-Modified:")) != string::npos)
	{
		found += 15;
		if((foundGMT = s.find("GMT",found)) != string::npos)
			updatelastModi(s,fnameHash,found,foundGMT-found + 3);
		else
			printf("Error in Last Modified date\n");
		return 1;
	}

	if((found = s.find("Date:")) != string::npos)
	{
		found += 6;
		if((foundGMT = s.find("GMT",found)) != string::npos)
			updatelastModi(s,fnameHash,found,foundGMT-found + 3);
		else
			printf("Error in Last Modified date\n");
		return 1;
	}
	return 0;
}

int lookFornocache(char *data, int nocacheflag)
{
	if(nocacheflag != -1)
		return nocacheflag;
	
	string s(data);
	size_t f1 = s.find("private");
	size_t f2 = s.find("no-cache");

	if(f1!=string::npos || f2!=string::npos)
		return 1;
	return 0;
}

int nextserver(string &host,const char *client_data,int new_fd,char *fname)
{
	cout << "inside " << fname << endl;
	int sock_proxy, bytes_recieved, rv;
	struct addrinfo hints, *servinfo;
	char recv_serverdata[1024], file[100],hostname[100];
	strcpy(file,"ProxyServerCache/");

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	int l = host.size();
	for(int i=0; i<l; i++)
		hostname[i] = host[i];
	hostname[l] = '\0';

	if ((rv = getaddrinfo("proxy.iiit.ac.in", "8080", &hints, &servinfo)) != 0) 
	{	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));	exit(1); 	}
	if ((sock_proxy = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
	{ 	perror("Socket");	exit(1); 	}   
	if (connect(sock_proxy, servinfo->ai_addr,servinfo->ai_addrlen) == -1) 
	{  	perror("Connect");	exit(1);  }


	if(send(sock_proxy,client_data,strlen(client_data), 0) == -1)	perror("SEND");

	strcat(file,fname);
	FILE *fp = fopen(file,"w");
	int fnameHash = str2num(fname), gotitFlag = 0, nocacheflag = -1;

	while(1)
	{
		cout  << "loop" << endl;
		if((bytes_recieved = recv(sock_proxy,recv_serverdata,1024,0)) == -1)	perror("recv");
		cout << bytes_recieved << endl;

		if(bytes_recieved <= 0)	break;

		gotitFlag = lookForLastModified(fnameHash,recv_serverdata,gotitFlag);
		nocacheflag = lookFornocache(recv_serverdata,nocacheflag);

		printf("\nRECIEVED DATA FROM SERVER\n%s\n", recv_serverdata);fflush(stdout);
		if(nocacheflag != 1)
			fwrite(recv_serverdata, bytes_recieved, 1, fp);

		int amt_sent = 0, curr_sent = 0;
		while(amt_sent < bytes_recieved)
		{
			if((curr_sent = send(new_fd, recv_serverdata + amt_sent, bytes_recieved - amt_sent, 0)) == -1) { perror("send"); }
			amt_sent += curr_sent;		
		}
	}
	cout << "bye " << pthread_self() << endl;
	recv_serverdata[0] = '\0';
	strcpy(recv_serverdata,"\n Percentage Cache Hit : ");
	strcat(recv_serverdata,num2str(HIT*100/TOTAL));
	strcat(recv_serverdata,"\n Percentage Cache Miss : ");
	strcat(recv_serverdata,num2str(MISS*100/TOTAL));
	
	if(nocacheflag != 1){
		int amt_sent = 0, curr_sent = 0;
		bytes_recieved = strlen(recv_serverdata);
		while(amt_sent < bytes_recieved)
		{
			if((curr_sent = send(new_fd, recv_serverdata + amt_sent, bytes_recieved - amt_sent, 0)) == -1) { perror("send"); }
			amt_sent += curr_sent;		
		}
	}
	else if(nocacheflag == 1)
	{
		int popped = fnameHash;
		sem_wait(&sem_m);
		m.erase(mrev[popped]);
		mrev.erase(popped);
		sem_post(&sem_m);

		if(POLICY == 1){
			sem_wait(&semS);
			S.erase(S.find(make_pair(url2time[popped],popped)));
			sem_post(&semS);

			sem_wait(&semurl2t);
			url2time.erase(popped);
			sem_post(&semurl2t);
		}

		sem_wait(&semlm);
		lastModi.erase(popped);
		sem_post(&semlm);

		char temp[1000];
		cout << "~~~~~~~~ rm was at next server" << endl;
		sprintf(temp,"rm ProxyServerCache/%d",popped);
		system(temp);
	
		sem_wait(&semcachecnt);
		CACHECNT--;
		sem_post(&semcachecnt);
	}

	close(sock_proxy);
	close(new_fd);
	fclose(fp);
	return nocacheflag;
}

void miss_mechanism(string &host,char *arg,int new_fd,string &path,int local_time)
{	
	int fnum;
	
	sem_wait(&semfnum);
	fnum = FNUM;
	FNUM++;
	sem_post(&semfnum);
	
	char fname[100];
	log << path << "::contacted origin server" << endl;

	string str(arg);
	size_t found;
	if((found = str.find("If-Modified-Since:")) != string::npos)
	{
		arg[found] = '\r';
		arg[found+1] = '\n';
		arg[found+2] = '\r';
		arg[found+3] = '\n';
		arg[found+4] = '\0';
		cout << arg << endl;
	}

	sem_wait(&semmiss);
	MISS++;
	sem_post(&semmiss);
	
	strcpy(fname,num2str(fnum));
	sem_wait(&sem_m);
	m[path] = fnum;
	mrev[fnum] = path;
	sem_post(&sem_m);

	if(POLICY == 1){
		sem_wait(&semS);
		S.insert(make_pair(local_time,fnum));
		sem_post(&semS);

		sem_wait(&semurl2t);
		url2time[fnum] = local_time;
		sem_post(&semurl2t);
	}
	
	int nocacheflag = nextserver(host,arg,new_fd,fname);
	
	if(POLICY == 2 && nocacheflag != 1){
		char temp[100];
		sprintf(temp,"ProxyServerCache/%d",fnum);
		int sz = file_length(fopen(temp,"r"));
		sem_wait(&semS);
		SS.insert(make_pair(-sz,fnum));
		sem_post(&semS);
		
		sem_wait(&semurl2sz);
		url2size[fnum] = sz;
		sem_post(&semurl2sz);
	}
	
	sem_wait(&semcachecnt);
	CACHECNT++;
	if(CACHECNT > 5)
	{
		int popped;
		if(POLICY == 1)
			popped = S.begin()->second;
		else if(POLICY == 2)
			popped = SS.begin()->second;
		string url = mrev[popped];

		if(POLICY == 1){
			sem_wait(&semurl2t);
			url2time.erase(popped);
			sem_post(&semurl2t);

			sem_wait(&semS);
			S.erase(S.begin());
			sem_post(&semS);
		}

		if(POLICY == 2){
			sem_wait(&semurl2sz);
			url2size.erase(popped);
			sem_post(&semurl2sz);

			sem_wait(&semS);
			SS.erase(SS.begin());
			sem_post(&semS);
		}
		
		sem_wait(&semlm);
		lastModi.erase(popped);
		sem_post(&semlm);

		CACHECNT--;

		sem_wait(&sem_m);
		m.erase(url);
		mrev.erase(popped);
		sem_post(&sem_m);
		
		char temp[1000];
		cout << "~~~~~~~~ rm was at miss-mech" << endl;
		sprintf(temp,"rm ProxyServerCache/%d",popped);
		system(temp);
	}
	sem_post(&semcachecnt);
}

void hit_mechanism(string &host,char *arg,int new_fd,string &path,int local_time)
{
	string str(arg);
	size_t found,foundGMT;
	if((found = str.find("If-Modified-Since:")) != string::npos)
	{
		long int date,cachedate;
		found += 19;
		if((foundGMT = str.find("GMT",found)) != string::npos)
		{
			string t = str.substr(found,foundGMT-found + 3);
			date = date2sec(t);
			string t2 = lastModi[m[path]];
			cachedate = date2sec(t2);
			if(cachedate < date)
			{
				int popped = m[path];
				sem_wait(&sem_m);
				m.erase(path);
				mrev.erase(popped);
				sem_post(&sem_m);

				if(POLICY == 1){
				sem_wait(&semS);
				S.erase(S.find(make_pair(url2time[popped],popped)));
				sem_post(&semS);

				sem_wait(&semurl2t);
				url2time.erase(popped);
				sem_post(&semurl2t);
				}

				if(POLICY == 2)
				{
					sem_wait(&semS);
					SS.erase(SS.find(make_pair(-url2size[popped],popped)));
					sem_post(&semS);

					sem_wait(&semurl2sz);
					url2size.erase(popped);
					sem_post(&semurl2sz);
				}

				sem_wait(&semlm);
				lastModi.erase(popped);
				sem_post(&semlm);

				sem_wait(&semcachecnt);
				CACHECNT--;
				sem_post(&semcachecnt);
				
				char temp[1000];
				cout << "~~~~~~~~ rm was at hit-mech" << endl;
				sprintf(temp,"rm ProxyServerCache/%d",popped);
				system(temp);

				miss_mechanism(host,arg,new_fd,path,local_time);
				return;
			}
		}
	}

	sem_wait(&semhit);
	HIT++;
	sem_post(&semhit);

	int cachefilename = m[path];

	if(POLICY == 1){
		sem_wait(&semS);
		S.erase(S.find(make_pair(url2time[cachefilename],cachefilename)));
		S.insert(make_pair(local_time,cachefilename));
		sem_post(&semS);

		sem_wait(&semurl2t);
		url2time[cachefilename] = local_time;
		sem_post(&semurl2t);
	}

	char fname[100], buff[1024], recv_serverdata[1024];
	strcpy(fname,"ProxyServerCache/");
	strcat(fname,num2str(m[path]));
	FILE *fp = fopen(fname,"r");

	log << path << "::" << fname << " " << file_length(fp) << endl;
	cout << "HIT FILE NAME" << fname << endl; 

	while(1)
	{
		int maxx = fread(buff,1,1024,fp);
		cout << "MAXX " << maxx <<  endl;
		int amt_sent = 0, curr_sent = 0;
		while(amt_sent < maxx)
		{
			if((curr_sent = send(new_fd, buff + amt_sent,maxx - amt_sent, 0)) == -1) { perror("send"); }
			amt_sent += curr_sent;		
		}
		if(maxx < 1024)
			break;
	}
	cout << "bye " << pthread_self() << endl;

	recv_serverdata[0] = '\0';
	strcpy(recv_serverdata,"\n Percentage Cache Hit : ");
	strcat(recv_serverdata,num2str(HIT*100/TOTAL));
	strcat(recv_serverdata,"\n Percentage Cache Miss : ");
	strcat(recv_serverdata,num2str(MISS*100/TOTAL));

	int amt_sent = 0, curr_sent = 0;
	int bytes_recieved = strlen(recv_serverdata);
	while(amt_sent < bytes_recieved)
	{
		if((curr_sent = send(new_fd, recv_serverdata + amt_sent, bytes_recieved - amt_sent, 0)) == -1) { perror("send"); }
		amt_sent += curr_sent;		
	}
	close(new_fd);
	fclose(fp);
}

void cache_mechanism(char *arg,int new_fd)
{
	int local_time;
	sem_wait(&semtotal);
	TOTAL++;
	local_time = TOTAL;
	sem_post(&semtotal);
	
	string host,path,data(arg,strlen(arg));
	istringstream iss(data);
	iss >> path; iss >> path;
	iss >> host;
	while(host != "Host:")
		iss >> host;
	iss >> host;
	
	if(path.find("http") == string::npos)
		path = host + "/" + path;
	else
		path = path.substr(7,path.size()-7);
	cout << "PATH ~~~~~~~" << path << "~~~~~~~~~" << endl;

	if(m.find(path) == m.end())
	{
		cout << "CACHE MISS" << endl;
		miss_mechanism(host,arg,new_fd,path,local_time);
	}
	else
	{
		cout << "CACHE HIT" << endl;
		hit_mechanism(host,arg,new_fd,path,local_time);
	}
}

void *AsClient(void *arg)
{
	char client_data[10240];
	int new_fd = (int)arg, bytes_recieved;

	if((bytes_recieved = recv(new_fd,client_data,10240,0)) == -1)   perror("recv");
	printf("RECIEVED DATA FROM CLIENT\n%s\n", client_data);fflush(stdout);
	if(strcmp(client_data,"Print Cache") == 0 || strcmp(client_data,"Print Log") == 0 || strncmp(client_data,"Search",6) == 0){

		if(strcmp(client_data,"Print Log") == 0){
			char buff[1024];
			FILE *fp = fopen("Logfile","r");
			while(1)
			{
				int maxx = fread(buff,1,1024,fp);
				cout << buff << endl;
				int amt_sent = 0, curr_sent = 0;
				while(amt_sent < maxx)
				{
					if((curr_sent = send(new_fd, buff + amt_sent,maxx - amt_sent, 0)) == -1) { perror("send"); }
					amt_sent += curr_sent;
				}
				if(maxx < 1024)
					break;
			}
			fclose(fp);
		}
		else if(strcmp(client_data,"Print Cache") == 0){
			char cache[1024],temp[1024];
			cout << "~~~~~ printing Cache ~~~~~~~" << endl;
			for(map<string,int>::iterator it = m.begin(); it != m.end(); it++)
			{
				cout << it->first << " " << it->second << endl;
				int l = (it->first).size();
				for(int i=0; i<l; i++)
					temp[i] = (it->first)[i];
				temp[l] = '\0';
				sprintf(cache,"%s ProxyServerCache/%d\n",temp,it->second);
				send(new_fd,cache,strlen(cache),0);
			}
		}
		else if(strncmp(client_data,"Search",6) == 0){ 
			char key[1024],temp[1024];
			string data(client_data);
			data = data.substr(7,data.size()-7);
			cout << "~~~~~ printing search ~~~~~~~" << endl;
			for(map<string,int>::iterator it = m.begin(); it != m.end(); it++)
			{   
				if (it->first.find(data) != string::npos) {
					cout << it->first << " " << it->second << endl;
					int l = (it->first).size();
					for(int i=0; i<l; i++)
						temp[i] = (it->first)[i];
					temp[l] = '\0';

					sprintf(key,"%s\n",temp,it->second);
					send(new_fd,key,strlen(key),0);
				}   
			}   
		}   
		close(new_fd);
	}
	else
	{
		cache_mechanism(client_data,new_fd);
	}
}
int main(void)
{
	cout << "Enter Cache Replacement Policy : " << endl << "1 => The oldest object in the cache" << endl << "2 => The largest object in the cache" << endl;
	cin >> POLICY;
	sem_init(&semtotal,0,1);
	sem_init(&semhit,0,1);
	sem_init(&semmiss,0,1);
	sem_init(&semlm,0,1);
	sem_init(&semfnum,0,1);
	sem_init(&semcachecnt,0,1);
	sem_init(&semurl2t,0,1);
	sem_init(&semurl2sz,0,1);
	sem_init(&sem_m,0,1);
	sem_init(&semS,0,1);
	int sockfd,new_fd,rv,yes = 1 ;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // connector's address information
	socklen_t sin_size;
	struct sigaction sa;
	char s[INET6_ADDRSTRLEN];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0)
	{	fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));		return 1;	}

	// loop through all the results and bind to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) 
		{	perror("server: socket");	continue;		}
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,sizeof(int)) == -1)
		{		perror("setsockopt");	exit(1);	}
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
		{		close(sockfd);		perror("server: bind");		continue;	}
		break;
	}

	if (p == NULL) 
	{	fprintf(stderr, "server: failed to bind\n");	return 2;	}

	freeaddrinfo(servinfo); // all done with this structure

	if (listen(sockfd, BACKLOG) == -1) 
	{	perror("listen");	exit(1);	}
	printf("server: waiting for connections...\n");

	int cnt = 0;
	pthread_t th[30000];
	while(1) 
	{
		cout << "********************************************** parent here **************************************************" << endl;
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd == -1) {
			perror("accept");
			continue;
		}
		inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr),s, sizeof s);
		printf("server: got connection from %s\n", s);

		pthread_create(&th[cnt],NULL,AsClient,(void *)new_fd);
		cnt = (cnt + 1)%30000;
	}
	sem_destroy(&semtotal);
	sem_destroy(&semhit);
	sem_destroy(&semmiss);
	sem_destroy(&semlm);
	sem_destroy(&semfnum);
	sem_destroy(&semcachecnt);
	sem_destroy(&semurl2t);
	sem_destroy(&semurl2sz);
	sem_destroy(&sem_m);
	sem_destroy(&semS);
	log.close();
	return 0;
}

