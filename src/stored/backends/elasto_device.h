/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2014 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

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
 * ELASTO API device abstraction.
 *
 * Marco van Wieringen, November 2014
 */

#ifndef ELASTO_DEVICE_H
#define ELASTO_DEVICE_H

#include <elasto/data.h>
#include <elasto/file.h>

class elasto_device: public DEVICE {
private:
   char *m_elasto_configstring;
   char *m_elasto_ps_file;      /* Azure publishsettings file */
   char *m_basedir;
   bool m_insecure_http;
   struct elasto_fh *m_efh;
   POOLMEM *m_virtual_filename;
   boffset_t m_offset;

public:
   elasto_device();
   ~elasto_device();

   /*
    * Interface from DEVICE
    */
   int d_close(int);
   int d_open(const char *pathname, int flags, int mode);
   int d_ioctl(int fd, ioctl_req_t request, char *mt = NULL);
   boffset_t d_lseek(DCR *dcr, boffset_t offset, int whence);
   ssize_t d_read(int fd, void *buffer, size_t count);
   ssize_t d_write(int fd, const void *buffer, size_t count);
   bool d_truncate(DCR *dcr);
};
#endif /* ELASTO_DEVICE_H */
