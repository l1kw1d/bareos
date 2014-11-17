/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * Bareos File Daemon specific configuration and defines
 *
 * Kern Sibbald, Jan MMI
 */

#define FILE_DAEMON 1
#include "filed_conf.h"
#include "lib/breg.h"
#include "lib/htable.h"
#include "lib/runscript.h"
#include "findlib/find.h"
#include "fd_plugins.h"
#include "ch.h"
#include "backup.h"
#include "restore.h"
#include "protos.h"                   /* file daemon prototypes */

extern CLIENTRES *me;                 /* "Global" Client resource */

void terminate_filed(int sig);
