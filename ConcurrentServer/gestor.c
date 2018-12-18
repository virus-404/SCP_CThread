/* ---------------------------------------------------------------
Práctica 1.
Código fuente : gestor.c
Grau Informàtica
53399223P Agapito Gallart Bernat
--------------------------------------------------------------- */

#include "service.h"
#include "gestor.h"

#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>

// globals
	char g_pwd[PATH_MAX+1];
  int sesion = 0;

	int signal_kill = 0;
	int max_threads;
	int* external_list_thread;
	pthread_t master_thread;
	pthread_t *threads_terminate = NULL;
	struct data * server_stats;
// funcs

void sigchld_handler(int s)
{
	while(wait(NULL) > 0);
}

void sig_kill(int threads)
{
		fflush(stdout);
		printf("\n(!) WARNING SHUTDOWN (!)\n");
		signal_kill = 1;

}



int main(int a_argc, char **ap_argv)
{
	// check args
		if(a_argc < 4)
		{
			printf("Usage: %s dir_name port_number\n", ap_argv[0]);
			return 1;
		}

	// variables
		max_threads = atoi(ap_argv[3]);

  	int serverSocket, clientSocket [max_threads];
  	socklen_t clientAddrSize;
		struct sockaddr_in clientAddr;
		struct sigaction signalAction;


		int active_threads, free_thread;
		int list_threads[max_threads];
		pthread_t threads[max_threads];
		struct data *finished_client;
		struct data server;
		clock_t start, end;

	// init vars
		realpath(ap_argv[1], g_pwd);

  // globals vars
		master_thread = pthread_self();
		external_list_thread = list_threads;
		threads_terminate = threads;
		server_stats = &server;

	// local vars
		active_threads = 0;
		for (size_t i = 0; i < max_threads; i++) {
			list_threads[i] = -1;
		}

		start  = clock();
		server_stats->id = -1;
		server_stats->n_comands = 0;
		server_stats->n_gets = 0;
		server_stats->n_puts = 0;
		server_stats->b_get = 0;
		server_stats->b_put = 0;
		server_stats->t_session = 0;
		server_stats->t_get = 0;
		server_stats->t_put = 0;

	// create service
		if(!service_create(&serverSocket, strtol(ap_argv[2], (char**)NULL, 10)))
		{
			fprintf(stderr, "%s: unable to create service on port: %s\n", ap_argv[0], ap_argv[2]);
			return 2;
		}

	// setup termination handler
		signalAction.sa_handler = sigchld_handler; // reap all dead processes
		sigemptyset(&signalAction.sa_mask);
		signalAction.sa_flags = SA_RESTART;

		if (sigaction(SIGCHLD, &signalAction, NULL) == -1)
		{
			perror("main(): sigaction");
			return 3;
		}

		signal(SIGINT,sig_kill);

		#ifndef NOSERVERDEBUG
			printf("\nmain(): waiting for clients...\n");
		#endif
	// dispatcher loop
		while(signal_kill == 0)
		{
			clientAddrSize = sizeof(clientAddr);
			if (max_threads > active_threads){

				free_thread = get_lazy_thread(list_threads);
				clientSocket[free_thread] = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrSize);

				if(clientSocket[free_thread] != -1){
					#ifndef NOSERVERDEBUG
						printf("\nmain(): got client connection [addr=%s,port=%d] --> %d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), sesion);
					#endif
					active_threads ++;
				  if(session_create(clientSocket[free_thread])){
							void * tmp = &clientSocket[free_thread];
							pthread_create (&threads[free_thread], NULL, server_service_loop, tmp);
							list_threads [free_thread] = 1;
							#ifndef NOSERVERDEBUG
								printf("\nmain(): waiting for clients...\n");
							#endif
						}
				}
			}

			free_thread = finished_client_getter();

			if(free_thread != -1){
					session_destroy(clientSocket[free_thread]);
					pthread_join(threads[free_thread], (void*) &finished_client);
					list_threads[free_thread] = -1;
					active_threads--;
					show_statistics(*(finished_client));
					free(finished_client);
					close(clientSocket[free_thread]); // parent doesn't need this socket}
				}
		}

		free_thread = finished_client_getter();

		while (free_thread != -1) {
			session_destroy(clientSocket[free_thread]);
			pthread_join(threads[free_thread], (void*) &finished_client);
			list_threads[free_thread] = -1;
			active_threads--;
			show_statistics(*(finished_client));
			free(finished_client);
			close(clientSocket[free_thread]); // parent doesn't need this socket}

			free_thread = finished_client_getter();
		}

	// destroy service
		fflush(stdout);
		close(serverSocket);
		end = clock();
		server_stats->t_session= ((double)(end - start) / CLOCKS_PER_SEC ) * 1000;
		show_statistics(*(server_stats));
		return 0;
}

Boolean service_create(int *ap_socket, const int a_port)
{
	// variables
		struct sockaddr_in serverAddr;

		#ifdef _PLATFORM_SOLARIS
			char yes='1';
		#else
			int yes=1;
		#endif

	// create address
		memset(&serverAddr, 0, sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		serverAddr.sin_port = htons(a_port);

	// create socket
		if((*ap_socket = socket(serverAddr.sin_family, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
		{
			perror("service_create(): create socket");
			return false;
		}

	// set options
		if(setsockopt(*ap_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0)
		{
			perror("service_create(): socket opts");
			return false;
		}

	// bind socket
		if(bind(*ap_socket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
		{
			perror("service_create(): bind socket");
			close(*ap_socket);
			return false;
		}

	// listen to socket
		if(listen(*ap_socket, SERVER_SOCKET_BACKLOG) < 0)
		{
			perror("service_create(): listen socket");
			close(*ap_socket);
			return false;
		}
	return true;
}

Boolean session_create(const int a_socket)
{
	// variables
		Message msgOut, msgIn;

	// init vars
		Message_clear(&msgOut);
		Message_clear(&msgIn);

	// session challenge dialogue

		// client: greeting
			if(!siftp_recv(a_socket, &msgIn) || !Message_hasType(&msgIn, SIFTP_VERBS_SESSION_BEGIN))
			{
				fprintf(stderr, "[%d] service_create(): session not requested by client.\n",sesion);
				return false;
			}

		// server: identify
		// client: username
			Message_setType(&msgOut, SIFTP_VERBS_IDENTIFY);

			if(!service_query(a_socket, &msgOut, &msgIn) || !Message_hasType(&msgIn, SIFTP_VERBS_USERNAME))
			{
				fprintf(stderr, "[%d] service_create(): username not specified by client.\n",sesion);
				return false;
			}

		// server: accept|deny
		// client: password
			Message_setType(&msgOut, SIFTP_VERBS_ACCEPTED); //XXX check username... not required for this project

			if(!service_query(a_socket, &msgOut, &msgIn) || !Message_hasType(&msgIn, SIFTP_VERBS_PASSWORD))
			{
				fprintf(stderr, "[%d] service_create(): password not specified by client.\n",sesion);
				return false;
			}

		// server: accept|deny
			if(Message_hasValue(&msgIn, SERVER_PASSWORD))
			{
				Message_setType(&msgOut, SIFTP_VERBS_ACCEPTED);
				siftp_send(a_socket, &msgOut);
			}
			else
			{
				Message_setType(&msgOut, SIFTP_VERBS_DENIED);
				siftp_send(a_socket, &msgOut);

				fprintf(stderr, "[%d] service_create(): client password rejected.\n",sesion);
				return false;
			}

		// session now established
			#ifndef NOSERVERDEBUG
				printf("[%d] session_create(): success\n",sesion);
			#endif

	return true;
}

void* server_service_loop(void * socket)
{
	// variables
		Message msg;
		String *p_argv;
		int argc;

		const int a_socket = *((int *) socket);
		struct data *statistics =  malloc (sizeof(struct data));
		clock_t start, end;

	//fcntl(a_socket, F_SETFL, fcntl(a_socket, F_GETFL) | O_NONBLOCK);
	// init vars
		Message_clear(&msg);
		statistics->id = sesion;
		sesion++;
		statistics->n_comands = 0;
		statistics->n_gets = 0;
		statistics->n_puts = 0;
		statistics->b_get = 0;
		statistics->b_put = 0;
		statistics->t_session = 0;
		statistics->t_get = 0;
		statistics->t_put = 0;

		start  = clock();

	while(signal_kill == 0) // await request
	{
		siftp_recv(a_socket, &msg);
		if(Message_hasType(&msg, SIFTP_VERBS_SESSION_END) || Message_hasType(&msg, "")){
			break;
		}else {
			#ifndef NOSERVERDEBUG
				printf("[%d] service_loop(): got command '%s'\n",sesion, Message_getValue(&msg));
			#endif

			// parse request
				if((p_argv = service_parseArgs(Message_getValue(&msg), &argc)) == NULL || argc <= 0)
				{
					service_freeArgs(p_argv, argc);

					if(!service_sendStatus(a_socket, false)) // send negative ack
						break;
					continue;
				}

			// handle request
				if(!server_service_handleCmd(a_socket, p_argv, argc, statistics))
					service_sendStatus(a_socket, false); // send negative ack upon fail

			// clean up
				service_freeArgs(p_argv, argc);
				p_argv = NULL;
		}
	}

	for (size_t i = 0; i < max_threads; i++) {
		if (pthread_equal(pthread_self(), *(threads_terminate + i))) {
				*(external_list_thread + i) = 0;

		}
	}
	end = clock();
	statistics->t_session = ((double)(end - start) / CLOCKS_PER_SEC ) * 1000;
	pthread_exit(statistics);
}

Boolean server_service_handleCmd(const int a_socket, const String *ap_argv, const int a_argc, struct data *statistics)
{
	// variables
		Message msg;

		String dataBuf;
		int dataBufLen;

		Boolean tempStatus = false;

    clock_t start, end;

	// init variables
		Message_clear(&msg);

	if(strcmp(ap_argv[0], "ls") == 0)
	{
    statistics->n_comands++;
		if((dataBuf = service_readDir(g_pwd, &dataBufLen)) != NULL)
		{
			// transmit data
				if(service_sendStatus(a_socket, true))
					tempStatus = siftp_sendData(a_socket, dataBuf, dataBufLen);

				#ifndef NOSERVERDEBUG
					printf("[%d] ls(): status=%d\n",sesion, tempStatus);
				#endif

			// clean up
				free(dataBuf);

			return tempStatus;
		}
	}

	else if(strcmp(ap_argv[0], "pwd") == 0)
	{
    statistics->n_comands++;
		if(service_sendStatus(a_socket, true))
			return siftp_sendData(a_socket, g_pwd, strlen(g_pwd));
	}

	else if(strcmp(ap_argv[0], "cd") == 0 && a_argc > 1)
	{
    statistics->n_comands++;
		return service_sendStatus(a_socket, service_handleCmd_chdir(g_pwd, ap_argv[1]));
	}

	else if(strcmp(ap_argv[0], "get") == 0 && a_argc > 1)
	{
    statistics->n_comands++;
		statistics->n_gets++;
		char srcPath[PATH_MAX+1];

		// determine absolute path
		if(service_getAbsolutePath(g_pwd, ap_argv[1], srcPath))
		{
			// check read perms & file type
			if(service_permTest(srcPath, SERVICE_PERMS_READ_TEST) && service_statTest(srcPath, S_IFMT, S_IFREG))
			{
				// read file

				if((dataBuf = service_readFile(srcPath, &dataBufLen)) != NULL)
				{
					if(service_sendStatus(a_socket, true))
					{
						// send file
            start  = clock();
						tempStatus = siftp_sendData(a_socket, dataBuf, dataBufLen);
            end = clock();
            statistics->t_get = ((double)(end - start) / CLOCKS_PER_SEC ) * 1000 + statistics->t_get;
            statistics->b_get = dataBufLen + statistics->b_get;
						#ifndef NOSERVERDEBUG
							printf("[%d] get(): file sent %s.\n",sesion, tempStatus ? "OK" : "FAILED");
						#endif
					}
					#ifndef NOSERVERDEBUG
					else
						printf("[%d] get(): remote host didn't get status ACK.\n",sesion);
					#endif

					free(dataBuf);
				}
				#ifndef NOSERVERDEBUG
				else
					printf("[%d]get(): file reading failed.\n",sesion);
				#endif
			}
			#ifndef NOSERVERDEBUG
			else
				printf("[%d]get(): don't have read permissions.\n",sesion);
			#endif
		}
		#ifndef NOSERVERDEBUG
		else
			printf("[%d] get(): absolute path determining failed.\n",sesion);
		#endif

		return tempStatus;
	}

	else if(strcmp(ap_argv[0], "put") == 0 && a_argc > 1)
	{
    statistics->n_comands++;
    statistics->n_puts++;
		char dstPath[PATH_MAX+1];

		// determine destination file path
		if(service_getAbsolutePath(g_pwd, ap_argv[1], dstPath))
		{
			// check write perms & file type
			if(service_permTest(dstPath, SERVICE_PERMS_WRITE_TEST) && service_statTest(dstPath, S_IFMT, S_IFREG))
			{
				// send primary ack: file perms OK
				if(service_sendStatus(a_socket, true))
				{
					// receive file
					if((dataBuf = siftp_recvData(a_socket, &dataBufLen)) != NULL)
					{
						#ifndef NOSERVERDEBUG
							printf("[%d] put(): about to write to file '%s'\n",sesion, dstPath);
						#endif

            start  = clock();

						tempStatus = service_writeFile(dstPath, dataBuf, dataBufLen);

            end = clock();
            statistics->t_put = ((double)(end - start) / CLOCKS_PER_SEC ) * 1000 + statistics->t_put;
            statistics->b_put = dataBufLen + statistics->b_put;
						free(dataBuf);

						#ifndef NOSERVERDEBUG
							printf("[%d] put(): file writing %s.\n", sesion, tempStatus ? "OK" : "FAILED");
						#endif

						// send secondary ack: file was written OK
						if(tempStatus)
						{
							return service_sendStatus(a_socket, true);
						}
					}
					#ifndef NOSERVERDEBUG
					else
						printf("[%d] put(): getting of remote file failed.\n",sesion);
					#endif
				}
				#ifndef NOSERVERDEBUG
				else
					printf("[%d] put(): remote host didn't get status ACK.\n",sesion);
				#endif
			}
			#ifndef NOSERVERDEBUG
			else
				printf("[%d] put(): don't have write permissions.\n",sesion);
			#endif
		}
		#ifndef NOSERVERDEBUG
		else
			printf("[%d] put(): absolute path determining failed.\n",sesion);
		#endif
	}

	else if (strcmp(ap_argv[0], "exit") == 0){
		statistics->n_comands++;
		return true;
	}
	return false;
}

int get_lazy_thread(int list_threads []) {
	for (size_t i = 0; i < max_threads ; i++) {
		if (list_threads[i] == -1) {
			return (int)  i;
		}
	}
	return -1;
}

int finished_client_getter(){
	for (size_t i = 0; i < max_threads; i++) {
		if (*(external_list_thread + i) == 0){
			return (int) i;
		}
	}
	return -1;
}


void show_statistics(struct data statistics){
	printf("--------------------------\n");
	printf("[ID]Statistic: value\n");
	printf("--------------------------\n");
	printf("[%d]Session time: %f\n", statistics.id, statistics.t_session / (1000));
	printf("[%d]Number commands: %i\n", statistics.id, statistics.n_comands);
	printf("[%d]Commmand per minute: %f\n", statistics.id,  ( 60 * 1000 * (double) statistics.n_comands )/ (statistics.t_session));
	printf("[%d]Number of uploads: %i\n", statistics.id, statistics.n_puts);
	printf("[%d]Upload velocity: %f\n",     statistics.id, ((double) statistics.b_put )/ (statistics.t_session / 1000));
	printf("[%d]Number of downloads: %i\n", statistics.id, statistics.n_gets);
	printf("[%d]Downloads velocity: %f\n",  statistics.id, ((double) statistics.b_get) / (statistics.t_session / 1000));
	printf("--------------------------\n");

	
	server_stats->n_comands = server_stats->n_comands + statistics.n_comands;
	server_stats->n_gets = server_stats->n_gets + statistics.n_gets;
	server_stats->n_puts = server_stats->n_puts + statistics.n_puts;
	server_stats->b_put = server_stats->b_put + statistics.b_put;
	server_stats->b_get = server_stats->b_get + statistics.b_get;
	server_stats->t_put = server_stats->t_put + statistics.t_put;
	server_stats->t_get = server_stats->t_get + statistics.t_get;

}
