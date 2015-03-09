#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/select.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <math.h>
#include "my402list.h"

#define BUFLEN 100

typedef struct tagSysspec
{
	float lumda;   //arrival rate packet/sec
	float pinterarrivaltime;    //packet interarrival time in us

	float mu;     //service rate packet/sec
	float servicetime;  //service time per packet in us

	float r;       //token arrival rate token/sec
	float tinterarrivaltime; //token inter arrival time in us

	int B;      //token buffer length
	int P;      //no of tokens per packet
	int n;      //no of packets
	int realpacketno;   //after dropped

	long begintime;
	long simduration;
}Sysspec;

typedef struct tagStatistics
{
	long totalinterarrivaltime;
	double avginterarrivaltime;

	long totalservicetime;
	double avgservicetime;

	long totaltimeQ1;

	long totaltimeQ2;

	long totaltimeS;
	double totaltimeSsquare;
	double avgtimeS;
	double VtimeS;
	double DevtimeS;

	int no_tokendropped;
	int no_tokentotal;
	double tokendropp;

	int no_packetdropped;
	int no_packetarrived;
	double packetdropp;
}Statistics;

typedef struct tagToken
{
	int index;
	long arrivetime;
}Token;

typedef struct tagPacket
{
	int index;
	int no_token;
	long interarrivaltime;    //time are all in 'us'
	long servicetime;

	long arriveSystime;
	long realinterarrivaltime;
	long arriveQ1time;
	long leaveQ1time;
	long arriveQ2time;
	long leaveQ2time;
	long leaveSystime;
	long realservicetime;
	long realSystime;
}Packet;
///////////////////////////////////////////////////////////////////////////////////
int token_num=0;
int flag=0; //whether there is a packet in the server right now
int nopacketdropped=0;
My402List Q1;
My402List Q2;
My402List plist;
My402List final;
Sysspec sysspec;
Packet prevpacket;
Packet pinserver;
Statistics statistics;

pthread_t arrival_thread, server_thread, token_thread;
pthread_mutex_t m=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t q_not_empty=PTHREAD_COND_INITIALIZER;
sigset_t set;

//////////////////////////////////////////////////////////////////////////
long currtime()
{
	struct timeval Currtime;
	gettimeofday(&Currtime, NULL);
	return (Currtime.tv_sec*1000000+Currtime.tv_usec);
}

void InitlPacket(Packet *newpacket, Sysspec *sysspec)
{
	newpacket->index=0;   
	newpacket->no_token=0;
	newpacket->interarrivaltime=0;
	newpacket->servicetime=0;

	newpacket->arriveQ1time=0;
	newpacket->leaveQ1time=0;
	newpacket->arriveQ2time=0;
	newpacket->leaveQ2time=0;
	newpacket->arriveSystime=0;
	newpacket->leaveSystime=0;
	newpacket->realinterarrivaltime=0;
	newpacket->realservicetime=0;
	newpacket->realSystime=0;
}

void InitlStatistics(Statistics *statistics)
{
	statistics->totalinterarrivaltime=0;
	statistics->avginterarrivaltime=0;

	statistics->totalservicetime=0;
	statistics->avgservicetime=0;

	statistics->totaltimeQ1=0;

	statistics->totaltimeQ2=0;

	statistics->totaltimeS=0;
	statistics->avgtimeS=0;
	statistics->totaltimeSsquare=0;
	statistics->VtimeS=0;
	statistics->DevtimeS=0;

	statistics->no_tokendropped=0;
	statistics->no_tokentotal=0;
	statistics->tokendropp=0;

	statistics->no_packetdropped=0;
	statistics->no_packetarrived=0;
	statistics->packetdropp=0;
}
void creatplist(My402List *plist, Sysspec *sysspec)
{
	int no_packet;
	for(no_packet=1; no_packet<=sysspec->n; no_packet=no_packet+1)
	{
		Packet *newpacket = malloc(sizeof(Packet));
		InitlPacket(newpacket, sysspec);
		newpacket->index=no_packet;   
		newpacket->interarrivaltime=(long)sysspec->pinterarrivaltime;
		newpacket->servicetime=(long)sysspec->servicetime;
		newpacket->no_token=sysspec->P;
		My402ListAppend(plist, newpacket);
	}
	if(sysspec->B<sysspec->P)
		sysspec->realpacketno=0;
	else
		sysspec->realpacketno=My402ListLength(plist);
}
void attach(char *buf, My402List *plist, Sysspec *sysspec, int i)  // still need to be modifiled##############
{
	int k=0, j=0;
	char Interarrivaltime[30], No_token[30], Servicetime[30], start_ptr[1024];

	Packet *newpacket = malloc(sizeof(Packet));
	InitlPacket(newpacket, sysspec);
	newpacket->index=i;

	strncpy(start_ptr, buf, strlen(buf));
	start_ptr[strlen(buf)]='\0';

	while(start_ptr[k]!=' '&&start_ptr[k]!='\t')
	{
		Interarrivaltime[j]=start_ptr[k];
		k=k+1;
		j=j+1;
	}
	Interarrivaltime[j]='\0';

	while(start_ptr[k]==' '||start_ptr[k]=='\t')
	{
		  k=k+1;
	}

	j=0;
  while(start_ptr[k]!=' '&&start_ptr[k]!='\t')
	{
		No_token[j]=start_ptr[k];
		k=k+1;
		j=j+1;
	}
	No_token[j]='\0';

	while(start_ptr[k]==' '||start_ptr[k]=='\t')
	{
		  k=k+1;
	}

	j=0;
  while(start_ptr[k]!=' '&&start_ptr[k]!='\t'&&start_ptr[k]!='\0')
	{
		Servicetime[j]=start_ptr[k];
		k=k+1;
		j=j+1;
	}
	Servicetime[j]='\0';

	newpacket->interarrivaltime=atoi(Interarrivaltime)*1000;
	newpacket->no_token=atoi(No_token);
	newpacket->servicetime=atoi(Servicetime)*1000;
	My402ListAppend(plist, newpacket);

	if(newpacket->no_token>sysspec->B)
	{
		nopacketdropped=nopacketdropped+1;
	}
}

void readfile(FILE *fp, My402List *plist, Sysspec *sysspec)
{
	char buf[BUFLEN];
	int i=0;
	while(fgets(buf, BUFLEN, fp)!=NULL)
	{
		if(strlen(buf)>1)
		{
			buf[strlen(buf)-1]='\0';
			if(i==0)                                      //the first line
			{
				sysspec->n=atoi(buf); 
				i=i+1;
			}
			else
			{
				attach(buf, plist, sysspec,i);
				i=i+1;
			}
		}
	}
  sysspec->realpacketno=My402ListLength(plist)-nopacketdropped;
}

int Command(int argc, char *argv[], Sysspec *sysspec)
{
	//default value for sysspec
	sysspec->lumda=0.5;
	sysspec->pinterarrivaltime=(1/sysspec->lumda)*1000000;
	sysspec->mu=0.35;
	sysspec->servicetime=(1/sysspec->mu)*1000000;
	sysspec->r=1.5;
	sysspec->tinterarrivaltime=(1/sysspec->r)*1000000;
	sysspec->B=10;
	sysspec->P=3;
	sysspec->n=20;
  sysspec->realpacketno=0;
	//read command line
	FILE *fp=NULL;
	char filename[50];
	int i;
	for(i=1;i<argc; i=i+1)
	{
		if(strcmp(argv[i], "-lambda")==0)
		{
			i=i+1;
			sysspec->lumda=(float) atof(argv[i]);
			sysspec->pinterarrivaltime=(1/sysspec->lumda)*1000000;
		}
		else if(strcmp(argv[i], "-mu")==0)
		{
			i=i+1;
			sysspec->mu=(float) atof(argv[i]);
			sysspec->servicetime=(1/sysspec->mu)*1000000;
		}
		else if(strcmp(argv[i], "-r")==0)
		{
			i=i+1;
			sysspec->r=(float) atof(argv[i]);
			sysspec->tinterarrivaltime=(1/sysspec->r)*1000000;
		}
		else if(strcmp(argv[i], "-B")==0)
		{
			i=i+1;
			sysspec->B=atoi(argv[i]);
		}
		else if(strcmp(argv[i], "-P")==0)
		{
			i=i+1;
			sysspec->P=atoi(argv[i]);
		}
		else if(strcmp(argv[i], "-n")==0)
		{
			i=i+1;
			sysspec->n=atoi(argv[i]);
		}
		else if(strcmp(argv[i], "-t")==0)
		{
			i=i+1;
			fp=fopen(argv[i], "r");
			strncpy(filename, argv[i], strlen(argv[i]));
			filename[strlen(argv[i])]='\0';
		}
		else
		{
			fprintf(stderr,"Command Error.\n");
			exit(1);
		}
	}
/////////create plist
	if(fp==NULL)
	{
      fprintf(stdout, "Emulation Parameters:\n");
			fprintf(stdout, "\tnumber to arrive = %d\n", sysspec->n);
			fprintf(stdout, "\tlambda = %g\n", sysspec->lumda);
			fprintf(stdout, "\tmu = %g\n", sysspec->mu);
			fprintf(stdout, "\tr = %g\n", sysspec->r);
			fprintf(stdout, "\tB = %d\n", sysspec->B);
			fprintf(stdout, "\tP = %d\n", sysspec->P);
      
  	  if(sysspec->lumda<0.1)
	   {
		 sysspec->lumda=0.1;
	   sysspec->pinterarrivaltime=(1/sysspec->lumda)*1000000;
	   }
	   if(sysspec->mu<0.1)
	   {
		 sysspec->mu=0.1;
  	 sysspec->servicetime=(1/sysspec->mu)*1000000;
	   }
     if(sysspec->n<1000000000)
		 {
			creatplist(&plist, sysspec);
		 }
	}
	else
	{
		readfile(fp, &plist, sysspec);

		fprintf(stdout, "Emulation Parameters:\n");
		fprintf(stdout, "\tnumber to arrive = %d\n", sysspec->n);;
		fprintf(stdout, "\tr = %g\n", sysspec->r);
		fprintf(stdout, "\tB = %d\n", sysspec->B);
		fprintf(stdout, "\ttsfile = %s\n", filename);		
	}
	if(sysspec->r<0.1)
	{
		sysspec->r=0.1;
    sysspec->tinterarrivaltime=(1/sysspec->r)*1000000;
	}
	return TRUE;
}


void copypacket(Packet *oldp, Packet *newp)
{
	newp->index=oldp->index;  
	newp->no_token=oldp->no_token;
	newp->interarrivaltime=oldp->interarrivaltime;
	newp->servicetime=oldp->servicetime;

	newp->arriveQ1time=oldp->arriveQ1time;
	newp->leaveQ1time=oldp->leaveQ1time;
	newp->arriveQ2time=oldp->arriveQ2time;
	newp->leaveQ2time=oldp->leaveQ2time;
	newp->arriveSystime=oldp->arriveSystime;;
	newp->leaveSystime=oldp->leaveSystime;
	newp->realinterarrivaltime=oldp->realinterarrivaltime;
	newp->realservicetime=oldp->realservicetime;
	newp->realSystime=oldp->realSystime;
}

void calstatistics(Statistics *statistics, My402List *list)
{
	statistics->avginterarrivaltime=((double)statistics->totalinterarrivaltime/(double)(sysspec.n))/1000;   //in ms
	statistics->tokendropp=(double)statistics->no_tokendropped/(double)statistics->no_tokentotal;
	if(sysspec.B<sysspec.P)
		statistics->packetdropp=1;
	else
		statistics->packetdropp=(double)statistics->no_packetdropped/(double)(statistics->no_packetarrived);
	if (My402ListEmpty(list)==TRUE)
	{

		fprintf(stdout, "Statistics:\n");
		fprintf(stdout, "\taverage packet inter-arrival time = %.6gms\n", statistics->avginterarrivaltime);
		fprintf(stdout, "\taverage packet service time not avaliable, because no completed packet\n");
		fprintf(stdout, "\taverage number of packets in Q1 not avaliable, because no completed packet\n");
		fprintf(stdout, "\taverage number of packets in Q2 not avaiable, because no completed packet\n");
		fprintf(stdout, "\taverage number of packets in S not avaliable, because no completed packet\n");
		fprintf(stdout, "\taverage time a packet spent in system because no completed packet\n");
		fprintf(stdout, "\tstandard deviation for time spent in system because no completed packet\n");
		fprintf(stdout, "\ttoken drop probability = %.6g\n", statistics->tokendropp);
		fprintf(stdout, "\tpacket drop probability = %.6g\n",statistics->packetdropp);
	}
	else{
	My402ListElem *elem=My402ListFirst(list);
	Packet *packet=elem->obj;

	int i;
	for(i=1; i<=My402ListLength(list); i=i+1)
	{
		statistics->totalservicetime=statistics->totalservicetime+packet->realservicetime;
		statistics->totaltimeQ1=statistics->totaltimeQ1+(packet->leaveQ1time-packet->arriveQ1time);
		statistics->totaltimeQ2=statistics->totaltimeQ2+(packet->leaveQ2time-packet->arriveQ2time);
		statistics->totaltimeS=statistics->totaltimeS+(packet->leaveSystime-packet->arriveSystime);
		statistics->totaltimeSsquare=statistics->totaltimeSsquare+((double)packet->realSystime/1000*(double)packet->realSystime/1000);
		if(i!=My402ListLength(list))
   {
    elem=My402ListNext(list, elem);
		packet=elem->obj;}		
	}

	statistics->avgservicetime=((double)statistics->totalservicetime/My402ListLength(list))/1000;
	statistics->avgtimeS=((double)statistics->totaltimeS/My402ListLength(list))/1000;
	statistics->VtimeS=statistics->totaltimeSsquare/My402ListLength(list)-statistics->avgtimeS*statistics->avgtimeS;
	statistics->DevtimeS=sqrt(statistics->VtimeS);

	fprintf(stdout, "Statistics:\n");
	fprintf(stdout, "\taverage packet inter-arrival time = %.6gms\n", statistics->avginterarrivaltime);
	fprintf(stdout, "\taverage packet service time = %.6gms\n", statistics->avgservicetime);
	fprintf(stdout, "\taverage number of packets in Q1= %.6g\n", (double)statistics->totaltimeQ1/(double)sysspec.simduration);
	fprintf(stdout, "\taverage number of packets in Q2= %.6g\n", (double)statistics->totaltimeQ2/(double)sysspec.simduration);
	fprintf(stdout, "\taverage number of packets in S = %.6g\n", (double)statistics->totaltimeS/(double)sysspec.simduration);
	fprintf(stdout, "\taverage time a packet spent in system = %.6gms\n", statistics->avgtimeS);
	fprintf(stdout, "\tstandard deviation for time spent in system = %.6gms\n", statistics->DevtimeS);
	fprintf(stdout, "\ttoken drop probability = %.6g\n", statistics->tokendropp);
	fprintf(stdout, "\tpacket drop probability = %.6g\n", statistics->packetdropp);
	}
}
//////////////////////////////////////////////////////////////////////////
void PAQ1(Packet *newpacket, Sysspec *sysspec, Packet *prevpacket)
{
	statistics.no_packetarrived=statistics.no_packetarrived+1;
	newpacket->arriveSystime=currtime()-sysspec->begintime;    //assign arriveQ1time
		newpacket->realinterarrivaltime=newpacket->arriveSystime-prevpacket->arriveSystime;
		statistics.totalinterarrivaltime=statistics.totalinterarrivaltime+newpacket->realinterarrivaltime;
		if(newpacket->no_token<=sysspec->B)
    fprintf(stdout, "%012.3fms: p%d arrives, need %d tokens, inter-arrival time = %.3fms\n", (float)newpacket->arriveSystime/1000, newpacket->index, newpacket->no_token, (float)newpacket->realinterarrivaltime/1000);
    else if(newpacket->no_token>sysspec->B)
	{
		fprintf(stdout, "%012.3fms: p%d arrives, need %d tokens, inter-arrival time = %.3fms, dropped\n", (float)newpacket->arriveSystime/1000, newpacket->index, newpacket->no_token, (float)newpacket->realinterarrivaltime/1000);
		statistics.no_packetdropped=statistics.no_packetdropped+1;
	}
		if(newpacket->no_token<=sysspec->B)	
		{
			My402ListAppend(&Q1, newpacket);   //enqueue the packet to Q1(my402list)
			newpacket->arriveQ1time=currtime()-sysspec->begintime;
			fprintf(stdout, "%012.3fms: p%d enters Q1\n", (float) newpacket->arriveQ1time/1000, newpacket->index);
		}
}

void PLQ1(Packet *newpacket, int token_num, Sysspec *sysspec)
{
	newpacket->leaveQ1time=currtime()-sysspec->begintime;
	fprintf(stdout, "%012.3fms: p%d leaves Q1, time in Q1 = %.3fms, token bucket now has %d token\n", (float) newpacket->leaveQ1time/1000, newpacket->index, ((float) newpacket->leaveQ1time-(float) newpacket->arriveQ1time)/1000, token_num);
}

void PAQ2(Packet *packet, Sysspec *sysspec)
{
	packet->arriveQ2time=currtime()-sysspec->begintime;
	fprintf(stdout, "%012.3fms: p%d enters Q2\n", (float) packet->arriveQ2time/1000, packet->index);
}

void PLQ2(Packet *packet, Sysspec *sysspec)
{
	packet->leaveQ2time=currtime()-sysspec->begintime;
	fprintf(stdout, "%012.3fms: p%d begin service at S, time in Q2 = %.3fms\n", (float) packet->leaveQ2time/1000, packet->index,  ((float) packet->leaveQ2time-(float) packet->arriveQ2time)/1000);
}
void PLS(Packet *packet, Sysspec *sysspec)
{
	packet->leaveSystime=currtime()-sysspec->begintime;
	packet->realservicetime=packet->leaveSystime-packet->leaveQ2time;
	packet->realSystime=packet->leaveSystime-packet->arriveSystime;
	fprintf(stdout, "%012.3fms: p%d departs form S, service time = %.3fms, time in system = %.3fms\n", (float)packet->leaveSystime/1000, packet->index, (float)packet->realservicetime/1000, ((float)packet->leaveSystime-(float) packet->arriveSystime)/1000);
}
void interrupt(int sig)   //server's handler when receive SIGINT
{
	long now=currtime()-sysspec.begintime;
	long remain;
	pthread_kill(arrival_thread, SIGUSR1);    //send SIGUSR1 to arrival_thread
 

	//statistics.no_packetdropped=statistics.no_packetdropped+My402ListLength(&Q1);
	My402ListUnlinkAll(&Q1);

	//statistics.no_packetdropped=statistics.no_packetdropped+My402ListLength(&Q2);
	My402ListUnlinkAll(&Q2);

  if(flag==0)
	{
		pthread_kill(token_thread, SIGUSR2);    //send SIGUSR2 to token_thread
		pthread_exit(0);
	}
	else
	{
		remain=pinserver.servicetime-(now-pinserver.leaveQ2time);
		struct timeval Servicetime;
		Servicetime.tv_sec = remain/1000000;
		Servicetime.tv_usec= remain-Servicetime.tv_sec*1000000;
		select(0, NULL, NULL, NULL, &Servicetime);
		Packet *lastp=malloc(sizeof(Packet));
		copypacket(&pinserver, lastp);
    PLS(lastp, &sysspec);
		My402ListAppend(&final, lastp);
		pthread_kill(token_thread, SIGUSR2);    //send SIGUSR2 to token_thread
		pthread_exit(0);
	}
}
void sigusr1(int sig)  //arrival_thread's handler when receive SIGUSR1
{
	pthread_exit(0);
}
void sigusr2(int sig)  //token_thread's handler when receive SIGUSR2
{
	pthread_exit(0);
}
/////////////////////////////////////////////////////////////////////////////////////
void *arrival_th( )
{
	sigset(SIGUSR1, sigusr1);
	struct timeval Interarrivaltime;
	int no_packets=1;
 	Packet *newpacket;
	Packet *firstpacket;

	while(no_packets<=sysspec.n)
	{
		My402ListElem *firstelem=NULL;
		if(sysspec.n<1000000000)
		{
			firstelem=My402ListFirst(&plist);
			newpacket=firstelem->obj;
		}
		else
		{
			newpacket = malloc(sizeof(Packet));
			InitlPacket(newpacket, &sysspec);
			newpacket->index=no_packets;   
			newpacket->interarrivaltime=(long)sysspec.pinterarrivaltime;
			newpacket->servicetime=(long)sysspec.servicetime;
			newpacket->no_token=sysspec.P;
		}
	
			
			Interarrivaltime.tv_sec = newpacket->interarrivaltime/1000000;
			Interarrivaltime.tv_usec=newpacket->interarrivaltime-Interarrivaltime.tv_sec*1000000;
			select(0, NULL, NULL, NULL, &Interarrivaltime);   //match the given interarrival time for tokens
		
			pthread_mutex_lock(&m);
		if(sysspec.n<1000000000)
			My402ListUnlink(&plist, firstelem);

			PAQ1(newpacket, &sysspec, &prevpacket);  //packet arrives Q1				
		
			no_packets=no_packets+1;

			copypacket(newpacket,&prevpacket);
			if(newpacket->no_token>sysspec.B)
			{
				
				pthread_mutex_unlock(&m);
				continue;
			}

		if(My402ListEmpty(&Q1)==FALSE)
		{
			firstelem=My402ListFirst(&Q1);
			firstpacket=firstelem->obj;
			if(token_num>=firstpacket->no_token)  //move the first packet in Q1 to Q2 if there are enough tokens
			{
				My402ListUnlink(&Q1,  firstelem);
				token_num=token_num-firstpacket->no_token;
				PLQ1(firstpacket, token_num, &sysspec);
				if(My402ListEmpty(&Q2)==TRUE)   //if Q2 was empty before, need to signal or broad a queue-not-empty condition
				{
					pthread_cond_signal(&q_not_empty);
				}
				My402ListAppend(&Q2, firstpacket);
				PAQ2(firstpacket, &sysspec);
			}	
		}
			pthread_mutex_unlock(&m);
			//go back to sleep() for the right amount
	}
	return 0;
}

void *server_th() 
{
	sigset(SIGINT, interrupt);  //set server as the only thread which can accept CTRL+C
	pthread_sigmask(SIG_UNBLOCK, &set, NULL);
	if(sysspec.n>1000000000)
	{
		sysspec.realpacketno=sysspec.n;
	}
	int no_packet=1;

	while(no_packet<=sysspec.realpacketno)
	{
		pthread_mutex_lock(&m);
		if(My402ListEmpty(&Q2)==TRUE)
			pthread_cond_wait(&q_not_empty, &m);   
 
		My402ListElem *elem=My402ListFirst(&Q2);//dequeues a packet and unlock mutex
		Packet *firstpacket=elem->obj;
		My402ListUnlink(&Q2, elem);
		pthread_mutex_unlock(&m);
			
		PLQ2(firstpacket, &sysspec);
		copypacket(firstpacket, &pinserver);
		flag=1;  //there is a packet being served

		no_packet=no_packet+1;
			
		struct timeval Servicetime;
		Servicetime.tv_sec = firstpacket->servicetime/1000000;
		Servicetime.tv_usec= firstpacket->servicetime-Servicetime.tv_sec*1000000;
		select(0, NULL, NULL, NULL, &Servicetime);

			//sleep for an interval matching the service time of the packet;
		PLS(firstpacket, &sysspec);//then eject the packet from the system
		My402ListAppend(&final, firstpacket);
		flag=0;  //finish serving packet
	}
	return 0;
}

void *token_th()
{
	sigset(SIGUSR2, sigusr2);
	struct timeval Tinterarrivaltime;
	Tinterarrivaltime.tv_sec = (long) (sysspec.tinterarrivaltime/1000000);
	Tinterarrivaltime.tv_usec=((long) (sysspec.tinterarrivaltime))-Tinterarrivaltime.tv_sec*1000000;
	int token_no=0;

	while(1)
	{
		select(0, NULL, NULL, NULL, &Tinterarrivaltime); //sleeps for an interval (match a given interarrival time for tokens)
		
		//wake up, try to increment token count
		Token *newtoken=malloc(sizeof(Token));
		token_no=token_no+1;
		statistics.no_tokentotal=token_no;
		newtoken->index=token_no;
		newtoken->arrivetime=currtime()-sysspec.begintime;

		pthread_mutex_lock(&m);     //cannot lock any more ######################################################       

		if(token_num<sysspec.B)
		{
			token_num=token_num+1;
			fprintf(stdout, "%012.3fms: token t%d arrives, token bucket now has %d tokens \n", (float)newtoken->arrivetime/1000, newtoken->index, token_num);
		}
		else
		{
			token_num=sysspec.B;
			statistics.no_tokendropped=statistics.no_tokendropped+1;
			fprintf(stdout, "%012.3fms: token t%d arrives, dropped, token bucket now has %d tokens \n", (float)newtoken->arrivetime/1000, newtoken->index, token_num);
		}
		

		//check if can move first packet from Q1 to Q2
		if(My402ListEmpty(&Q1)==FALSE)
		{
			My402ListElem *firstelem=My402ListFirst(&Q1);
			Packet *firstpacket=firstelem->obj;
			if(token_num>=firstpacket->no_token)  //move the first packet in Q1 to Q2 if there are enough tokens
			{
				My402ListUnlink(&Q1,  firstelem);
				token_num=token_num-firstpacket->no_token;
				PLQ1(firstpacket, token_num, &sysspec);	
			
				if(My402ListEmpty(&Q2)==TRUE)   //if Q2 was empty before, need to signal or broad a queue-not-empty condition
				{	
					pthread_cond_signal(&q_not_empty);
				}
				My402ListAppend(&Q2, firstpacket);
				PAQ2(firstpacket, &sysspec);
			}
		}

		pthread_mutex_unlock(&m);

		if(sysspec.n<1000000000){
			if(My402ListEmpty(&plist)==TRUE&&My402ListEmpty(&Q1)==TRUE)	//when packet list and Q1 is empty, return
			{
				return 0;                                                   
			}
		}
	}
}

int main(int argc, char *argv[])
{
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	pthread_sigmask(SIG_BLOCK, &set, 0);
	
	if(My402ListInit(&Q1)==FALSE||My402ListInit(&Q2)==FALSE||My402ListInit(&plist)==FALSE||My402ListInit(&final)==FALSE)   //initialize Q1 and Q2 and plist
	{
		fprintf(stderr, "Initial unsuccessful.\n");
		return FALSE;
	}

	InitlStatistics(&statistics);
	
	if(Command(argc, argv, &sysspec)==FALSE)
	{
		printf("Error commandline.\n");
		exit(1);
	}

	sysspec.begintime=currtime();   //############################## move this line behind
	fprintf(stdout, "%012.3fms: emulation begins \n", ((float)currtime()-(float)sysspec.begintime)/1000);

	InitlPacket(&prevpacket, &sysspec);

	pthread_create(& arrival_thread, 0, arrival_th, 0);
	pthread_create(& server_thread, 0, server_th, 0 );
	pthread_create(& token_thread, 0, token_th, 0);
	
	pthread_join(arrival_thread, 0);
	pthread_join(token_thread, 0);
	pthread_join(server_thread, 0);

	sysspec.simduration=currtime()-sysspec.begintime;  //#############not sure yet
	fprintf(stdout, "%012.3fms: emulation ends \n", ((float)sysspec.simduration)/1000);
	
  calstatistics(&statistics, &final);
  return 0;
}
