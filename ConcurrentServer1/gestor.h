/**
 * Suraj Kurapati <skurapat@ucsc.edu>
 * CMPS-150, Spring04, final project
 *
 * SimpleFTP server interface.
**/

#ifndef SERVER_H
#define SERVER_H

#include "siftp.h"

	/* constants */

		#define SERVER_SOCKET_BACKLOG	5
		#define SERVER_PASSWORD	"scp18"

//		#define CONCURRENT 1

	/* services */

		/**
		 * Establishes a network service on the specified port.
		 * @param	ap_socket	Storage for socket descriptor.
		 * @param	a_port	Port number in decimal.
		 */
		Boolean service_create(int *ap_socket, const int a_port);

    int get_lazy_thread(int list_threads []);
		int finished_client_getter();
		void show_statistics(struct data statistics);
#endif
