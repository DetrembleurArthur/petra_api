#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <netdb.h>
#include<errno.h>
#include<fcntl.h>
#include<signal.h>
#include <pthread.h>

#define log(msg) _log(__func__, __LINE__, msg)
#define logerr(msg) _logerr(__func__, __LINE__, msg)


#define ACTUATORS_FILE "/dev/actuateursPETRA"
#define SENSORS_FILE "/dev/capteursPETRA"

typedef enum
{
	NO_PA,
	ROLLER1,
	ROLLER2,
	SUCKER,
	TUB,
	ARM,
	BLOCKER,
	ARM_TUB,
	ARM_R1,
	ARM_R2,
	ARM_R1R2,
	AUTO_COMMIT,
	COMMIT,
	EXIT
} PetraActions;


enum Position
{
	RES,
	R1R2,
	R1,
	R2
};


struct Actuators
{
	unsigned roller_arm : 2; //weak bits
	unsigned roller1 : 1;
	unsigned roller2 : 1;
	unsigned sucker : 1;
	unsigned diver : 1;
	unsigned arm : 1;
	unsigned hook : 1; //heavy bit
};

struct Sensors
{
	unsigned sensor1 : 1;
	unsigned sensor2 : 1;
	unsigned T  : 1;
	unsigned slot  : 1;
	unsigned chariot : 1;
	unsigned arm : 1;
	unsigned diver : 1;
	unsigned tub : 1;
};

union USensors
{
	struct Sensors bits;
	unsigned char byte;
};

union UActuators
{
	struct Actuators bits;
	unsigned char byte;
};


typedef struct
{
	union USensors sensors;
	union UActuators actuators;
	int in;
	int out;
} PetraDriver;



int open_petra(PetraDriver *petra, const char *driver_in, const char *driver_out);
void close_petra(PetraDriver *petra);

void write_petra(PetraDriver *petra);
void read_petra(PetraDriver *petra);


void switch_roller1(PetraDriver *petra);
void switch_roller2(PetraDriver *petra);void switch_sucker(PetraDriver *petra);
void switch_arm(PetraDriver *petra);
void switch_diver(PetraDriver *petra);
void switch_hook(PetraDriver *petra);
void switch_rollArm(PetraDriver *petra, enum Position position);


void print_petra(const PetraDriver *petra);



void _log(const char*, int, const char*);
void _logerr(const char*, int, const char*);

void petra_hanler(void);
void *sensors_watcher_handler(void *);


int create_server(const char *ip, unsigned short port, struct sockaddr_in *addr_in);
int accept_client(int socketServer, struct sockaddr_in *addr_in);

void interrupt_main(int sig);

pthread_mutex_t petra_mutex;
struct sockaddr_in addr_in_w;
int petraWriteSocketServer = -1;
int petraWriteSocketClient = -1;
struct sockaddr_in addr_in_r;
int petraReadSocketServer = -1;
int petraReadSocketClient = -1;
PetraDriver petra;
pthread_t sensors_watcher_thread;

int main(int argc, char const *argv[])
{
	const unsigned short PORT = 50000;
	const char *IP = "127.0.0.1";

	struct sigaction interrupt = {0};
	interrupt.sa_handler = interrupt_main;
	sigaction(SIGINT, &interrupt, NULL);

	pthread_mutex_init(&petra_mutex, NULL);


	petraWriteSocketServer = create_server(IP, PORT, &addr_in_w);
	if(petraWriteSocketServer == -1)
	{
		exit(EXIT_FAILURE);
	}

	petraReadSocketServer = create_server(IP, PORT+1, &addr_in_r);
	if(petraReadSocketServer == -1)
	{
		close(petraWriteSocketServer);
		exit(EXIT_FAILURE);
	}
	
	petraWriteSocketClient = accept_client(petraWriteSocketServer, &addr_in_w);
	if(petraWriteSocketClient == -1)
	{
		close(petraWriteSocketServer);
		close(petraReadSocketServer);
		exit(EXIT_FAILURE);
	}

	petraReadSocketClient = accept_client(petraReadSocketServer, &addr_in_r);
	if(petraReadSocketClient == -1)
	{
		close(petraWriteSocketClient);
		close(petraWriteSocketServer);
		close(petraReadSocketServer);
		exit(EXIT_FAILURE);
	}

	petra_hanler();

	pthread_mutex_destroy(&petra_mutex);

	close(petraWriteSocketClient);
	close(petraReadSocketClient);
   	close(petraWriteSocketServer);
	close(petraReadSocketServer);
   	log("socket closed");



	return 0;
}

void interrupt_main(int sig)
{
	pthread_kill(sensors_watcher_thread, SIGUSR1);
	pthread_join(sensors_watcher_thread, NULL);

	pthread_mutex_destroy(&petra_mutex);

	if(petraWriteSocketClient != -1)
		close(petraWriteSocketClient);
	if(petraReadSocketClient != -1)
		close(petraReadSocketClient);
	if(petraWriteSocketServer != -1)
   		close(petraWriteSocketServer);
	if(petraReadSocketServer != -1)
		close(petraReadSocketServer);
	log("closed by interrupt");
	exit(EXIT_SUCCESS);
}

int accept_client(int socketServer, struct sockaddr_in *addr_in)
{
	int socketClient = -1;
	socklen_t len = 0;
	if(listen(socketServer, SOMAXCONN) < 0)
	{
		logerr("listen error");
		close(socketServer);
		return -1;
	}

	log("wait client");
	len = sizeof(struct sockaddr_in);
	if((socketClient = accept(socketServer, (struct sockaddr *) addr_in, &len)) < 0)
	{
		logerr("accept failed");
		close(socketServer);
		return -1;
	}
	log("client catched");
	return socketClient;
}

int create_server(const char *ip, unsigned short port, struct sockaddr_in *addr_in)
{
	int socketServer = -1;
	struct hostent *infosHost = NULL;
	memset((void *)addr_in, 0, sizeof(struct sockaddr_in));
	

	socketServer = socket(AF_INET, SOCK_STREAM, 0);
	if(socketServer == -1)
	{
		logerr("socket creation failed");
		return -1;
	}
	log("socket creation success");

	addr_in->sin_family = AF_INET; 
    infosHost = gethostbyname(ip);
	if(infosHost == NULL)  
    { 
       logerr("gethostbyname failed");
       close(socketServer);
       return -1;
    }


    addr_in->sin_port = htons(port);
    memcpy(&addr_in->sin_addr, infosHost->h_addr, infosHost->h_length);
   
   	if(bind(socketServer, (const struct sockaddr *) addr_in, sizeof(struct sockaddr_in )) < 0)
   	{
   		logerr("binding failed");
   		close(socketServer);
   		return -1;
   	}
   	log("binding success");
	return socketServer;
}


void sensors_watcher_interrupt(int sig)
{
	log("sensors watcher interrupt");
	int rc = 0xff;
	//envoi du signal de fin
	send(petraReadSocketClient, &rc, sizeof(unsigned char), 0);
	pthread_exit(NULL);
}

void *sensors_watcher_handler(void *_)
{
	int running = 1;

	struct sigaction interrupt;
	interrupt.sa_handler = sensors_watcher_interrupt;
	sigaction(SIGUSR1, &interrupt, NULL);

	while(running)
	{
		pthread_mutex_lock(&petra_mutex);
		read_petra(&petra);
		if(send(petraReadSocketClient, &petra.sensors.byte, sizeof(unsigned char), 0) == -1)
		{
			running = 0;
			logerr("watcher end");
		}
		pthread_mutex_unlock(&petra_mutex);
	}
	return NULL;
}

void petra_hanler(void)
{
	log("Petra handler started");

	petra.actuators.byte = 0x0;
	petra.sensors.byte = 0x0;

	int running = 1;
	int auto_commit = 1;
	PetraActions action = NO_DATA;

	int rc = open_petra(&petra, SENSORS_FILE, ACTUATORS_FILE);
	//envoi de l'état de l'ouverture du driver
	send(petraWriteSocketClient, (void *)&rc, sizeof(int), 0);
	if(!rc)
	{
		pthread_create(&sensors_watcher_thread, NULL, sensors_watcher_handler, NULL);
		while(running)
		{
			if(recv(petraWriteSocketClient, (void *)&action, sizeof(int), 0) == -1)
			{
				logerr("recv error");
				break;
			}
			pthread_mutex_lock(&petra_mutex);
			switch(action)
			{
				case ROLLER1:
					switch_roller1(&petra);
					break;
				case ROLLER2:
					switch_roller2(&petra);
					break;
				case SUCKER:
					switch_sucker(&petra);
					break;
				case TUB:
					switch_diver(&petra);
					break;
				case ARM:
					switch_arm(&petra);
					break;
				case BLOCKER:
					switch_hook(&petra);
					break;
				case ARM_TUB:
					switch_rollArm(&petra, RES);
					break;
				case ARM_R1:
					switch_rollArm(&petra, R1);
					break;
				case ARM_R2:
					switch_rollArm(&petra, R2);
					break;
				case ARM_R1R2:
					switch_rollArm(&petra, R1R2);
					break;
				case AUTO_COMMIT:
					auto_commit = !auto_commit;
					break;
				case COMMIT:
					write_petra(&petra);
					break;
				case EXIT:
					action = NO_PA;
					running = 0;
					break;
				default:
					log("ignore");
			}
			if(action && auto_commit)
			{
				write_petra(&petra);
			}
			pthread_mutex_unlock(&petra_mutex);
			action = NO_PA;
		}
	}
	pthread_kill(sensors_watcher_thread, SIGUSR1);
	pthread_join(sensors_watcher_thread, NULL);
	
	log("Petra handler stop");
}



int open_petra(PetraDriver *petra, const char *driver_in, const char *driver_out)
{
	if((petra->in = open(driver_in, O_RDONLY)) == -1)
	{
		logerr("driver in not exists");
		return -1;
	}
	if((petra->out = open(driver_out, O_WRONLY)) == -1)
	{
		logerr("driver out not exists");
		close(petra->in);
		return -2;
	}
	return 0;
}

void close_petra(PetraDriver *petra)
{
	close(petra->in);
	close(petra->out);
}


void write_petra(PetraDriver *petra)
{
	write(petra->out, &petra->actuators.byte, sizeof(unsigned char));
}

void read_petra(PetraDriver *petra)
{
	read(petra->in, &petra->sensors.byte, sizeof(unsigned char));
}


void switch_roller1(PetraDriver *petra)
{
	petra->actuators.bits.roller1 = !petra->actuators.bits.roller1;
}

void switch_roller2(PetraDriver *petra)
{
	petra->actuators.bits.roller2 = !petra->actuators.bits.roller2;
}

void switch_sucker(PetraDriver *petra)
{
	petra->actuators.bits.sucker = !petra->actuators.bits.sucker;
}

void switch_arm(PetraDriver *petra)
{
	petra->actuators.bits.arm = !petra->actuators.bits.arm;
}

void switch_diver(PetraDriver *petra)
{
	petra->actuators.bits.diver = !petra->actuators.bits.diver;
}

void switch_hook(PetraDriver *petra)
{
	petra->actuators.bits.hook = !petra->actuators.bits.hook;
}

void switch_rollArm(PetraDriver *petra, enum Position position)
{
	if(position >= 0 && position <= 3)
	{
		petra->actuators.bits.roller_arm = position;
	}
}



void print_petra(const PetraDriver *petra)
{
	
	printf("\033[1;1Hsensors  : ");
	for(int i = 0; i < 8; i++)
	{
		printf("%u", (petra->sensors.byte >> (7 - i)) & 1);
	}
	printf("\033[2;1Hactuators: ");
	for(int i = 0; i < 8; i++)
	{
		printf("%u", (petra->actuators.byte >> (7 - i)) & 1);
	}
	printf("\n");
}





/*-----------------------------------------------------------------------------------
 * Affichage pour le débugging
 -----------------------------------------------------------------------------------*/
void _log(const char *func, int line, const char *msg)
{
	fprintf(stderr, "\n[INFO] pid(%d) > func(%s) > line(%d) :\n\t%s\n\n", getpid(), func, line, msg);
}

/*-----------------------------------------------------------------------------------
 * Affichage pour le débugging + message errno
 -----------------------------------------------------------------------------------*/
void _logerr(const char *func, int line, const char *msg)
{
	fprintf(stderr, "\n[ERROR] pid(%d) > func(%s) > line(%d) :\n\t", getpid(), func, line);
	perror(msg);
	fprintf(stderr, "\n");
}
