/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2014 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

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

#include "bareos.h"

#ifdef HAVE_ELASTO
#include "stored.h"
#include "elasto_device.h"

 /*
  * Options that can be specified for this device type.
 */
enum device_option_type {
   argument_none = 0,
   argument_insecure_http
};

struct device_option {
   const char *name;
   enum device_option_type type;
   int compare_size;
};

static device_option device_options[] = {
   { "insecure_http", argument_insecure_http, 14 },
   { NULL, argument_none }
};

/*
 * Open a volume using libelasto.
 */
int elasto_device::d_open(const char *pathname, int flags, int mode)
{
   berrno be;
   int status;
   uint32_t elasto_flags = 0;
   struct elasto_fauth auth;
   struct elasto_fstat efstat;

#if 1
   Mmsg1(errmsg, _("Elasto Storage devices are not yet supported, please disable %s\n"), dev_name);
   return -1;
#endif

   if (!m_elasto_configstring) {
      char *bp, *next_option;

      m_elasto_configstring = bstrdup(dev_name);
      bp = strchr(m_elasto_configstring, ':');
      if (!bp) {
         Mmsg1(errmsg, _("Unable to parse device %s.\n"), dev_name);
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      } else {
         *bp++ = '\0';
         m_elasto_ps_file = m_elasto_configstring;
         m_basedir = bp;

         /*
          * See if there are any options.
          */
         bp = strchr(m_basedir, ':');
         if (bp) {
            *bp++ = '\0';

            while (bp) {
               next_option = strchr(bp, ',');
               if (next_option) {
                  *next_option++ = '\0';
               }

               for (int i = 0; device_options[i].name; i++) {
                  /*
                   * Try to find a matching device option.
                   */
                  if (bstrncasecmp(bp, device_options[i].name, device_options[i].compare_size)) {
                     switch (device_options[i].type) {
                     case argument_insecure_http:
                        m_insecure_http = true;
                        break;
                     default:
                        break;
                     }
                  }
               }

               bp = next_option;
            }
         }
      }
   }

   /*
    * See if we don't have a file open already.
    */
   if (m_efh) {
      elasto_fclose(m_efh);
      m_efh = NULL;
   }

   auth.type = ELASTO_FILE_AZURE;
   auth.az.ps_path = m_elasto_ps_file;
   auth.insecure_http = m_insecure_http;

#if 0
   status = elasto_fmkdir(&auth, m_basedir);
   if (status < 0) {
      goto bail_out;
   }
#endif
   Mmsg(m_virtual_filename, "%s/%s", m_basedir, getVolCatName());

   if (flags & O_CREAT) {
      elasto_flags = ELASTO_FOPEN_CREATE;
   }

   status = elasto_fopen(&auth, m_virtual_filename, elasto_flags, &m_efh);
   if (status < 0) {
      goto bail_out;
   }

   status = elasto_fstat(m_efh, &efstat);
   if (status < 0) {
      goto bail_out;
   }

   if (efstat.lease_status == ELASTO_FLEASE_LOCKED) {
      status = elasto_flease_break(m_efh);
      if (status < 0) {
         goto bail_out;
      }
   }

   m_offset = 0;

   return 0;

bail_out:
   if (m_efh) {
      elasto_fclose(m_efh);
      m_efh = NULL;
   }

   return -1;
}

/*
 * Read data from a volume using libelasto.
 */
ssize_t elasto_device::d_read(int fd, void *buffer, size_t count)
{
   if (m_efh) {
      int status;
      int nr_read;
      struct elasto_data *data;

      /*
       * When at start of volume make sure we are able to read the amount
       * of bytes requested as the error handling of elasto_fread on
       * a zero size object (when a volume is labeled its first read to
       * see if its not an existing labeled volume).
       */
      if (m_offset == 0) {
         struct elasto_fstat efstat;

         status = elasto_fstat(m_efh, &efstat);
         if (status < 0) {
            errno = EIO;
            return -1;
         }

         if (count > efstat.size) {
            errno = EIO;
            return -1;
         }
      }

      status = elasto_data_iov_new((uint8_t *)buffer, count, 0, false, &data);
      if (status < 0) {
         Mmsg0(errmsg, _("Failed to setup iovec for reading data.\n"));
         Emsg0(M_FATAL, 0, errmsg);
         return -1;
      }

      nr_read = elasto_fread(m_efh, m_offset, count, data);
      if (nr_read >= 0) {
         m_offset += nr_read;
         data->iov.buf = NULL;
         elasto_data_free(data);

         return nr_read;
      } else {
         errno = -nr_read;
         return -1;
      }
   } else {
      errno = EBADF;
      return -1;
   }
}

/*
 * Write data to a volume using libelasto.
 */
ssize_t elasto_device::d_write(int fd, const void *buffer, size_t count)
{
   if (m_efh) {
      int status;
      int nr_written;
      struct elasto_data *data;

      status = elasto_data_iov_new((uint8_t *)buffer, count, 0, false, &data);
      if (status < 0) {
         Mmsg0(errmsg, _("Failed to setup iovec for writing data.\n"));
         Emsg0(M_FATAL, 0, errmsg);
         return -1;
      }

      nr_written = elasto_fwrite(m_efh, m_offset, count, data);
      if (nr_written >= 0) {
         m_offset += nr_written;
         data->iov.buf = NULL;
         elasto_data_free(data);

         return nr_written;
      } else {
         errno = -nr_written;
         return -1;
      }
   } else {
      errno = EBADF;
      return -1;
   }
}

int elasto_device::d_close(int fd)
{
   if (m_efh) {
      elasto_fclose(m_efh);
      m_efh = NULL;
   } else {
      errno = EBADF;
      return -1;
   }

   return 0;
}

int elasto_device::d_ioctl(int fd, ioctl_req_t request, char *op)
{
   return -1;
}

boffset_t elasto_device::d_lseek(DCR *dcr, boffset_t offset, int whence)
{
   switch (whence) {
   case SEEK_SET:
      m_offset = offset;
      break;
   case SEEK_CUR:
      m_offset += offset;
      break;
   case SEEK_END:
      if (m_efh) {
         int status;
         struct elasto_fstat efstat;

         status = elasto_fstat(m_efh, &efstat);
         if (status < 0) {
            return -1;
         }
         m_offset = efstat.size + offset;
      } else {
         return -1;
      }
      break;
   default:
      return -1;
   }

   return m_offset;
}

bool elasto_device::d_truncate(DCR *dcr)
{
   if (m_efh) {
      int status;
      struct elasto_fstat efstat;

      status = elasto_ftruncate(m_efh, 0);
      if (status < 0) {
         return false;
      }

      status = elasto_fstat(m_efh, &efstat);
      if (status < 0) {
         return false;
      }

      if (efstat.size == 0) {
         m_offset = 0;
      } else {
         return false;
      }
   }

   return true;
}

elasto_device::~elasto_device()
{
   if (m_efh) {
      elasto_fclose(m_efh);
      m_efh = NULL;
   }

   if (m_elasto_configstring) {
      free(m_elasto_configstring);
   }

   free_pool_memory(m_virtual_filename);
}

elasto_device::elasto_device()
{
   m_elasto_configstring = NULL;
   m_elasto_ps_file = NULL;
   m_basedir = NULL;
   m_insecure_http = false;
   m_efh = NULL;
   m_virtual_filename = get_pool_memory(PM_FNAME);
}

#ifdef HAVE_DYNAMIC_SD_BACKENDS
extern "C" DEVICE SD_IMP_EXP *backend_instantiate(JCR *jcr, int device_type)
{
   DEVICE *dev = NULL;

   switch (device_type) {
   case B_ELASTO_DEV:
      dev = New(elasto_device);
      break;
   default:
      Jmsg(jcr, M_FATAL, 0, _("Request for unknown devicetype: %d\n"), device_type);
      break;
   }

   return dev;
}

extern "C" void SD_IMP_EXP flush_backend(void)
{
}
#endif
#endif /* HAVE_ELASTO */
