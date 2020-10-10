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

#define log(msg) _log(__func__, __LINE__, msg)
#define logerr(msg) _logerr(__func__, __LINE__, msg)

#define CONVOYEUR1 u_act.act.C1
#define CONVOYEUR2 u_act.act.C2
#define VENTOUSE u_act.act.PV
#define ACTIONNEURS u_act.byte
#define ACT_BRAS u_act.act.AA
#define ACT_PLONGEUR u_act.act.PA
#define GRAPPIN u_act.act.GA
#define BRAS_COULISSANT u_act.act.CP

#define CAPTEUR1 u_capt.capt.L1
#define CAPTEUR2 u_capt.capt.L2
#define PLONGEUR u_capt.capt.PP
#define BRAS u_capt.capt.AP
#define BAC u_capt.capt.DE
#define CHARIOT u_capt.capt.CS
#define SLOT u_capt.capt.S
#define CAPTEURS u_capt.byte

#define ACTUATORS_FILE "/dev/actuateursPETRA"
#define SENSORS_FILE "/dev/capteursPETRA"

struct actuateurs
{
	unsigned CP : 2;
	unsigned C1 : 1;
	unsigned C2 : 1;
	unsigned PV : 1;
	unsigned PA : 1;
	unsigned AA : 1;
	unsigned GA : 1;
};

struct capteurs
{
	unsigned L1 : 1;
	unsigned L2 : 1;
	unsigned T  : 1;
	unsigned S  : 1;
	unsigned CS : 1;
	unsigned AP : 1;
	unsigned PP : 1;
	unsigned DE : 1;
};

union
{
	struct capteurs capt;
	unsigned char byte;
} u_capt;

union
{
	struct actuateurs act;
	unsigned char byte;
} u_act;


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
	ARM_R1R2
} PetraActions;


void _log(const char*, int, const char*);
void _logerr(const char*, int, const char*);

void petra_hanler(int socketClient);




int main(int argc, char const *argv[])
{
	int socketServer = -1;
	struct sockaddr_in addr_in = {0};
	struct in_addr ip = {0};
	struct hostent *infosHost = NULL;

	int socketClient = -1;
	socklen_t len = 0;

	int petra_in = -1;
	int petra_out = -1;

	if ((petra_out = open(ACTUATORS_FILE, O_WRONLY)) == -1)
	{
		logerr("Actuators file not open!\n");
		exit(EXIT_FAILURE);
	}
	log("Actuators file open!\n");


	if ((petra_in = open(SENSORS_FILE, O_RDONLY)) == -1)
	{
		logerr("Sensors file not open!\n");
		close(petra_out);
		exit(EXIT_FAILURE);
	}
	log("Sensors file open!\n");

	socketServer = socket(AF_INET, SOCK_STREAM, 0);
	if(socketServer == -1)
	{
		logerr("socket creation failed");
		exit(EXIT_FAILURE);
	}
	log("socket creation success");
	log("Petra ready");

	addr_in.sin_family = AF_INET; 
    infosHost = gethostbyname("127.0.0.1");
	if(infosHost == NULL)  
    { 
       logerr("gethostbyname failed");
       close(socketServer);
       exit(EXIT_FAILURE);
    }

    memcpy(&ip, infosHost->h_addr, infosHost->h_length);
    log(inet_ntoa(ip));
    addr_in.sin_port = htons(6666);
    memcpy(&addr_in.sin_addr, infosHost->h_addr, infosHost->h_length);
   
   	if(bind(socketServer, (const struct sockaddr *) &addr_in, sizeof(addr_in)) < 0)
   	{
   		logerr("binding failed");
   		close(socketServer);
   		exit(EXIT_FAILURE);
   	}
   	log("binding success");

	if(listen(socketServer, SOMAXCONN) < 0)
	{
		logerr("listen error");
		close(socketServer);
		exit(EXIT_FAILURE);
	}

	len = sizeof(addr_in);
	if((socketClient = accept(socketServer, (struct sockaddr *) &addr_in, &len)) < 0)
	{
		logerr("accept failed");
		close(socketServer);
		exit(EXIT_FAILURE);
	}

	petra_hanler(socketClient);



   	close(socketServer);
   	log("socket closed");



	return 0;
}


void petra_hanler(int socketClient)
{
	log("Petra handler started");

	int running = 1;
	int action = -1;
	while(running)
	{
		if(recv(socketClient, (void *)&action, sizeof(int), 0) == -1)
		{
			logerr("recv error");
			close(socketClient);
			break;
		}
		/*
		NO_PA,
		ROLLER1,
		ROLLER2,
		SUCKER,
		TUB,
		ARM,
		BLOCKER,
		ROLL_ARM
		*/
		switch(action)
		{
			case ROLLER1:
				CONVOYEUR1 = !CONVOYEUR1;
				break;
			case ROLLER2:
				CONVOYEUR2 = !CONVOYEUR2;
				break;
			case SUCKER:
				VENTOUSE = !VENTOUSE;
				break;
			case TUB:
				ACT_PLONGEUR = !ACT_PLONGEUR;
				break;
			case ARM:
				ACT_BRAS = !ACT_BRAS;
				break;
			case BLOCKER:
				GRAPPIN = !GRAPPIN;
				break;
			case ARM_TUB:
				break;
			case ARM_R1:
				break;
			case ARM_R2:
				break;
			case ARM_R1R2:
				break;

		}
	}

	log("Petra handler stop");
}


void printCaptors()
{
	read(petra_captors_in, &CAPTEURS, sizeof(unsigned char));
	printf("Captors information\n");
	printf("Input tray => %d\n", BAC); //bac d'entrée
	printf("Carriage in position => %d\n", CHARIOT);
	printf("Position plongeur => %d\n", PLONGEUR);
	printf("Slot detection sensor => %d\n", SLOT);
	printf("Cut-out detection sensors L1 => %d", CAPTEUR1);
	printf("Cut-out detection sensors L2 => %d", CAPTEUR2);
	printf("Arm position = %d", BRAS);
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
