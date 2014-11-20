/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2014-2014 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * This file handles external connections made to the File daemon.
 *
 * Kern Sibbald, October MM
 *
 * Extracted from other source files by Marco van Wieringen, October 2014
 */

#include "bareos.h"
#include "filed.h"

/* Global variables */
static workq_t socket_workq;
static alist *sock_fds;
static pthread_t tcp_server_tid;

/*
 * Connection request. We accept connections either from the Director or the Storage Daemon
 *
 * NOTE! We are running as a separate thread
 *
 * Send output one line at a time followed by a zero length transmission.
 * Return when the connection is terminated or there is an error.
 *
 * Basic tasks done here:
 *  - If it was a connection from an SD, call handle_stored_connection()
 *  - Otherwise it was a connection from the DIR, call handle_director_connection()
 */
static void *handle_connection_request(void *arg)
{
   BSOCK *bs = (BSOCK *)arg;
   char tbuf[100];

   if (bs->recv() <= 0) {
      Emsg1(M_ERROR, 0, _("Connection request from %s failed.\n"), bs->who());
      bmicrosleep(5, 0);   /* make user wait 5 seconds */
      bs->close();
      delete bs;
      return NULL;
   }

   Dmsg1(110, "Conn: %s", bs->msg);

   /*
    * See if its a director making a connection.
    */
   if (bstrncmp(bs->msg, "Hello Director", 14)) {
      Dmsg1(110, "Got a DIR connection at %s\n", bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)));
      return handle_director_connection(bs);
   }

   /*
    * See if its a storage daemon making a connection.
    */
   if (bstrncmp(bs->msg, "Hello Storage", 13)) {
      Dmsg1(110, "Got a SD connection at %s\n", bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)));
      return handle_stored_connection(bs);
   }

   Emsg2(M_ERROR, 0, _("Invalid connection from %s. Len=%d\n"), bs->who(), bs->msglen);

   return NULL;
}

void start_socket_server(dlist *addrs)
{
   IPADDR *p;

   tcp_server_tid = pthread_self();

   /*
    * Become server, and handle requests
    */
   foreach_dlist(p, addrs) {
      Dmsg1(10, "filed: listening on port %d\n", p->get_port_host_order());
   }

   sock_fds = New(alist(10, not_owned_by_alist));
   bnet_thread_server_tcp(addrs,
                          me->MaxConcurrentJobs,
                          sock_fds,
                          &socket_workq,
                          me->nokeepalive,
                          handle_connection_request);
}

void stop_socket_server()
{
   bnet_stop_thread_server_tcp(tcp_server_tid);
   cleanup_bnet_thread_server_tcp(sock_fds, &socket_workq);
   delete sock_fds;
   sock_fds = NULL;

}
