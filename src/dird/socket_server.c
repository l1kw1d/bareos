/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.
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
 * This file handles external connections made to the director.
 *
 * Kern Sibbald, August MM
 *
 * Extracted from other source files by Marco van Wieringen, October 2014
 */

#include "bareos.h"
#include "dird.h"

/* Global variables */
static int started = FALSE;
static workq_t socket_workq;
static alist *sock_fds;
static pthread_t tcp_server_tid;

struct s_addr_port {
   char *addr;
   char *port;
};

static void *handle_connection_request(void *arg)
{
   BSOCK *bs = (BSOCK *)arg;
   char name[MAX_NAME_LENGTH];
   char tbuf[MAX_TIME_LENGTH];

   if (bs->recv() <= 0) {
      Emsg1(M_ERROR, 0, _("Connection request from %s failed.\n"), bs->who());
      bmicrosleep(5, 0);   /* make user wait 5 seconds */
      bs->close();
      return NULL;
   }

   /*
    * Do a sanity check on the message received
    */
   if (bs->msglen < 25 || bs->msglen > (int)sizeof(name)) {
      Dmsg1(000, "<filed: %s", bs->msg);
      Emsg2(M_ERROR, 0, _("Invalid connection from %s. Len=%d\n"), bs->who(), bs->msglen);
      bmicrosleep(5, 0);   /* make user wait 5 seconds */
      bs->close();
      return NULL;
   }

   Dmsg1(110, "Conn: %s", bs->msg);

   /*
    * See if this is a File daemon connection. If so call FD handler.
    */
   if (sscanf(bs->msg, "Hello Client %127s calling", name) == 1) {
      Dmsg1(110, "Got a FD connection at %s\n", bstrftimes(tbuf, sizeof(tbuf), (utime_t)time(NULL)));
      return handle_filed_connection(bs, name);
   }

   return handle_UA_client_request(bs);
}

extern "C" void *connect_thread(void *arg)
{
   pthread_detach(pthread_self());
   set_jcr_in_tsd(INVALID_JCR);

   /*
    * Permit MaxConnections connections.
    */
   sock_fds = New(alist(10, not_owned_by_alist));
   bnet_thread_server_tcp((dlist*)arg,
                          me->MaxConnections,
                          sock_fds,
                          &socket_workq,
                          me->nokeepalive,
                          handle_connection_request);

   return NULL;
}

/*
 * Called here by Director daemon to start UA (user agent)
 * command thread. This routine creates the thread and then
 * returns.
 */
void start_socket_server(dlist *addrs)
{
   int status;
   static dlist *myaddrs = addrs;

   if ((status = pthread_create(&tcp_server_tid, NULL, connect_thread, (void *)myaddrs)) != 0) {
      berrno be;
      Emsg1(M_ABORT, 0, _("Cannot create UA thread: %s\n"), be.bstrerror(status));
   }
   started = TRUE;

   return;
}

void stop_socket_server()
{
   if (!started) {
      return;
   }

   bnet_stop_thread_server_tcp(tcp_server_tid);
   cleanup_bnet_thread_server_tcp(sock_fds, &socket_workq);
   delete sock_fds;
   sock_fds = NULL;
}
