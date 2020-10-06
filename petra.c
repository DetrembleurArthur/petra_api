#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>


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


int open_petra(PetraDriver *petra, const char *driver);
void close_petra(PetraDriver *petra);

void write_petra(PetraDriver *petra);
void read_petra(PetraDriver *petra);


void switch_roller1(PetraDriver *petra);
void switch_roller2(PetraDriver *petra);
void switch_sucker(PetraDriver *petra);
void switch_arm(PetraDriver *petra);
void switch_diver(PetraDriver *petra);
void switch_hook(PetraDriver *petra);
void switch_rollArm(PetraDriver *petra, enum Position position);

int get_sensor1(const PetraDriver *petra);
int get_sensor2(const PetraDriver *petra);
int get_diver(const PetraDriver *petra);
int get_arm(const PetraDriver *petra);
int get_tub(const PetraDriver *petra);
int get_chariot(const PetraDriver *petra);
int get_slot(const PetraDriver *petra);
unsigned char get_sensors(const PetraDriver *petra);

void print_petra(const PetraDriver *petra);




int open_petra(PetraDriver *petra, const char *driver)
{
	if((petra->in = open(driver, O_RDONLY)) == -1)
	{
		return -1;
	}
	if((petra->out = open(driver, O_WRONLY)) == -1)
	{
		close(petra->in);
		return -1;
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


int get_sensor1(const PetraDriver *petra)
{
	return petra->sensors.bits.sensor1;
}

int get_sensor2(const PetraDriver *petra)
{
	return petra->sensors.bits.sensor2;
}

int get_diver(const PetraDriver *petra)
{
	return petra->sensors.bits.diver;
}

int get_arm(const PetraDriver *petra)
{
	return petra->sensors.bits.arm;
}

int get_tub(const PetraDriver *petra)
{
	return petra->sensors.bits.tub;
}

int get_chariot(const PetraDriver *petra)
{
	return petra->sensors.bits.chariot;
}

int get_slot(const PetraDriver *petra)
{
	return petra->sensors.bits.slot;
}

unsigned char get_sensors(const PetraDriver *petra)
{
	return petra->sensors.byte;
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


