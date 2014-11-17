#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2014 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation, which is
# listed in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#
# Author: Maik Aussendorf
#
# Bareos python plugins class that adds files from a local list to
# the backup fileset

import bareosfd
from bareos_fd_consts import bJobMessageType, bFileType, bRCs
import os
import BareosFdPluginBaseclass


class BareosFdPluginLocalFileset(BareosFdPluginBaseclass.BareosFdPluginBaseclass):  # noqa
    '''
    Simple Bareos-FD-Plugin-Class that parses a file and backups all files
    listed there Filename is taken from plugin argument 'filename'
    '''

    def parse_plugin_definition(self, context, plugindef):
        '''
        Parses the plugin argmuents and reads files from file given by
        argument 'filename'
        '''
        # BareosFdPluginBaseclass.parse_plugin_definition(self, context, plugindef); # noqa
        super(BareosFdPluginLocalFileset, self).parse_plugin_definition(
            context, plugindef)
        if ('filename' not in self.options):
            bareosfd.DebugMessage(context, 100,
                                  "Option \'filename\' not defined.\n")
            return bRCs['bRC_Error']
        bareosfd.DebugMessage(context, 100,
                              "Using %s to search for local files\n"
                              % (self.options['filename']))
        if os.path.exists(self.options['filename']):
            try:
                config_file = open(self.options['filename'], 'rb')
            except:
                bareosfd.DebugMessage(context, 100,
                                      "Could not open file %s\n"
                                      % (self.options['filename']))
                return bRCs['bRC_Error']
        else:
            bareosfd.DebugMessage(context, 100,
                                  "File %s does not exist\n"
                                  % (self.options['filename']))
            return bRCs['bRC_Error']
        self.files_to_backup = config_file.read().splitlines()
        return bRCs['bRC_OK']

    def start_backup_file(self, context, savepkt):
        '''
        Defines the file to backup and creates the savepkt. In this example
        only files (no directories) are allowed
        '''
        bareosfd.DebugMessage(context, 100, "start_backup called\n")
        if not self.files_to_backup:
            bareosfd.DebugMessage(context, 100, "No files to backup\n")
            return bRCs['bRC_Skip']

        file_to_backup = self.files_to_backup.pop()
        bareosfd.DebugMessage(context, 100, "file: " + file_to_backup + "\n")

        statp = bareosfd.StatPacket()
        savepkt.statp = statp
        savepkt.fname = file_to_backup
        savepkt.type = bFileType['FT_REG']

        bareosfd.JobMessage(context, bJobMessageType['M_INFO'],
                            "Starting backup of %s\n"
                            % (file_to_backup))
        return bRCs['bRC_OK']

    def end_backup_file(self, context):
        '''
        Here we return 'bRC_More' as long as our list files_to_backup is not
        empty and bRC_OK when we are done
        '''
        bareosfd.DebugMessage(
            context, 100,
            "end_backup_file() entry point in Python called\n")
        if self.files_to_backup:
            return bRCs['bRC_More']
        else:
            return bRCs['bRC_OK']


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
