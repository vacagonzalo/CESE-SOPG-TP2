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
/**
 * @brief Selector del puerto serie
 *
 */
int pnSerial;

/**
 * @brief Velocidad de transmisión del puerto serie
 *
 */
int baudrate;

/**
 * @brief Número de puerto TCP
 *
 */
int pnTCP;

/**
 * @brief Dirección IP del servidor
 *
 */
char *ipADDR;
///////////////////////////////////////////////////////////////////////////////

// Recursos globales //////////////////////////////////////////////////////////
/**
 * @brief Control de cierre de la aplicación
 *
 */
int killme = 0;

/**
 * @brief File descriptor de la conexión TCP
 *
 */
int newfd = -1;

/**
 * @brief Estado de los indicadores del subte
 *
 */
char lines[] = {OUT_OFF, OUT_OFF, OUT_OFF, OUT_OFF};

/**
 * @brief Protección de escritura para los leds de la EDU-CIAA
 *
 */
pthread_mutex_t mutexOuts = PTHREAD_MUTEX_INITIALIZER;
///////////////////////////////////////////////////////////////////////////////

// FUNCIONES //////////////////////////////////////////////////////////////////
/**
 * @brief Manejador de las señales del sistema operativo
 *
 * Puede manejar las señales SIGINT y SIGTERM. Ambas señales pondrán a 'killme'
 * en '1', esto finaliza el proceso.
 *
 * @param signal Identificador de la señal
 */
void signal_handler(int signal);

/**
 * @brief Función para el thread correspondiente a la comunicación TCP
 *
 * La función se identificará en los mensajes con el prefijo 'TPC - '
 *
 * @param param No se ingresan parámetros
 * @return void* No se regresan valores
 */
void *taskTCP(void *param);

/**
 * @brief Función para el thread correspondiente a la comunicación SERIE
 *
 * @param param No se ingresan parámetros
 * @return void* No se regresan valores
 */
void *taskSERIAL(void *param);
///////////////////////////////////////////////////////////////////////////////

// Constantes /////////////////////////////////////////////////////////////////
const out_t X = {.inBoard = 0, .inBuffer = 6};
const out_t Y = {.inBoard = 1, .inBuffer = 8};
const out_t W = {.inBoard = 2, .inBuffer = 10};
const out_t Z = {.inBoard = 3, .inBuffer = 12};

const char NUM_ARGUMENTS = 5;

const char POS_SERIAL_INDX = 1;
const char MIN_SERIAL_INDX = 1;
const char MAX_SERIAL_INDX = 21;

const char POS_BAUDRATE = 2;

const char POS_TCPPOR = 3;
const char POS_IPADDR = 4;

const char FPO = 1;
const char ACK = 'O';
const char TGL = 'T';
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief Punto de ingreso del proceso
 *
 * @param argc Se espera recibir el número 5
 * @param argv Se espera recibir el nombre del programa y 4 palabras
 * (variables de entorno)
 * @return int Si finalizó con normalidad o con errores
 */
int main(int argc, char *argv[])
{
	printf("MAI - START\n\r");

	// VERIFICACIÓN DE ARGUMENTOS /////////////////////////////////////////////
	if (NUM_ARGUMENTS != argc)
	{
		perror("MAI - INVALID NUMBER OF ARGUMENTS\n\r");
		exit(EXIT_FAILURE);
	}

	pnSerial = atoi(argv[POS_SERIAL_INDX]);
	if ((pnSerial > MAX_SERIAL_INDX) || (pnSerial < MIN_SERIAL_INDX))
	{
		perror("MAI - INVALID SERIAL PORT\n\r");
		exit(EXIT_FAILURE);
	}

	baudrate = atoi(argv[POS_BAUDRATE]);
	if (INVALID_BAUD(baudrate))
	{
		perror("MAI - INVALID BAUDRATE\n\r");
		exit(EXIT_FAILURE);
	}

	pnTCP = atoi(argv[POS_TCPPOR]);
	if (pnTCP < 0)
	{
		perror("MAI - INVALID TCP PORT\n\r");
	}

	ipADDR = argv[POS_IPADDR];
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
	addrSer.sin_addr.s_addr = inet_addr(ipADDR);
	if (addrSer.sin_addr.s_addr == INADDR_NONE)
	{
		perror("TCP - INVALID IP");
		exit(EXIT_FAILURE);
	}
	else
	{
		printf("TCP - SOCKET created\n\r");
	}

	// Apertura de puerto
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
			for (char i = 0; i < 4; ++i)
			{
				lines[i] = buf[7 + i] - '0';
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
			if (TGL == bufInp[FPO])
			{
				printf("SER - %s", bufInp);

				pthread_mutex_lock(&mutexOuts);
				// Suma módulo 2
				char pos = bufInp[14] - '0';
				outs[pos]++;
				if (outs[pos] > OUT_BLINK)
				{
					outs[pos] = OUT_OFF;
				}
				lines[pos] = outs[pos];
				pthread_mutex_unlock(&mutexOuts);

				bufOut[X.inBuffer] = outs[X.inBoard] + '0';
				bufOut[Y.inBuffer] = outs[Y.inBoard] + '0';
				bufOut[W.inBuffer] = outs[W.inBoard] + '0';
				bufOut[Z.inBuffer] = outs[Z.inBoard] + '0';
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
			else if (ACK == bufInp[FPO])
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
					bufOut[X.inBuffer + (i * 2)] = outs[i] + '0'; // Salto las ','
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