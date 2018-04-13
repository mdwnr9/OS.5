#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <pthread.h>

const int n = 18;
const int m = 10;
int verbose = 0;
int available[10];
int allocation[18][10];
int maximum[10];
int active[18];
//Check if work is less than need array
int check_min_n(int i, int *work, int *need)
{
	int j;
	for(j = 0; j < m;j++)
		if(need[m*i+j]>work[j])
			return 0;
	return 1;
}
//Check if work is less than available
int check_min_a(int *work)
{
	int j;
	for(j = 0; j < m;j++)
		if(available[j]<work[j])
			return 0;
	return 1;
}
//Add allocation resources to work
void add_alloc(int i, int *work)
{
	int j;
	for(j = 0; j < m;j++)
		work[j] += allocation[i][j];
}
//Check if the state is safety
int check_safety(int *need)
{
	int i;
	int finishes[n];
	int work[m];
	int flag;
	int actives = 0;
	for(i = 0; i < n; i++)
	{
		finishes[i]=active[i];
		if(finishes[i])
		{
			actives++;
		}
	}
	for(i = 0; i < m; i++)
		work[i]=available[i];
	while(actives)
	{
		flag = 1;
		for(i = 0; i < n; i++)
		{
			if(check_min_n(i, work,&need[i*m]))
			{
				add_alloc(i, work);
				actives--;
				flag = 0;
				break;
			}
		}
		if(flag)
			return 0;
	}
	return 1;
}
//Release resources
void release_request(int r, int *request, int *need)
{
	int i;
	for(i = 0; i < m; i++)
	{
		available[i]+=request[i];
		allocation[r][i]-=request[i];
		need[m*r+i]+=request[i];
	}
	if(verbose)
	printf("Resources release for process %d\n",r);
}
//Alloc resources or deny them
int resource_request(int r, int *request, int *need)
{
	int i;
	if(!check_min_a(request))
		return 2;
	for(i = 0; i < m; i++)
	{
		available[i]-=request[i];
		allocation[r][i]+=request[i];
		need[m*r+i]-=request[i];
	}
	if(check_safety(need))
	{
		if(verbose)
		printf("Resources allocated for process %d\n",r);
		return 1;
	}
	else
	{
		for(i = 0; i < m; i++)
		{
			available[i]+=request[i];
			allocation[r][i]-=request[i];
			need[m*r+i]+=request[i];
		}
		if(verbose)
			printf("Request rejected for process %d: it causes an unsafe state\n",r);
		return 0;
	}
}
//Process that request resources
void process(int index)
{
	int terminate;
	int wait;
	int i;
	int quantity;
	int *request;
	int *need;
	int *wait_request;
	int shmid;
	int shmkey = 1222;
	shmid = shmget(shmkey,2048, IPC_CREAT | S_IRUSR | S_IWUSR);					//Create shared memory
	if(shmid < 0)
	{
		fprintf(stderr, "Error creating shared memory\n");
		return;
	}
	int *shmpointer = (int*)shmat(shmid,(void*)0, 0);	
	request = shmpointer + m*index;
	wait_request = shmpointer + n*m + index;
	need = shmpointer + n*m + n + m*index;
	for(;;)
	{
		terminate = rand()%2;
		if(!terminate)
		{
			*wait_request = -1;
			return;
		}
		wait = rand()%2+1;
		quantity = rand()%4+1;
		for(i = 0; i < m; i++)
		{
			request[i] = 0;
			if(rand()%2==1)
			{
				quantity--;
				request[i]= rand()%maximum[i];
				request[i] = request[i] < 0 ? 0:request[i];
				need[i]=request[i];
				
			}
			if(!quantity)
				break;
		}
		sleep(wait);
		*wait_request = 0;
		while (*wait_request == 0);

		wait = rand()%2+1;
		sleep(wait);
		if(*wait_request == 4)
			continue;
		*wait_request = 3;
		while (*wait_request == 3);
	}
}
//Normalize time
void normalize(int *sec, int *nano)
{
	*sec += *nano/1000000000;
	*nano %= 1000000000;
}
//Master process
void oss()
{
	int queue[18];
	int ans;
	int last_queue;
	int first_queue;
	int *request;
	int *wait_request;
	int total_process = 0;
	int active_process = 0;
	int i , j;
	int *need;
	int usec = 0;
	int sec = 0;
	int shmid;
	int shmkey = 1222;
	time_t t;
	shmid = shmget(shmkey,2048, IPC_CREAT | S_IRUSR | S_IWUSR);					//Create shared memory
	if(shmid < 0)
	{
		fprintf(stderr, "Error creating shared memory\n");
		return;
	}
	int *shmpointer = (int*)shmat(shmid,(void*)0, 0);
	request = shmpointer;
	wait_request = shmpointer + n*m;
	need = wait_request + n;
	first_queue = 0;
	last_queue = 0;
	srand(time(NULL));
	for(i = 0; i < m; i++)
		available[i] = rand()%20 + 1;
	for(i = 0; i < m; i++)
		maximum[i] = available[i]%10;
	for(i = 0;i < n;i ++)
		for(j = 0; j <m; j++)
			allocation[i][j]=0;
	for(i = 0; i < n;i++)
	{
		active[i] = 0;
		queue[i] = -1;
	}
	t = time(NULL);
	for(;;)
	{
		if(difftime(time(NULL),t)>2)
		{
			printf("Exit. 2 secs running\n");
			break;
		}
		if(total_process + active_process < 100 && active_process < 18)
		{
			sleep(rand()%2+1);
			for(i = 0; i < n; i++)
			{
				if(!active[i])
				{
					active_process++;
					active[i] = 1;
					wait_request[i]=2;
					if(fork()==0)
					{
						process(i);
						return;
					}
					break;
					if(verbose)
					printf("Master created process P%d at time %d:%d\n",i,sec,usec);
				}
			}
			usec+=10000000;
			normalize(&sec, &usec);
		}
		while(queue[first_queue] != -1 && last_queue != first_queue)
		{
			ans = resource_request(first_queue, &request[first_queue*m], need);
			if(verbose)
			printf("Master checked enqueued process P%d at time %d:%d\n",i,sec,usec);
			usec+=20000000;
			normalize(&sec, &usec);
			if(ans == 2)
			{
				break;
			}
			else
			{
				queue[first_queue] = -1;
				first_queue = (first_queue + 1)%18;
				wait_request[i] = 1;
				active[i]=1;
				if(verbose)
				printf("Master unenqued process P%d at time %d:%d\n",i,sec,usec);
				usec+=20000000;
				normalize(&sec, &usec);
			}
			if(verbose)
			printf("Master checked enqueued process P%d at time %d:%d\n",i,sec,usec);
			usec+=20000000;
			normalize(&sec, &usec);
		}
		for(i = 0; i < n; i++)
		{
			if(active[i] == 1)
			{
				if(wait_request[i]==0)
				{
					ans = resource_request(i, request+i*m,need);
					if(verbose)
					printf("Master is attending request of P%d at time %d:%d\n",i,sec,usec);
					usec+=20000000;
					normalize(&sec, &usec);
					if(ans == 2)
					{
						if(verbose)
						printf("Master is enqueing process P%d at time %d:%d\n",i,sec,usec);
						usec+=20000000;
						normalize(&sec, &usec);
						active[i]=2;
						queue[last_queue] = i;
						last_queue = (last_queue + 1)%18;
					}
					else if(ans == 0)
					{
						wait_request[i] = 4;
					}
					else{
						wait_request[i] = 1;
					}
				}
				else if(wait_request[i]== 3)
				{
					release_request(i, request+i*m,need);
					wait_request[i] = 1;
				}
				else if(wait_request[i]==-1)
				{
					total_process++;
					active[i] = 0;
					active_process--;
				}
				usec+=30000000;
				normalize(&sec, &usec);
			}
		}
		printf("Allocated resources. Active Processes %d. Terminated Processes %d\n",active_process, total_process);
		for(i = 0; i < n; i ++)
		{
			if(active[i])
			{
				printf("Process %d\n",i);
				for(j = 0; j < m; j++)
					printf("%d ",allocation[i][j]);
				printf("\n");
				for(j = 0; j < m; j++)
					printf("%d ",available[j]);
				printf("\n");
			}
		}
		usec+=20000000;
		normalize(&sec, &usec);
		if(total_process == 20)
			break;
	}
	//Detach & remove from shared memory 

     
     shmdt((void *) shmpointer);
     printf("detached  from shared memory...\n");
     shmctl(shmid, IPC_RMID, NULL);
     printf(" removed from shared memory...\n");
  

	printf("Finishing oss simulator\n");
}
int main(int argc, char**argv)
{
	if(argc > 1)
	{
		if(strcmp(argv[1],"verbose")==0)
			verbose = 1;
	}
	oss();
	return 0;
}
