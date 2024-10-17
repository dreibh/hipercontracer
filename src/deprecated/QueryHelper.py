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
#  Copyright (C) 2015-2024 by Thomas Dreibholz
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

import configparser
import ipaddress
import sys


class DatabaseConfiguration:

   # ###### Constructor #####################################################
   def __init__(self, configurationFile):
      self.Configuration = {
         'dbBackend':         'PostgreSQL',
         'dbServer':          'localhost',
         'dbPort':            0,
         'dbCAFile':          None,
         'dbCRLFile':         None,
         'dbCertKeyFile':     None,
         'dbCertFile':        None,
         'dbKeyFile':         None,
         'dbConnectionFlags': None,
         'dbUser':            '!maintainer!',
         'dbPassword':        None,
         'database':         'PingTracerouteDB'
      }
      self.AvailableConnectionFlags = [
         'DisableTLS',
         'AllowInvalidHostname',
         'AllowInvalidCertificate'
      ]
      self.readConfiguration(configurationFile)


   # ###### Destructor ######################################################
   def __del__(self):
      self.destroyClient()


   # ###### Read database configuration #####################################
   def readConfiguration(self, configurationFile):
      parsedConfigFile = configparser.RawConfigParser()
      parsedConfigFile.optionxform = str   # Make it case-sensitive!
      try:
         parsedConfigFile.read_string('[root]\n' + open(configurationFile, 'r').read())
      except Exception as e:
          sys.stderr.write('ERROR: Unable to read database configuration file ' +  sys.argv[1] + ': ' + str(e) + '\n')
          sys.exit(1)

      # ====== Read parameters ==============================================
      for parameterName in self.Configuration.keys():
          if parsedConfigFile.has_option('root', parameterName.lower()):
             value = parsedConfigFile.get('root', parameterName.lower())
             if parameterName == 'dbBackend':
                if value not in [ 'MySQL', 'MariaDB', 'MongoDB', 'PostgreSQL' ] :
                   sys.stderr.write('ERROR: Bad backend ' + value + '!\n')
                   sys.exit(1)
                self.Configuration[parameterName] = value
             elif parameterName == 'dbPort':
                try:
                   self.Configuration['dbPort'] = int(self.Configuration['dbPort'])
                except Exception as e:
                   sys.stderr.write('ERROR: Bad dbPort value ' + value + '!\n')
                   sys.exit(1)
             elif parameterName == 'dbConnectionFlags':
                if value == 'NONE':
                   self.Configuration['dbConnectionFlags'] = None
                else:
                   self.Configuration['dbConnectionFlags'] = value.split(' ')
                   for flag in self.Configuration['dbConnectionFlags']:
                      if not flag in self.AvailableConnectionFlags:
                         sys.stderr.write('ERROR: Invalid dbConnectionFlags flag ' + flag + '!\n')
                         sys.exit(1)
             else:
                self.Configuration[parameterName] = value

      # ====== Check parameters =============================================
      if self.Configuration['dbPort'] == 0:
         if self.Configuration['dbBackend'] in [ 'MySQL', 'MariaDB' ]:
            self.Configuration['dbPort'] = 3306
         elif self.Configuration['dbBackend'] == 'PostgreSQL':
            self.Configuration['dbPort'] = 5432
         elif self.Configuration['dbBackend'] == 'MongoDB':
            self.Configuration['dbPort'] = 27017

      if self.Configuration['dbBackend'] in [ 'MySQL', 'MariaDB' ]:
         if self.Configuration['dbCertKeyFile'] != None:
            sys.stderr.write('ERROR: MySQL/MariaDB backend expects dbCertFile+dbKeyFile, not dbCertKeyFile!\n')
            sys.exit(1)
         if self.Configuration['dbCRLFile'] != None:
            sys.stderr.write('WARNING: MySQL/MariaDB backend (based on mysql-connector-python) does not support dbCRLFile!\n')
      elif self.Configuration['dbBackend'] == 'PostgreSQL':
         if self.Configuration['dbCertKeyFile'] != None:
            sys.stderr.write('ERROR: PostgreSQL backend expects dbCertFile+dbKeyFile, not dbCertKeyFile!\n')
            sys.exit(1)
      elif self.Configuration['dbBackend'] == 'MongoDB':
         if self.Configuration['dbCertFile'] != None or \
            self.Configuration['dbKeyFile']  != None :
            sys.stderr.write('ERROR: MongoDB backend expects dbCertKeyFile, not dbCertFile+dbKeyFile!\n')
            sys.exit(1)
         if self.Configuration['dbCRLFile'] != None:
            # NOTE: Using a CRL with PyMongo fails with "ssl_crlfile cannot be used with PyOpenSSL"!
            sys.stderr.write('WARNING: MongoDB backend (based on pymongo) does not support dbCRLFile!\n')
            self.Configuration['dbCRLFile'] = None

      # Legacy parameter settings:
      for option in [ 'dbCAFile', 'dbCRLFile', 'dbCertFile', 'dbKeyFile', 'dbCertKeyFile' ]:
         if ((self.Configuration[option] != None) and
             (self.Configuration[option].upper() in [ 'NONE', 'IGNORE' ])):
            self.Configuration[option] = None

      # print(self.Configuration)


   # ###### Get backend #####################################################
   def getBackend(self):
      return self.Configuration['dbBackend']


   # ###### Create new database client instance #############################
   def createClient(self):

      # ====== MySQL/MariaDB ================================================
      if self.Configuration['dbBackend'] in [ 'MySQL', 'MariaDB' ]:
         try:
            import mysql.connector
         except Exception as e:
            sys.stderr.write('ERROR: Cannot load the Python MySQL Connector module: ' + str(e) + """
Try:
 * Ubuntu/Debian: (get DEB package from: https://downloads.mysql.com/archives/c-python/)
 * Fedora:        sudo dnf install -y mysql-connector-python3
 * FreeBSD:       sudo pkg install -y py311-mysql-connector-python
""")
            sys.exit(1)

         ssl_disabled                = False
         ssl_verify_cert             = True
         ssl_verify_identity         = True
         if self.Configuration['dbConnectionFlags'] != None:
            for flag in self.Configuration['dbConnectionFlags']:
               if flag == 'DisableTLS':
                  sys.stderr.write("WARNING: TLS explicitly disabled. CONFIGURE TLS PROPERLY!!\n")
                  ssl_disabled = True
               elif flag == 'AllowInvalidCertificate':
                  ssl_verify_cert = False
                  sys.stderr.write("WARNING: TLS certificate check explicitliy disabled. CONFIGURE TLS PROPERLY!!\n")
               elif flag == 'AllowInvalidHostname':
                  ssl_verify_identity = False
                  sys.stderr.write("TLS hostname check explicitliy disabled. CONFIGURE TLS PROPERLY!!\n")
         try:
            caFile = self.Configuration['dbCAFile']
            if caFile == None:
               caFile = ''   # Otherwise: "Missing ssl_ca argument." if caFile is None.
            self.dbConnection = mysql.connector.connect(
               host                = self.Configuration['dbServer'],
               port                = self.Configuration['dbPort'],
               user                = self.Configuration['dbUser'],
               password            = self.Configuration['dbPassword'],
               database            = self.Configuration['database'],
               ssl_disabled        = ssl_disabled,
               ssl_verify_identity = ssl_verify_identity,
               ssl_verify_cert     = ssl_verify_cert,
               ssl_ca              = caFile,
               # ssl_crl             = self.Configuration['dbCRLFile'],   !!! This does not work! !!!
               ssl_key             = self.Configuration['dbCertFile'],
               ssl_cert            = self.Configuration['dbKeyFile'],
            )
            self.dbConnection.autocommit = False
            self.dbCursor = self.dbConnection.cursor()
         except Exception as e:
            sys.stderr.write('ERROR: Unable to connect to the database: ' + str(e) + '\n')
            sys.exit(1)
         return { 'connection': self.dbConnection,
                  'cursor':     self.dbCursor }

      # ====== PostgreSQL ===================================================
      elif self.Configuration['dbBackend'] == 'PostgreSQL':
         try:
            import psycopg2
         except Exception as e:
            sys.stderr.write('ERROR: Cannot load the Python PostgreSQL module: ' + str(e) + """
Try:
 * Ubuntu/Debian: sudo apt get install -y python3-psycopg
 * Fedora:        sudo dnf install -y python3-psycopg2
 * FreeBSD:       sudo pkg install -y py311-psycopg2
""")
            sys.exit(1)

         ssl_mode = 'verify-full'
         if self.Configuration['dbConnectionFlags'] != None:
            for flag in self.Configuration['dbConnectionFlags']:
               if flag == 'DisableTLS':
                  sys.stderr.write("WARNING: TLS explicitly disabled. CONFIGURE TLS PROPERLY!!\n")
                  ssl_mode = 'disable'
               elif flag == 'AllowInvalidCertificate':
                  ssl_mode = 'require'
                  sys.stderr.write("WARNING: TLS certificate check explicitliy disabled. CONFIGURE TLS PROPERLY!!\n")
               elif flag == 'AllowInvalidHostname':
                  ssl_mode = 'verify-ca'
                  sys.stderr.write("TLS hostname check explicitliy disabled. CONFIGURE TLS PROPERLY!!\n")
         try:
            self.dbConnection = psycopg2.connect(
               host        = self.Configuration['dbServer'],
               port        = self.Configuration['dbPort'],
               user        = self.Configuration['dbUser'],
               password    = self.Configuration['dbPassword'],
               dbname      = self.Configuration['database'],
               sslmode     = ssl_mode,
               sslrootcert = self.Configuration['dbCAFile'],
               sslcrl      = self.Configuration['dbCRLFile'],
               sslkey      = self.Configuration['dbCertFile'],
               sslcert     = self.Configuration['dbKeyFile'])
            self.dbConnection.autocommit = False
            self.dbCursor = self.dbConnection.cursor()
         except Exception as e:
            sys.stderr.write('ERROR: Unable to connect to the database: ' + str(e) + '\n')
            sys.exit(1)
         return { 'connection': self.dbConnection,
                  'cursor':     self.dbCursor }

      # ====== MongoDB ======================================================
      if self.Configuration['dbBackend'] == 'MongoDB':
         try:
            import pymongo
         except Exception as e:
            sys.stderr.write('ERROR: Cannot load the Python MongoDB module: ' + str(e) + """
Try:
 * Ubuntu/Debian: sudo apt install -y python3-pymongo python3-snappy python3-zstandard
 * Fedora:        sudo dnf install -y python3-pymongo python3-snappy python3-zstandard
 * FreeBSD:       sudo pkg install -y py311-pymongo py311-python-snappy py311-zstandard
""")
            sys.exit(1)

         tls                         = True
         tlsAllowInvalidHostnames    = False
         tlsAllowInvalidCertificates = False
         if self.Configuration['dbConnectionFlags'] != None:
            for flag in self.Configuration['dbConnectionFlags']:
               if flag == 'DisableTLS':
                  tls = False
                  sys.stderr.write("WARNING: TLS explicitly disabled. CONFIGURE TLS PROPERLY!!\n")
               elif flag == 'AllowInvalidCertificate':
                  tlsAllowInvalidCertificates = True
                  sys.stderr.write("WARNING: TLS certificate check explicitliy disabled. CONFIGURE TLS PROPERLY!!\n")
               elif flag == 'AllowInvalidHostname':
                  tlsAllowInvalidHostnames = True
                  sys.stderr.write("TLS hostname check explicitliy disabled. CONFIGURE TLS PROPERLY!!\n")
         try:
            self.dbConnection = pymongo.MongoClient(
               host                        = self.Configuration['dbServer'],
               port                        = int(self.Configuration['dbPort']),
               tls                         = tls,
               tlsAllowInvalidHostnames    = tlsAllowInvalidHostnames,
               tlsAllowInvalidCertificates = tlsAllowInvalidCertificates,
               tlsCAFile                   = self.Configuration['dbCAFile'],
               tlsCRLFile                  = self.Configuration['dbCRLFile'],
               tlsCertificateKeyFile       = self.Configuration['dbCertKeyFile'],
               compressors                 = "zstd,zlib,snappy"
            )
            self.database = self.dbConnection[str(self.Configuration['database'])]
            self.database.authenticate(self.Configuration['dbUser'],
                                       self.Configuration['dbPassword'])
         except Exception as e:
            sys.stderr.write('ERROR: Unable to connect to the database: ' + str(e) + '\n')
            sys.exit(1)
         return { 'connection': self.dbConnection,
                  'database':   self.database }

      return None


   # ###### Destroy new database client instance ############################
   def destroyClient(self):
      # ====== MySQL/MariaDB ================================================
      if self.Configuration['dbBackend'] in [ 'MySQL', 'MariaDB' ]:
         try:
            self.dbConnection.close()
         except Exception as e:
            pass

      # ====== PostgreSQL ===================================================
      elif self.Configuration['dbBackend'] == 'PostgreSQL':
         try:
            self.dbConnection.close()
         except Exception as e:
            pass

      # ====== MongoDB ======================================================
      if self.Configuration['dbBackend'] == 'MongoDB':
         try:
            self.dbConnection.close()
         except Exception as e:
            pass


   # ###### Query database ##################################################
   def query(self, request):
      self.execute(request)

      # ====== MySQL/MariaDB ================================================
      if self.Configuration['dbBackend'] in [ 'MySQL', 'MariaDB' ]:
         rows = self.dbCursor.fetchall()
         return rows

      # ====== PostgreSQL ===================================================
      elif self.Configuration['dbBackend'] == 'PostgreSQL':
         rows = self.dbCursor.fetchall()
         return rows

      raise Exception('Backend not implemented!')


   # ###### Execute update ##################################################
   def execute(self, request):

      # ====== MySQL/MariaDB ================================================
      if self.Configuration['dbBackend'] in [ 'MySQL', 'MariaDB' ]:
         try:
            self.dbCursor.execute(request)
         except Exception as e:
            sys.stderr.write('ERROR: Execution failed: ' + str(e) + '\n')
            sys.exit(1)

      # ====== PostgreSQL ===================================================
      elif self.Configuration['dbBackend'] == 'PostgreSQL':
         try:
            self.dbCursor.execute(request)
         except Exception as e:
            sys.stderr.write('ERROR: Execution failed: ' + str(e) + '\n')
            sys.exit(1)

      else:
         raise Exception('Backend not implemented!')


   # ###### Commit ##########################################################
   def commit(self):

      # ====== MySQL/MariaDB ================================================
      if self.Configuration['dbBackend'] in [ 'MySQL', 'MariaDB' ]:
         try:
            self.dbConnection.commit()
         except Exception as e:
            sys.stderr.write('ERROR: Commit failed: ' + str(e) + '\n')
            sys.exit(1)

      # ====== PostgreSQL ===================================================
      elif self.Configuration['dbBackend'] == 'PostgreSQL':
         try:
            self.dbConnection.commit()
         except Exception as e:
            sys.stderr.write('ERROR: Commit failed: ' + str(e) + '\n')
            sys.exit(1)

      else:
         raise Exception('Backend not implemented!')


   # ###### Rollback ########################################################
   def rollback(self):

      # ====== MySQL/MariaDB ================================================
      if self.Configuration['dbBackend'] in [ 'MySQL', 'MariaDB' ]:
         try:
            self.dbConnection.rollback()
         except Exception as e:
            sys.stderr.write('ERROR: Rollback failed: ' + str(e) + '\n')
            sys.exit(1)

      # ====== PostgreSQL ===================================================
      elif self.Configuration['dbBackend'] == 'PostgreSQL':
         try:
            self.dbConnection.rollback()
         except Exception as e:
            sys.stderr.write('ERROR: Rollback failed: ' + str(e) + '\n')
            sys.exit(1)

      else:
         raise Exception('Backend not implemented!')


   # ###### Query MongoDB database ###########################################
   def queryMongoDB(self, table, request):
      # ====== MongoDB ======================================================
      if not self.Configuration['dbBackend'] == 'MongoDB':
         raise Exception('This method only works for MongoDB!')

      rows = self.database[table].find(request, batch_size=1000000).batch_size(1000000)
      return rows


# ###### Convert IPv4-mapped IPv6 address to IPv4 address, if possible ######
def unmap(address):
   if isinstance(address, ipaddress.IPv6Address) and address.ipv4_mapped:
      return address.ipv4_mapped
   return address
