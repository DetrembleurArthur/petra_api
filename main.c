#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <sys/socket.h>
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
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
void reset_petra(PetraDriver *petra);
void print_petra(const PetraDriver *petra);
void _log(const char*, int, const char*);
void _logerr(const char*, int, const char*);
void petra_handler(void);
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
pthread_t sensors_watcher_thread = 0L;

int main(int argc, char const *argv[])
{
	const unsigned short PORT = argc > 2 ? atoi(argv[1]) : 50000;
	const char *IP = argc > 3 ? argv[2] : "10.59.28.2";

	log(IP);

	struct sigaction interrupt = {0};
	interrupt.sa_handler = interrupt_main;
	sigaction(SIGINT, &interrupt, NULL);

	int rc = open_petra(&petra, SENSORS_FILE, ACTUATORS_FILE);
	if(!rc)
	{
		pthread_mutex_init(&petra_mutex, NULL);
		reset_petra(&petra);

		petraWriteSocketServer = create_server(IP, PORT, &addr_in_w);
		if(petraWriteSocketServer == -1)
		{
			close_petra(&petra);
			exit(EXIT_FAILURE);
		}

		petraReadSocketServer = create_server(IP, PORT+1, &addr_in_r);
		if(petraReadSocketServer == -1)
		{
			close(petraWriteSocketServer);
			close_petra(&petra);
			exit(EXIT_FAILURE);
		}
		
		petraWriteSocketClient = accept_client(petraWriteSocketServer, &addr_in_w);
		if(petraWriteSocketClient == -1)
		{
			close(petraWriteSocketServer);
			close(petraReadSocketServer);
			close_petra(&petra);
			exit(EXIT_FAILURE);
		}

		petraReadSocketClient = accept_client(petraReadSocketServer, &addr_in_r);
		if(petraReadSocketClient == -1)
		{
			close(petraWriteSocketClient);
			close(petraWriteSocketServer);
			close(petraReadSocketServer);
			close_petra(&petra);
			exit(EXIT_FAILURE);
		}

		petra_handler();

		reset_petra(&petra);

		pthread_mutex_destroy(&petra_mutex);

		close(petraWriteSocketClient);
		close(petraReadSocketClient);
		close(petraWriteSocketServer);
		close(petraReadSocketServer);
		close_petra(&petra);
		log("socket closed");
	}

	return 0;
}

void reset_petra(PetraDriver *petra)
{
	petra->actuators.byte = 0x0;
	write_petra(petra);
}

void interrupt_main(int sig)
{
	if(sensors_watcher_thread != 0)
	{
		pthread_kill(sensors_watcher_thread, SIGUSR1);
		pthread_join(sensors_watcher_thread, NULL);
		log("join thread");
	}
	pthread_mutex_destroy(&petra_mutex);
	reset_petra(&petra);
	if(petraWriteSocketClient != -1)
		close(petraWriteSocketClient);
	if(petraReadSocketClient != -1)
		close(petraReadSocketClient);
	if(petraWriteSocketServer != -1)
		close(petraWriteSocketServer);
	if(petraReadSocketServer != -1)
		close(petraReadSocketServer);
	close_petra(&petra);
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
	pthread_mutex_unlock(&petra_mutex);
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
		usleep(100000);
	}
	return NULL;
}

void petra_handler(void)
{
	log("Petra handler started");

	int running = 1;
	int auto_commit = 1;
	PetraActions action = NO_DATA;

	send(petraWriteSocketClient, &petra.actuators.byte, sizeof(unsigned char), 0);
	int rc = 0;
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
				petra.actuators.bits.roller1 = !petra.actuators.bits.roller1;
				break;
			case ROLLER2:
				petra.actuators.bits.roller2 = !petra.actuators.bits.roller2;
				break;
			case SUCKER:
				petra.actuators.bits.sucker = 1;
				break;
			case TUB:
				petra.actuators.bits.diver = !petra.actuators.bits.diver;
				break;
			case ARM:
				petra.actuators.bits.arm = !petra.actuators.bits.arm;
				break;
			case BLOCKER:
				petra.actuators.bits.hook = !petra.actuators.bits.hook;
				break;
			case ARM_TUB:
				petra.actuators.bits.roller_arm = RES;
				break;
			case ARM_R1:
				petra.actuators.bits.roller_arm = R1;
				break;
			case ARM_R2:
				petra.actuators.bits.roller_arm = R2;
				break;
			case ARM_R1R2:
				petra.actuators.bits.roller_arm = R1R2;
				break;
			case AUTO_COMMIT:
				auto_commit = !auto_commit;
				break;
			case COMMIT:
				write_petra(&petra);
				printf("commit writting\n");
				break;
			case EXIT:
				action = NO_PA;
				running = 0;
				log("exit");
				break;
			default:
				log("ignore");
		}
		if(action && auto_commit)
		{
			write_petra(&petra);
			printf("autocommit writting\n");
		}
		send(petraWriteSocketClient, &petra.actuators.byte, sizeof(unsigned char), 0);
		printf("Actuators buffer: %hhu\n", petra.actuators.byte);
		pthread_mutex_unlock(&petra_mutex);
		action = NO_PA;
	}
	
	if(sensors_watcher_thread != 0)
	{
		pthread_kill(sensors_watcher_thread, SIGUSR1);
		pthread_join(sensors_watcher_thread, NULL);
	}
	
	
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
	petra->actuators.bits.sucker = 0x0;
}

void read_petra(PetraDriver *petra)
{
	read(petra->in, &petra->sensors.byte, sizeof(unsigned char));
}

void _log(const char *func, int line, const char *msg)
{
	fprintf(stderr, "\n[INFO] pid(%d) > func(%s) > line(%d) :\n\t%s\n\n", getpid(), func, line, msg);
}

void _logerr(const char *func, int line, const char *msg)
{
	fprintf(stderr, "\n[ERROR] pid(%d) > func(%s) > line(%d) :\n\t", getpid(), func, line);
	perror(msg);
	fprintf(stderr, "\n");
}
