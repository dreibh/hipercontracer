#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
#  =================================================================
#           #     #                 #     #
#           ##    #   ####   #####  ##    #  ######   #####
#           # #   #  #    #  #    # # #   #  #          #
#           #  #  #  #    #  #    # #  #  #  #####      #
#           #   # #  #    #  #####  #   # #  #          #
#           #    ##  #    #  #   #  #    ##  #          #
#           #     #   ####   #    # #     #  ######     #
#
#        ---   The NorNet Testbed for Multi-Homed Systems  ---
#                        https://www.nntb.no
#  =================================================================
#
#  High-Performance Connectivity Tracer (HiPerConTracer)
#  Copyright (C) 2015-2022 by Thomas Dreibholz
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#  Contact: dreibh@simula.no

import sys
import configparser


class DatabaseConfiguration:

   # ###### Constructor #####################################################
   def __init__(self, configurationFile):
      self.Configuration = {
         'dbBackend':  'PostgreSQL',
         'dbServer':   'localhost',
         'dbPort':     '5432',
         'dbUser':     '!maintainer!',
         'dbPassword': None,
         'database':  'PingTracerouteDB'
      }
      self.readConfiguration(configurationFile)


   # ###### Read database configuration #####################################
   def readConfiguration(self, configurationFile):
      parsedConfigFile = configparser.RawConfigParser()
      parsedConfigFile.optionxform = str   # Make it case-sensitive!
      try:
         parsedConfigFile.read_string('[root]\n' + open(configurationFile, 'r').read())
      except Exception as e:
          sys.stderr.write('ERROR: Unable to read database configuration file' +  sys.argv[1] + ': ' + str(e) + '\n')
          sys.exit(1)

      for parameterName in self.Configuration.keys():
          if parsedConfigFile.has_option('root', parameterName.lower()):
             value = parsedConfigFile.get('root', parameterName.lower())
             if parameterName == 'dbBackend':
                if value not in [ 'MySQL', 'MariaDB', 'MongoDB', 'PostgreSQL' ] :
                   sys.stderr.write('ERROR: Bad backend ' + value + '\n')
                   sys.exit(1)
                self.Configuration[parameterName] = value
             else:
                self.Configuration[parameterName] = value

      # print(self.Configuration)


   # ###### Create new database client instance #############################
   def createClient(self):

      # ====== MySQL/MariaDB ================================================
      if self.Configuration['dbBackend'] in [ 'MySQL', 'MariaDB' ]:
         import mysql.connector
         try:
            self.dbConnection = mysql.connector.connect(host=self.Configuration['dbServer'],
                                                        port=self.Configuration['dbPort'],
                                                        user=self.Configuration['dbUser'],
                                                        password=self.Configuration['dbPassword'],
                                                        database=self.Configuration['database'])
            self.dbCursor = self.dbConnection.cursor()
         except Exception as e:
            sys.stderr.write('ERROR: Unable to connect to the database: ' + str(e) + '\n')
            sys.exit(1)
         return { 'connection': self.dbConnection,
                  'cursor':     self.dbCursor }

      # ====== PostgreSQL ===================================================
      if self.Configuration['dbBackend'] == 'PostgreSQL':
         import psycopg2
         try:
            self.dbConnection = psycopg2.connect(host=self.Configuration['dbServer'],
                                                 port=self.Configuration['dbPort'],
                                                 user=self.Configuration['dbUser'],
                                                 password=self.Configuration['dbPassword'],
                                                 dbname=self.Configuration['database'])
            self.dbConnection.autocommit = False
            self.dbCursor = self.dbConnection.cursor()
         except Exception as e:
            sys.stderr.write('ERROR: Unable to connect to the database: ' + str(e) + '\n')
            sys.exit(1)
         return { 'connection': self.dbConnection,
                  'cursor':     self.dbCursor }

      return None


   # ###### Query database ##################################################
   def query(self, request):

      # ====== MySQL/MariaDB ================================================
      if self.Configuration['dbBackend'] in [ 'MySQL', 'MariaDB' ]:
         try:
            self.dbCursor.execute(request)
         except Exception as e:
            sys.stderr.write('ERROR: Query failed: ' + str(e) + '\n')
            sys.exit(1)
         return self.dbCursor

      # ====== PostgreSQL ===================================================
      if self.Configuration['dbBackend'] == 'PostgreSQL':
         try:
            self.dbCursor.execute(request)
            self.dbConnection.commit()
         except Exception as e:
            sys.stderr.write('ERROR: Query failed: ' + str(e) + '\n')
            sys.exit(1)
         rows = self.dbCursor.fetchall()
         return rows

      return None
