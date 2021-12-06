#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "SerialManager.h"
#include "myTypes.h"
#include "macros.h"

// VARIABLES DE ENTORNO ///////////////////////////////////////////////////////

int pnSerial;
int baudrate;
int pnTCP;

///////////////////////////////////////////////////////////////////////////////

int killme = 0;
int newfd = -1;
char lines[] = {OUT_OFF, OUT_OFF, OUT_OFF, OUT_OFF};
pthread_mutex_t mutexOuts = PTHREAD_MUTEX_INITIALIZER;

void signal_handler(int signal);

void *taskTCP(void *param);

void *taskSERIAL(void *param);

int main(int argc, char *argv[])
{
	printf("MAI - START\n\r");

	// VERIFICACIÓN DE ARGUMENTOS /////////////////////////////////////////////
	if (5 != argc)
	{
		perror("MAI - INVALID NUMBER OF ARGUMENTS\n\r");
		exit(EXIT_FAILURE);
	}

	pnSerial = atoi(argv[1]);
	if ((pnSerial > 21) || (pnSerial < 0))
	{
		perror("MAI - INVALID SERIAL PORT\n\r");
		exit(EXIT_FAILURE);
	}

	baudrate = atoi(argv[2]);
	if (INVALID_BAUD(baudrate))
	{
		perror("MAI - INVALID BAUDRATE\n\r");
		exit(EXIT_FAILURE);
	}

	pnTCP = atoi(argv[3]);
	if (pnTCP < 0)
	{
		perror("MAI - INVALID TCP PORT\n\r");
	}
	///////////////////////////////////////////////////////////////////////////

	// CONFIGURACIÓN DEL MANEJO DE SEÑALES ////////////////////////////////////
	sigset_t sigset;
	struct sigaction sa;

	sa.sa_handler = signal_handler;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sa.sa_mask);

	if (-1 == sigaction(SIGINT, &sa, NULL))
	{
		perror("MAI - SIGINT config\n\r");
		exit(EXIT_FAILURE);
	}

	if (-1 == sigaction(SIGTERM, &sa, NULL))
	{
		perror("MAI - SIGTERM config\n\r");
		exit(EXIT_FAILURE);
	}
	///////////////////////////////////////////////////////////////////////////

	// LOCK DE SEÑALES ////////////////////////////////////////////////////////
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigaddset(&sigset, SIGTERM);
	pthread_sigmask(SIG_BLOCK, &sigset, NULL);
	///////////////////////////////////////////////////////////////////////////

	// CREACIÓN DE THREADS ////////////////////////////////////////////////////
	int res;
	const char *msgSER = "SER";
	const char *msgTCP = "TCP";
	pthread_t serialThread;
	pthread_t tcpThread;

	res = pthread_create(&serialThread, NULL, taskSERIAL, (void *)msgSER);
	if (0 != res)
	{
		perror("MAI - THREAD SER creation error\n\r");
		exit(EXIT_FAILURE);
	}
	printf("MAI - THREAD SER created\n\r");

	res = pthread_create(&tcpThread, NULL, taskTCP, (void *)msgTCP);
	if (0 != res)
	{
		perror("MAI - THREAD TCP creation error\n\r");
		exit(EXIT_FAILURE);
	}
	printf("MAI - THREAD TCP created\n\r");
	///////////////////////////////////////////////////////////////////////////

	// UNLOCK DE SEÑALES //////////////////////////////////////////////////////
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigaddset(&sigset, SIGTERM);
	pthread_sigmask(SIG_UNBLOCK, &sigset, NULL);
	///////////////////////////////////////////////////////////////////////////

	// ESPERA POR SEÑAL ///////////////////////////////////////////////////////
	while (0 == killme)
	{
		sleep(1);
	}
	///////////////////////////////////////////////////////////////////////////

	// LIMPIEZA Y CIERRE //////////////////////////////////////////////////////
	printf("\n\r"); // Proligidad en la terminal luego de ^C

	// Cierre de puerto serie
	serial_close();
	printf("MAI - SER closed\n\r");

	// Cierre de socket
	if (-1 != newfd)
	{
		if (close(newfd) < 0)
		{
			perror("MAI - TCP close error\n\r");
			exit(EXIT_FAILURE);
		}
		else
		{
			printf("MAI - TCP closed\n\r");
		}
	}

	// Cancelación de threads
	if (0 != pthread_cancel(serialThread))
	{
		perror("MAI - THREAD SER close error\n\r");
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("MAI - THREAD SER closed\n\r");
	}

	if (0 != pthread_cancel(tcpThread))
	{
		perror("MAI - THREAD TCP close error\n\r");
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("MAI - THREAD TCP closed\n\r");
	}

	// Join de threads
	if (0 != pthread_join(serialThread, NULL))
	{
		perror("MAI - THREAD SER join error\n\r");
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("MAI - THREAD SER joined\n\r");
	}

	if (0 != pthread_join(tcpThread, NULL))
	{
		perror("MAI - THREAD TCP join error\n\r");
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("MAI - THREAD TCP joined\n\r");
	}
	exit(EXIT_SUCCESS);
	///////////////////////////////////////////////////////////////////////////
}

void signal_handler(int signal)
{
	killme = ((SIGINT == signal) || (SIGTERM == signal));
}

void *taskTCP(void *param)
{
	socklen_t addrSize;
	struct sockaddr_in addrCli;
	struct sockaddr_in addrSer;

	// Socket
	int s = socket(PF_INET, SOCK_STREAM, 0);

	// Server IP:PORT
	bzero((char *)&addrSer, sizeof(addrSer));
	addrSer.sin_family = AF_INET;
	addrSer.sin_port = htons(pnTCP);
	addrSer.sin_addr.s_addr = inet_addr("127.0.0.1");
	if (addrSer.sin_addr.s_addr == INADDR_NONE)
	{
		perror("TCP - INVALID IP");
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("TCP - SOCKET created\n\r");
	}

	// Abrimos puerto con bind()
	if (-1 == bind(s, (struct sockaddr *)&addrSer, sizeof(addrSer)))
	{
		close(s);
		perror("TCP - BIND error\n\r");
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("TCP - BINDED\n\r");
	}

	if (-1 == listen(s, 10)) // Modo listen
	{
		perror("TCP - LISTEN ERROR\n\r");
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("TCP - LISTENING\n\r");
	}

	while (1)
	{
		// Accept() según lo visto en clase 7
		addrSize = sizeof(struct sockaddr_in);
		if (-1 == (newfd = accept(s, (struct sockaddr *)&addrCli, &addrSize)))
		{
			perror("TCP - ACCEPT ERROR\n\r");
			exit(EXIT_FAILURE);
		}
		else
		{
			printf("TCP - ACCEPTED\n\r");
		}

		int flag = 1;
		int n;
		char buf[128];
		while (1 == flag)
		{
			if ((n = read(newfd, buf, 128)) < 0)
			{
				perror("TCP - READ ERROR\n\r");
				exit(EXIT_FAILURE);
			}
			else if (0 == n)
			{
				break;
			}
			else
			{
				buf[n] = 0;
				printf("TCP - %s\n\r", buf);
			}

			pthread_mutex_lock(&mutexOuts);
			for (char j = 0; j < 4; ++j)
			{
				lines[j] = buf[7 + j] - '0';
			}
			pthread_mutex_unlock(&mutexOuts);
			flag = (0 != buf[0]);
		}

		// Cerramos conexion con cliente
		if (close(newfd) < 0)
		{
			perror("TCP - CLOSE ERROR\n\r");
			exit(EXIT_FAILURE);
		}
		else
		{
			newfd = -1;
			printf("TCP - LOST CONNECTION\n\r");
		}
	}
	exit(EXIT_FAILURE);
}

void *taskSERIAL(void *param)
{
	char bufInp[20];
	char bufOut[20] = ">OUTS:0,0,0,0\r\n";

	char frame[128] = ":LINEXTG\n\0";

	char outs[4] = {OUT_OFF, OUT_OFF, OUT_OFF, OUT_OFF};

	int port;
	port = serial_open(pnSerial, baudrate);

	int flag = 0;
	int status;

	while (0 == port)
	{
		if (0 == flag)
		{
			printf("SER - PORT INIT\n\r");
			flag = 1;
		}

		status = serial_receive(bufInp, 20);
		if (-1 != status && 0 != status)
		{
			if ('T' == bufInp[1]) // Toggle
			{
				printf("SER - %s", bufInp);

				pthread_mutex_lock(&mutexOuts);
				// Suma módulo 2
				outs[bufInp[14] - '0']++;
				if (outs[bufInp[14] - '0'] > 2)
				{
					outs[bufInp[14] - '0'] = 0;
				}
				lines[bufInp[14] - '0'] = outs[bufInp[14] - '0'];
				pthread_mutex_unlock(&mutexOuts);

				bufOut[6] = outs[0] + '0';
				bufOut[8] = outs[1] + '0';
				bufOut[10] = outs[2] + '0';
				bufOut[12] = outs[3] + '0';
				serial_send(bufOut, 15);
				printf("SER - %s\n\r", bufOut);

				if (-1 != newfd)
				{
					frame[5] = bufInp[14];
					if (-1 == write(newfd, frame, sizeof(":LINEXTG\n\0")))
					{
						perror("SER - SOCKET ERROR\n\r");
						exit(EXIT_FAILURE);
					}
					printf("SER - %s\n\r", frame);
				}
			}
			// Recibe ACK
			else if ('O' == bufInp[1])
			{
				printf("SER - %s\n\r", bufInp);
			}
		}
		else
		{
			int cflag = 0;
			for (char i = 0; i < 4; ++i)
			{
				pthread_mutex_lock(&mutexOuts);
				if (outs[i] != lines[i]) // Comparo con el recurso global
				{
					outs[i] = lines[i];
					bufOut[6 + i * 2] = outs[i] + '0'; // Salto las ','
					cflag = 1;
				}
				pthread_mutex_unlock(&mutexOuts);
			}
			if (1 == cflag)
			{
				serial_send(bufOut, 15);
				printf("SER - %s\n\r", bufOut);
			}
		}
		sleep(1);
	}
	perror("SER - COULD NOT OPEN PORT\n\r");
	exit(EXIT_FAILURE);
}