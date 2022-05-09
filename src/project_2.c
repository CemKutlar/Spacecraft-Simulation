#include "queue.c"
#include <pthread.h>
#include <string.h>
#include <sys/time.h>

#define LAUNCHING 0
#define LANDING 1
#define ASSEMBLY 2
#define EMERGENCY 3
#define true 1
#define false 0
#define MAX_JOB_COUNT 1000
#define for_each_item(item, q) for(item = q.head; item != NULL && item != q.tail; item = item->prev) // for_each implementation for debugging ID in each array


int simulationTime = 120;    // simulation time
int seed = 10;               // seed for randomness
int emergencyFrequency = 40; // frequency of emergency
float p = 0.2;               // probability of a ground job (launch & assembly)
int ID = 0; 
pthread_t tid[MAX_JOB_COUNT];
int threadCount = 0;
Queue queArray[4];
int isPadAFree = true;
int isPadBFree = true;
int isEmrgncyA = false;
int isEmrgncyB = false;
char **logArray; //string array for event.log
int request_time[MAX_JOB_COUNT]; 
char *typeJob;
int debug_start = 0;

pthread_mutex_t padA_lock = PTHREAD_MUTEX_INITIALIZER; //mutex for padA
pthread_mutex_t padB_lock = PTHREAD_MUTEX_INITIALIZER; //mutex for padB
pthread_mutex_t emergency = PTHREAD_MUTEX_INITIALIZER; //mutex for job emergency
pthread_mutex_t land = PTHREAD_MUTEX_INITIALIZER;      //mutex for job land
pthread_mutex_t launch = PTHREAD_MUTEX_INITIALIZER;    //mutex for job launch
pthread_mutex_t assembly = PTHREAD_MUTEX_INITIALIZER;  //mutex for job assembly


pthread_mutex_t first_job_mutex; //mutex for unlocking tower
pthread_cond_t condA; //condition for emergency handling
pthread_cond_t condB; //condition for emergency handling

time_t simulation_end_time;
time_t simulation_start_time;

void* LandingJob(void *id); 
void* LaunchJob(void *id);
void* EmergencyJob(void *id); 
void* AssemblyJob(void *id); 
void* ControlTower(void *id);   
void CreateJob(int type, Queue queArray[]);
float randomNumGenerator();        //generate rondom number 0 to 1
void printLog();                   //writes to log file
char *my_itoa(int num, char *str); //convert int to string
void* DebugQue(void *args);

// pthread sleeper function
int pthread_sleep (int seconds)
{
	pthread_mutex_t mutex;
	pthread_cond_t conditionvar;
	struct timespec timetoexpire;
	if(pthread_mutex_init(&mutex,NULL))
	{
		return -1;
	}
	if(pthread_cond_init(&conditionvar,NULL))
	{
		return -1;
	}
	struct timeval tp;
	//When to expire is an absolute time, so get the current time and add it to our delay time
	gettimeofday(&tp, NULL);
	timetoexpire.tv_sec = tp.tv_sec + seconds; timetoexpire.tv_nsec = tp.tv_usec * 1000;
	
	pthread_mutex_lock (&mutex);
	int res =  pthread_cond_timedwait(&conditionvar, &mutex, &timetoexpire);
	pthread_mutex_unlock (&mutex);
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&conditionvar);
	
	//Upon successful completion, a value of zero shall be returned
	return res;
}

int main(int argc,char **argv){
	// -p (float) => sets p
	// -t (int) => simulation time in seconds
	// -s (int) => change the random seed
	for(int i=1; i<argc; i++){
		if(!strcmp(argv[i], "-p")) {p = atof(argv[++i]);}
		else if(!strcmp(argv[i], "-t")) {simulationTime = atoi(argv[++i]);}
		else if(!strcmp(argv[i], "-s"))  {seed = atoi(argv[++i]);}
		else if(!strcmp(argv[i], "-n"))  {debug_start = atoi(argv[++i]);}
	}
	
	srand(seed); // feed the seed

	struct timeval tp;
	gettimeofday(&tp, NULL);
	simulation_start_time = tp.tv_sec;
	simulation_end_time = tp.tv_sec + simulationTime;


	logArray = (char**)malloc(MAX_JOB_COUNT*sizeof(char*));
	for ( int i = 0; i < MAX_JOB_COUNT; i++ ) {
		logArray[i] = (char*)malloc(6*sizeof(char));//init string array for events.log
	}

	//initilaze queue arryas
	Queue *landQue = ConstructQueue(MAX_JOB_COUNT);
	Queue *launchQue = ConstructQueue(MAX_JOB_COUNT);
	Queue *assemblQue = ConstructQueue(MAX_JOB_COUNT);
	Queue *emergencyQue = ConstructQueue(MAX_JOB_COUNT);
	queArray[0] = *launchQue;
	queArray[1] = *landQue;
	queArray[2] = *assemblQue;
	queArray[3] = *emergencyQue;

	if(pthread_mutex_init(&first_job_mutex,NULL))
	{
		return -1;
	}
	if(pthread_mutex_init(&padA_lock,NULL))
	{
		return -1;
	}
	if(pthread_mutex_init(&padB_lock,NULL))
	{
		return -1;
	}
	if(pthread_mutex_init(&emergency,NULL))
	{
		return -1;
	}
	if(pthread_mutex_init(&land,NULL))
	{
		return -1;
	}
	if(pthread_mutex_init(&launch,NULL))
	{
		return -1;
	}
	if(pthread_mutex_init(&assembly,NULL))
	{
		return -1;
	}
	if(pthread_cond_init(&condA,NULL))
	{
		return -1;
	}
	if(pthread_cond_init(&condB,NULL))
	{
		return -1;
	}

	// lock all mutex before unlocking control tower
	pthread_mutex_lock(&padA_lock);
	pthread_mutex_lock(&padB_lock);
	pthread_mutex_lock(&first_job_mutex);
	pthread_mutex_lock(&land);
	pthread_mutex_lock(&launch);
	pthread_mutex_lock(&assembly);

	pthread_create(&(tid[threadCount++]), NULL, &ControlTower, NULL); // control tower
	CreateJob(LAUNCHING, queArray);  // first job 
	pthread_create(&(tid[threadCount++]), NULL, &DebugQue,NULL); //debug thread

	int t = 80; //emrgency countdown (40 second)
	while(tp.tv_sec < simulation_end_time) {

		pthread_sleep(2);
		float random = randomNumGenerator(); 
		if(random <= 1 - p) {
			CreateJob(LANDING, queArray);
		}
		if(random <= p / 2){ 
			CreateJob(LAUNCHING, queArray);
		} 
		if(random <= p / 2) {
			CreateJob(ASSEMBLY, queArray);

		}
		if(t == 0){
			CreateJob(EMERGENCY, queArray);
			CreateJob(EMERGENCY, queArray);
			t=80;
		}
		gettimeofday(&tp, NULL);
		t -= 2;		
	}
	printLog(queArray);  //write events.log after simulation end
	
	return 0;
}
void* DebugQue(void *args) {	
	struct timeval tp;
	gettimeofday(&tp, NULL);
	while(debug_start == 0){} //if you do not enter n as an argument debug will not work Note: See README file for type of command line arguments of the simulation

	while(tp.tv_sec < simulation_end_time) {
		pthread_sleep(1);
		for(int j = 0; j < 3 && tp.tv_sec - simulation_start_time > debug_start-1 && debug_start > 0; j++) {
			NODE *x;
			if(j == 0) printf("At %ld sec launch :\t", tp.tv_sec - simulation_start_time);
			if(j == 1) printf("At %ld sec landing :\t", tp.tv_sec - simulation_start_time);
			if(j == 2) printf("At %ld sec assembly :\t", tp.tv_sec - simulation_start_time);
			if(!isEmpty(&queArray[j])) printf("%d", queArray[j].head->data.ID);
			for_each_item(x, queArray[j]) {
				printf(", %d", x->prev->data.ID);
			}
			printf("\n");
		}
		if(tp.tv_sec - simulation_start_time > debug_start-1) printf("\n");
		gettimeofday(&tp, NULL);		
	}
	pthread_exit(NULL);
}
// the function that creates plane threads for landing
void* LandingJob(void *id){
	
	int job_id = *((int *) id);
	int isLand = 0;
	while(job_id != queArray[LANDING].head->data.ID) {}
	do {
		if(isPadAFree) 
		{    
			pthread_mutex_lock(&land);
			pthread_mutex_lock(&padA_lock);			
			isPadAFree = false; 
			int endTime;
			int turnAroundTime;
			struct timeval tp;			
			int sleep = 2;
			while(sleep != 0) {//not sure if the job start from scratch or continue from where it stopped.
				while(!isEmpty(&queArray[EMERGENCY])){
					isPadAFree = true;
					pthread_cond_wait(&condA, &padA_lock);
					isPadAFree = false;
				}
				pthread_sleep(1); 
				sleep--;
			}
			gettimeofday(&tp, NULL);
			endTime = tp.tv_sec - simulation_start_time;
			turnAroundTime = endTime - request_time[job_id];
			char b[5];
			char c[5];
			my_itoa(endTime,b);
			strcat(logArray[job_id],b);
			strcat(logArray[job_id],"\t\t");
			
			my_itoa(turnAroundTime,c);
			strcat(logArray[job_id],c);
			strcat(logArray[job_id],"\t\t");

			strcat(logArray[job_id],"A");

			Dequeue(&queArray[LANDING]);
			isPadAFree = true; 
			pthread_exit(NULL);
		}
		if(isPadBFree)         
		{      
			pthread_mutex_lock(&land);
			pthread_mutex_lock(&padB_lock);					
			isPadBFree = false;
			int endTime;
			int turnAroundTime;
			struct timeval tp;
			int sleep = 2;
			while(sleep != 0) {//not sure if the job start from scratch or continue from where it stopped.
				while(!isEmpty(&queArray[EMERGENCY])){
					isPadBFree = true;
					pthread_cond_wait(&condB, &padB_lock);
					isPadBFree = false;
				}
				pthread_sleep(1);
				sleep--;
			}
			gettimeofday(&tp, NULL);
			endTime = tp.tv_sec - simulation_start_time;
			turnAroundTime = endTime - request_time[job_id];
			char b[5];
			char c[5];
			my_itoa(endTime,b);
			strcat(logArray[job_id],b);
			strcat(logArray[job_id],"\t\t");
			
			my_itoa(turnAroundTime,c);
			strcat(logArray[job_id],c);
			strcat(logArray[job_id],"\t\t");

			strcat(logArray[job_id],"B");
			Dequeue(&queArray[LANDING]);
			isPadBFree = true;
			pthread_exit(NULL);
		}   
	} while(!isLand);
}

// the function that creates plane threads for departure
void* LaunchJob(void *id){
	int job_id = *((int *) id);
	if (job_id == 0) {
		pthread_mutex_unlock(&first_job_mutex);
		printf("Simulation has been started and a rocket taking off from pad A.\n\n");
		isPadAFree = false;
		int endTime;
		int turnAroundTime;
		struct timeval tp;		
		pthread_sleep(4);
		gettimeofday(&tp, NULL);
		endTime = tp.tv_sec - simulation_start_time;
		turnAroundTime = endTime - request_time[job_id];
		char b[5];
		char c[5];
		my_itoa(endTime,b);
		strcat(logArray[job_id],b);
		strcat(logArray[job_id],"\t\t");
			
		my_itoa(turnAroundTime,c);
		strcat(logArray[job_id],c);
		strcat(logArray[job_id],"\t\t");

		strcat(logArray[job_id],"A");
		Dequeue(&queArray[LAUNCHING]);
		isPadAFree = true;
		pthread_exit(NULL);
	}   
	pthread_mutex_lock(&launch);
	while(job_id != queArray[LAUNCHING].head->data.ID) {}
	pthread_mutex_lock(&padA_lock);
	isPadAFree = false;
	int endTime;
	int turnAroundTime;
	struct timeval tp;
	int sleep = 4;
	while(sleep != 0) {//not sure if the job start from scratch or continue from where it stopped.
		while(!isEmpty(&queArray[EMERGENCY])){
			isPadAFree = true;
			pthread_cond_wait(&condA, &padA_lock);
			isPadAFree = false;
		}
		pthread_sleep(1);
		sleep--;
	}
	gettimeofday(&tp, NULL);
	endTime = tp.tv_sec - simulation_start_time;
	turnAroundTime = endTime - request_time[job_id];
	char b[5];
	char c[5];
	my_itoa(endTime,b);
	strcat(logArray[job_id],b);
	strcat(logArray[job_id],"\t\t");
			
	my_itoa(turnAroundTime,c);
	strcat(logArray[job_id],c);
	strcat(logArray[job_id],"\t\t");

	strcat(logArray[job_id],"A");
	Dequeue(&queArray[LAUNCHING]);
	isPadAFree = true;
	pthread_exit(NULL);
}

// the function that creates plane threads for emergency landing
void* EmergencyJob(void *id){
	int job_id = *((int *) id);
	int isLand = 0;
	pthread_mutex_lock(&emergency);	
	do {			
		if(isEmrgncyA) 
		{   
			isEmrgncyA = false;
			pthread_mutex_unlock(&emergency);		
			pthread_mutex_lock(&padA_lock);
			isPadAFree = false;
			int endTime;
			int turnAroundTime;
			struct timeval tp;
			pthread_sleep(2);
			gettimeofday(&tp, NULL);
			endTime = tp.tv_sec - simulation_start_time;
			turnAroundTime = endTime - request_time[job_id];
			char b[5];
			char c[5];
			my_itoa(endTime,b);
			strcat(logArray[job_id],b);
			strcat(logArray[job_id],"\t\t");
			
			my_itoa(turnAroundTime,c);
			strcat(logArray[job_id],c);
			strcat(logArray[job_id],"\t\t");

			strcat(logArray[job_id],"A");
			Dequeue(&queArray[EMERGENCY]);
			isPadAFree = true; 
			pthread_cond_broadcast(&condA);
			pthread_exit(NULL);			
		}
		if(isEmrgncyB)         
		{
			isEmrgncyB = false;
			pthread_mutex_unlock(&emergency);	
			pthread_mutex_lock(&padB_lock);
			isPadBFree = false;
			int endTime;
			int turnAroundTime;
			struct timeval tp;
			pthread_sleep(2);
			gettimeofday(&tp, NULL);
			endTime = tp.tv_sec - simulation_start_time;
			turnAroundTime = endTime - request_time[job_id];
			char b[5];
			char c[5];
			my_itoa(endTime,b);
			strcat(logArray[job_id],b);
			strcat(logArray[job_id],"\t\t");
			
			my_itoa(turnAroundTime,c);
			strcat(logArray[job_id],c);
			strcat(logArray[job_id],"\t\t");

			strcat(logArray[job_id],"B");
			Dequeue(&queArray[EMERGENCY]);
			isPadBFree = true;
			pthread_cond_broadcast(&condB);
			pthread_exit(NULL);						
		}				
	} while(!isLand);	
}

// the function that creates plane threads for emergency landing
void* AssemblyJob(void *id){
	pthread_mutex_lock(&assembly);
	int job_id = *((int *) id);	
	while(job_id != queArray[ASSEMBLY].head->data.ID) {}
	pthread_mutex_lock(&padB_lock);
	isPadBFree = false;
	int endTime;
	int turnAroundTime;
	struct timeval tp;
	int sleep = 12;
	while(sleep != 0){ //not sure if the job start from scratch or continue from where it stopped.
		while(!isEmpty(&queArray[EMERGENCY])){
			isPadBFree = true;
			pthread_cond_wait(&condB, &padB_lock);
			isPadBFree = false;
		}
		pthread_sleep(1);
		sleep--;
	}
	
	gettimeofday(&tp, NULL);
	endTime = tp.tv_sec - simulation_start_time;
	turnAroundTime = endTime - request_time[job_id];
	char b[5];
	char c[5];
	my_itoa(endTime,b);
	strcat(logArray[job_id],b);
	strcat(logArray[job_id],"\t\t");
			
	my_itoa(turnAroundTime,c);
	strcat(logArray[job_id],c);
	strcat(logArray[job_id],"\t\t");

	strcat(logArray[job_id],"B");
	Dequeue(&queArray[ASSEMBLY]);
	isPadBFree = true;
	pthread_exit(NULL);
}

// the function that controls the air traffic
void* ControlTower(void *arg){
	pthread_mutex_lock(&first_job_mutex);
	struct timeval tp;
	gettimeofday(&tp, NULL);
	simulation_end_time = tp.tv_sec + simulationTime;
	while(tp.tv_sec < simulation_end_time)
	{
		if(!isEmpty(&queArray[EMERGENCY]) && (isPadAFree && isPadBFree)) //controls emergency
		{ 			
			pthread_mutex_unlock(&padA_lock);
			isPadAFree = false;
			isEmrgncyA = true;
			pthread_mutex_unlock(&padB_lock);
			isPadBFree = false;
			isEmrgncyB = true;			
		}
		if((!isEmpty(&queArray[LANDING]) && (isPadAFree || isPadBFree)) && (queArray[LAUNCHING].size < 3 || queArray[ASSEMBLY].size < 3) && isEmpty(&queArray[EMERGENCY])) //controls landing
		{
			
			if(isPadAFree) 
			{
				pthread_mutex_unlock(&land);
				pthread_mutex_unlock(&padA_lock);
			}
			if(isPadBFree)
			{
				pthread_mutex_unlock(&land);
				pthread_mutex_unlock(&padB_lock);
			}    
		}
		if((queArray[LAUNCHING].size >= 3 || (isEmpty(&queArray[LANDING]) && !isEmpty(&queArray[LAUNCHING]))) && isPadAFree && isEmpty(&queArray[EMERGENCY])) //controls launching
		{
			pthread_mutex_unlock(&launch);
			pthread_mutex_unlock(&padA_lock); 
		}
		if((queArray[ASSEMBLY].size >= 3 || (isEmpty(&queArray[LANDING]) && !isEmpty(&queArray[ASSEMBLY]))) && isPadBFree && isEmpty(&queArray[EMERGENCY])) //controls assembling
		{
			pthread_mutex_unlock(&assembly);
			pthread_mutex_unlock(&padB_lock);
		}

		gettimeofday(&tp, NULL);
		pthread_sleep(1); //to not confuse jobs sleep 1 second in each job handled
	}   
	pthread_exit(NULL);
}

void CreateJob(int type, Queue queArray[]){
	Job j;
	j.type = type;
	j.ID = ID;
	int *idPtr = (int *) malloc(sizeof(int));
	*idPtr = ID;
	
	switch (type) {
		case LAUNCHING:
			Enqueue(&queArray[0], j);
			pthread_create(&(tid[threadCount]), NULL, &LaunchJob, (void *) idPtr);
			break;
		case LANDING:
			Enqueue(&queArray[1], j);
			pthread_create(&(tid[threadCount]), NULL, &LandingJob, (void *) idPtr);
			break;
		case ASSEMBLY:
			Enqueue(&queArray[2], j);
			pthread_create(&(tid[threadCount]), NULL, &AssemblyJob, (void *) idPtr);
			break;
		case EMERGENCY:
			Enqueue(&queArray[3], j);
			pthread_create(&(tid[threadCount]), NULL, &EmergencyJob, (void *) idPtr);
			break;
	}

	struct timeval tp;
	gettimeofday(&tp, NULL);
	request_time[ID] = tp.tv_sec - simulation_start_time;

	char b[5];
	my_itoa(ID,b);
	strcat(logArray[ID],"\t");
	strcat(logArray[ID],b); //put job id to log array
	strcat(logArray[ID],"\t\t");

	typeJob = (char*)malloc(1000*sizeof(char));
	if(type == LAUNCHING) strcat(typeJob, "D\0");
	if(type == LANDING) strcat(typeJob, "L\0");
	if(type == ASSEMBLY) strcat(typeJob, "A\0");
	if(type == EMERGENCY) strcat(typeJob, "E\0");
	strcat(logArray[ID],typeJob); //put job type to log array
	strcat(logArray[ID],"\t\t");
	free(typeJob);
	
	my_itoa(request_time[ID],b);
	strcat(logArray[ID],b); //put request time of the job to log array
	strcat(logArray[ID],"\t\t");

	ID+=1;
	threadCount++;
}

float randomNumGenerator() {
		return (float)rand() / RAND_MAX;
}

void printLog(Queue queArray[]) {
	FILE *fp = fopen("events.log", "w");
	fprintf(fp, "%s", " EventID  |  Status | Request Time | End Time | Turnaround Time | Pad\n");
	fprintf(fp, "%s", "______________________________________________________________________\n");

	for(int i = 0; i < ID; i++){
		fprintf(fp, "%s\n", logArray[i]); // write log array to events.log file
	}
	free(logArray);
}

char *my_itoa(int num, char *str) {
		if(str == NULL)
		{
				return NULL;
		}
		sprintf(str, "%d", num);
		return str;
}