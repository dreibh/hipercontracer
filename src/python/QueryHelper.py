#!/usr/bin/env python3
# ==========================================================================
#     _   _ _ ____            ____          _____
#    | | | (_)  _ \ ___ _ __ / ___|___  _ _|_   _| __ __ _  ___ ___ _ __
#    | |_| | | |_) / _ \ '__| |   / _ \| '_ \| || '__/ _` |/ __/ _ \ '__|
#    |  _  | |  __/  __/ |  | |__| (_) | | | | || | | (_| | (_|  __/ |
#    |_| |_|_|_|   \___|_|   \____\___/|_| |_|_||_|  \__,_|\___\___|_|
#
#       ---  High-Performance Connectivity Tracer (HiPerConTracer)  ---
#                 https://www.nntb.no/~dreibh/hipercontracer/
# ==========================================================================
#
# High-Performance Connectivity Tracer (HiPerConTracer)
# Copyright (C) 2015-2026 by Thomas Dreibholz
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# Contact: dreibh@simula.no

import configparser
import ipaddress
import sys

if sys.version_info < (3, 11):
   sys.stderr.write('ERROR: ' + sys.argv[0] + ' requires Python 3.11 or later!\n')
   sys.exit(1)

from typing import Any, Final, TextIO, TYPE_CHECKING

# Usually, a backend is imported only when needed. This avoids unnecessary
# dependencies. However, all imports are needed when type-checking is
# enabled (e.g. MyPy):
if TYPE_CHECKING:
   import mysql.connector
   import psycopg2
   import pymongo


class DatabaseConfiguration:

   # ###### Constructor #####################################################
   def __init__(self, configurationFileName : str) -> None:
      self.Configuration : dict[str, Any] = {
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
      self.AvailableConnectionFlags : Final[list[str]] = [
         'DisableTLS',
         'AllowInvalidHostname',
         'AllowInvalidCertificate'
      ]
      self.dbConnection : \
         mysql.connector.pooling.PooledMySQLConnection | \
         mysql.connector.abstracts.MySQLConnectionAbstract | \
         psycopg2.extensions.connection | \
         pymongo.MongoClient[dict[str, Any]] | \
         None = None
      self.dbCursor : \
          mysql.connector.abstracts.MySQLCursorAbstract | \
          psycopg2.extensions.cursor | \
          None = None
      self.readConfiguration(configurationFileName)


   # ###### Destructor ######################################################
   def __del__(self) -> None:
      self.destroyClient()


   # ###### Read database configuration #####################################
   def readConfiguration(self, configurationFileName : str) -> None:

      class CaseSensitiveConfigParser(configparser.RawConfigParser):
         def optionxform(self, optionstr : str) -> str:
            return optionstr

      parsedConfigFile : configparser.RawConfigParser = \
         CaseSensitiveConfigParser()
      try:
         parsedConfigFile.read_string('[root]\n' + open(configurationFileName, 'r').read())
      except Exception as e:
          sys.stderr.write('ERROR: Unable to read database configuration file ' +  sys.argv[1] + ': ' + str(e) + '\n')
          sys.exit(1)

      # ====== Read parameters ==============================================
      parameterName : str
      for parameterName in self.Configuration.keys():
          if parsedConfigFile.has_option('root', parameterName.lower()):
             value : str = \
                parsedConfigFile.get('root', parameterName.lower())
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
                if value.upper() in [ 'NONE', 'IGNORE' ]:
                   self.Configuration['dbConnectionFlags'] = None
                else:
                   self.Configuration['dbConnectionFlags'] = value.split(' ')
                   for flag in self.Configuration['dbConnectionFlags']:
                      if not flag in self.AvailableConnectionFlags:
                         sys.stderr.write('ERROR: Invalid dbConnectionFlags flag ' + flag + '!\n')
                         sys.exit(1)
             else:
                self.Configuration[parameterName] = value

      # ====== Set value to None if necessary ===============================
      for option in [ 'dbCAFile', 'dbCRLFile', 'dbCertFile', 'dbKeyFile', 'dbCertKeyFile' ]:
         if ((self.Configuration[option] is not None) and
             (self.Configuration[option].upper() in [ 'NONE', 'IGNORE' ])):
            self.Configuration[option] = None

      # ====== Check parameters =============================================
      if self.Configuration['dbPort'] == 0:
         if self.Configuration['dbBackend'] in [ 'MySQL', 'MariaDB' ]:
            self.Configuration['dbPort'] = 3306
         elif self.Configuration['dbBackend'] == 'PostgreSQL':
            self.Configuration['dbPort'] = 5432
         elif self.Configuration['dbBackend'] == 'MongoDB':
            self.Configuration['dbPort'] = 27017

      if self.Configuration['dbBackend'] in [ 'MySQL', 'MariaDB' ]:
         if self.Configuration['dbCertKeyFile'] is not None:
            sys.stderr.write('ERROR: MySQL/MariaDB backend expects dbCertFile+dbKeyFile, not dbCertKeyFile!\n')
            sys.exit(1)
         if self.Configuration['dbCRLFile'] is not None:
            sys.stderr.write('WARNING: MySQL/MariaDB backend (based on mysql-connector-python) does not support dbCRLFile!\n')
      elif self.Configuration['dbBackend'] == 'PostgreSQL':
         if self.Configuration['dbCertKeyFile'] is not None:
            sys.stderr.write('ERROR: PostgreSQL backend expects dbCertFile+dbKeyFile, not dbCertKeyFile!\n')
            sys.exit(1)
      elif self.Configuration['dbBackend'] == 'MongoDB':
         if self.Configuration['dbCertFile'] is not None or \
            self.Configuration['dbKeyFile']  is not None :
            sys.stderr.write('ERROR: MongoDB backend expects dbCertKeyFile, not dbCertFile+dbKeyFile!\n')
            sys.exit(1)
         if self.Configuration['dbCRLFile'] is not None:
            # NOTE: Using a CRL with PyMongo fails with "ssl_crlfile cannot be used with PyOpenSSL"!
            sys.stderr.write('WARNING: MongoDB backend (based on pymongo) does not support dbCRLFile!\n')
            self.Configuration['dbCRLFile'] = None

      # print(self.Configuration)


   # ###### Get backend #####################################################
   def getBackend(self) -> str:
      return str(self.Configuration['dbBackend'])


   # ###### Create new database client instance #############################
   def createClient(self) -> dict[str, Any] | None:

      connectParameters : dict[str, Any]

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

         ssl_disabled        : bool = False
         ssl_verify_cert     : bool = True
         ssl_verify_identity : bool = True
         if self.Configuration['dbConnectionFlags'] is not None:
            for flag in self.Configuration['dbConnectionFlags']:
               if flag == 'DisableTLS':
                  sys.stderr.write('WARNING: TLS explicitly disabled. CONFIGURE TLS PROPERLY!!\n')
                  ssl_disabled = True
               elif flag == 'AllowInvalidCertificate':
                  ssl_verify_cert = False
                  sys.stderr.write('WARNING: TLS certificate check explicitliy disabled. CONFIGURE TLS PROPERLY!!\n')
               elif flag == 'AllowInvalidHostname':
                  ssl_verify_identity = False
                  sys.stderr.write('TLS hostname check explicitliy disabled. CONFIGURE TLS PROPERLY!!\n')
         connectParameters = {
            'host':                self.Configuration['dbServer'],
            'port':                self.Configuration['dbPort'],
            'user':                self.Configuration['dbUser'],
            'password':            self.Configuration['dbPassword'],
            'database':            self.Configuration['database'],
            'ssl_disabled':        ssl_disabled,
            'ssl_verify_identity': ssl_verify_identity,
            'ssl_verify_cert':     ssl_verify_cert
         }
         if not ssl_disabled:
            if self.Configuration['dbCAFile'] is not None:
               connectParameters['ssl_ca'] = self.Configuration['dbCAFile']
            # !!! There is no CRL support in mysql.connector! !!!
            # if self.Configuration['dbCRLFile'] is not None:
            #    connectParameters['ssl_crl'] = self.Configuration['dbCRLFile']
            if self.Configuration['dbKeyFile'] is not None:
               connectParameters['ssl_key'] = self.Configuration['dbKeyFile']
            if self.Configuration['dbCertFile'] is not None:
               connectParameters['ssl_cert'] = self.Configuration['dbCertFile']
         try:
            self.dbConnection = mysql.connector.connect(**connectParameters)
            self.dbCursor     = self.dbConnection.cursor()
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
 * Ubuntu/Debian: sudo apt-get install -y python3-psycopg
 * Fedora:        sudo dnf install -y python3-psycopg2
 * FreeBSD:       sudo pkg install -y py311-psycopg2
""")
            sys.exit(1)

         ssl_mode : str = 'verify-full'
         if self.Configuration['dbConnectionFlags'] is not None:
            for flag in self.Configuration['dbConnectionFlags']:
               if flag == 'DisableTLS':
                  sys.stderr.write('WARNING: TLS explicitly disabled. CONFIGURE TLS PROPERLY!!\n')
                  ssl_mode = 'disable'
               elif flag == 'AllowInvalidCertificate':
                  ssl_mode = 'require'
                  sys.stderr.write('WARNING: TLS certificate check explicitliy disabled. CONFIGURE TLS PROPERLY!!\n')
               elif flag == 'AllowInvalidHostname':
                  ssl_mode = 'verify-ca'
                  sys.stderr.write('TLS hostname check explicitliy disabled. CONFIGURE TLS PROPERLY!!\n')
         connectParameters = {
            'host':     self.Configuration['dbServer'],
            'port':     self.Configuration['dbPort'],
            'user':     self.Configuration['dbUser'],
            'password': self.Configuration['dbPassword'],
            'dbname':   self.Configuration['database'],
            'sslmode':  ssl_mode
         }
         if ssl_mode != 'disable':
            if self.Configuration.get('dbCAFile') is not None:
               connectParameters['sslrootcert'] = self.Configuration['dbCAFile']
            if self.Configuration.get('dbCRLFile') is not None:
               connectParameters['sslcrl'] = self.Configuration['dbCRLFile']
            if self.Configuration.get('dbKeyFile') is not None:
               connectParameters['sslkey'] = self.Configuration['dbKeyFile']
            if self.Configuration.get('dbCertFile') is not None:
               connectParameters['sslcert'] = self.Configuration['dbCertFile']
         try:
            self.dbConnection = psycopg2.connect(**connectParameters)
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
 * Ubuntu/Debian: sudo apt-get install -y python3-pymongo python3-snappy python3-zstandard
 * Fedora:        sudo dnf install -y python3-pymongo python3-snappy python3-zstandard
 * FreeBSD:       sudo pkg install -y py311-pymongo py311-python-snappy py311-zstandard
""")
            sys.exit(1)

         tls                         : bool = True
         tlsAllowInvalidHostnames    : bool = False
         tlsAllowInvalidCertificates : bool = False
         if self.Configuration['dbConnectionFlags'] is not None:
            for flag in self.Configuration['dbConnectionFlags']:
               if flag == 'DisableTLS':
                  tls = False
                  sys.stderr.write('WARNING: TLS explicitly disabled. CONFIGURE TLS PROPERLY!!\n')
               elif flag == 'AllowInvalidCertificate':
                  tlsAllowInvalidCertificates = True
                  sys.stderr.write('WARNING: TLS certificate check explicitliy disabled. CONFIGURE TLS PROPERLY!!\n')
               elif flag == 'AllowInvalidHostname':
                  tlsAllowInvalidHostnames = True
                  sys.stderr.write('TLS hostname check explicitliy disabled. CONFIGURE TLS PROPERLY!!\n')
         connectParameters = {
            'host':                        self.Configuration['dbServer'],
            'port':                        int(self.Configuration['dbPort']),
            'tls':                         tls,
            'tlsAllowInvalidHostnames':    tlsAllowInvalidHostnames,
            'tlsAllowInvalidCertificates': tlsAllowInvalidCertificates,
            'compressors':                 'zstd,zlib,snappy'
         }
         if tls:
            if self.Configuration.get('dbCAFile'):
               connectParameters['tlsCAFile'] = self.Configuration['dbCAFile']
            if self.Configuration.get('dbCRLFile'):
               connectParameters['tlsCRLFile'] = self.Configuration['dbCRLFile']
            if self.Configuration.get('dbCertKeyFile'):
               connectParameters['tlsCertificateKeyFile'] = self.Configuration['dbCertKeyFile']
         try:
            self.dbConnection = pymongo.MongoClient(**connectParameters)
            self.database = self.dbConnection[str(self.Configuration['database'])]
            self.database.authenticate(self.Configuration['dbUser'],
                                       self.Configuration['dbPassword'])
            self.dbConnection.admin.command('ping')
         except Exception as e:
            sys.stderr.write('ERROR: Unable to connect to the database: ' + str(e) + '\n')
            sys.exit(1)
         return { 'connection': self.dbConnection,
                  'database':   self.database }

      return None


   # ###### Destroy new database client instance ############################
   def destroyClient(self) -> None:

      if self.dbConnection is not None:

         # ====== MySQL/MariaDB =============================================
         if self.Configuration['dbBackend'] in [ 'MySQL', 'MariaDB' ]:
            try:
               self.dbConnection.close()
            except Exception as e:
               pass

         # ====== PostgreSQL ================================================
         elif self.Configuration['dbBackend'] == 'PostgreSQL':
            try:
               self.dbConnection.close()
            except Exception as e:
               pass

         # ====== MongoDB ===================================================
         elif self.Configuration['dbBackend'] == 'MongoDB':
            try:
               self.dbConnection.close()
            except Exception as e:
               pass


   # ###### Query database ##################################################
   def query(self, request : str) -> tuple[Any, Any]:

      assert self.dbCursor is not None

      # ====== Execute query ================================================
      self.execute(request)

      # ====== MySQL/MariaDB ================================================
      if self.Configuration['dbBackend'] in [ 'MySQL', 'MariaDB' ]:
         rows = self.dbCursor.fetchall()
         return rows, self.dbCursor.description

      # ====== PostgreSQL ===================================================
      elif self.Configuration['dbBackend'] == 'PostgreSQL':
         rows = self.dbCursor.fetchall()
         return rows, self.dbCursor.description

      raise Exception('Backend not implemented!')


   # ###### Execute update ##################################################
   def execute(self, request : str) -> None:

      assert self.dbCursor is not None

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
   def commit(self) -> None:

      assert self.dbConnection is not None

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
   def rollback(self) -> None:

      assert self.dbConnection is not None

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


   # ###### Query MongoDB database ##########################################
   def queryMongoDB(self,
                    table   : str,
                    request : dict[str, Any],
                    orderBy : list[tuple[str, int]] | None = None) -> Any:

      # ====== MongoDB ======================================================
      if not self.Configuration['dbBackend'] == 'MongoDB':
         raise Exception('This method only works for MongoDB!')

      rows = self.database[table].find(request, sort = orderBy, batch_size=1000000).batch_size(1000000)
      return rows


# ###### Convert IPv4-mapped IPv6 address to IPv4 address, if possible ######
def unmap(address : ipaddress.IPv4Address | ipaddress.IPv6Address) -> ipaddress.IPv4Address | ipaddress.IPv6Address:
   if isinstance(address, ipaddress.IPv6Address) and address.ipv4_mapped:
      return address.ipv4_mapped
   return address
