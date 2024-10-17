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
# Copyright (C) 2015-2025 by Thomas Dreibholz
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

import os
import shutil
import sys


# Certificate Types:
CRT_RootCA          = 1
CRT_IntermediateCA  = 2
CRT_LeafCA          = 3
CRT_Server          = 4
CRT_User            = 5

# Some defaults:
DefaultCAKeyLength   = 8192
DefaultCertKeyLength = 4096


# ###### Execute command ####################################################
def execute(command, mayFail = False):
   result = 1
   try:
      sys.stdout.write('\x1b[37m' + command + '\x1b[0m\n')
      result = os.system(command)
   except Exception as e:
      sys.stderr.write('FAILED COMMAND:\n' + command + '\n')
      sys.exit(1)
   if not mayFail:
      assert result == 0
   return result



# ###### CA #################################################################
GlobalCRLSet             = set([])
DefaultGlobalCRLFileName = 'Test.crl'

class CA:
   # ###### Constructor #####################################################
   def __init__(self, mainDirectory, name, parentCA, subject, certType,
                days = 10 * 365, keyLength = DefaultCAKeyLength,
                globalCRLFileName = DefaultGlobalCRLFileName):
      sys.stdout.write('\x1b[34mCreating CA ' + name + ' ...\x1b[0m\n')

      self.MainDirectory     = os.path.abspath(mainDirectory)
      self.Directory         = os.path.join(self.MainDirectory, name)
      self.GlobalCRLFileName = os.path.join(self.MainDirectory, globalCRLFileName)
      self.ParentCA          = parentCA
      self.Subject           = subject
      self.CAName            = name
      self.CertType          = certType
      if self.CertType == CRT_RootCA:
         self.Extension = 'v3_ca'
      elif self.CertType == CRT_IntermediateCA:
         self.Extension = 'v3_intermediate_ca'
      elif self.CertType == CRT_LeafCA:
         self.Extension = 'v3_leaf_ca'
      else:
         raise Exception('Invalid certificate type')

      self.DefaultDays       = days
      self.KeyLength         = keyLength

      self.CertsDirectory    = os.path.join(self.Directory, 'certs')
      self.NewCertsDirectory = os.path.join(self.Directory, 'newcerts')
      self.CRLDirectory      = os.path.join(self.Directory, 'crl')
      self.PrivateDirectory  = os.path.join(self.Directory, 'private')

      self.KeyFileName       = os.path.join(self.PrivateDirectory, name + '.key')
      self.PasswordFileName  = os.path.join(self.PrivateDirectory, name + '.password')
      self.CSRFileName       = os.path.join(self.Directory,        name + '.csr')
      self.CertFileName      = os.path.join(self.CertsDirectory,   name + '.crt')
      self.ChainFileName     = os.path.join(self.CertsDirectory,   name + '-chain.pem')
      self.CRLFileName       = os.path.join(self.CRLDirectory,     name + '.crl')

      # ====== Create directories, if not already existing ==================
      for directory in [
            self.CertsDirectory,      # For issued certificates
            self.NewCertsDirectory,   # Certificates to be signed
            self.CRLDirectory          # Certificates Revocation Lists (CRLs)
         ]:
         os.makedirs(os.path.join(self.Directory, directory),
                     mode     = 0o755,
                     exist_ok = True)
      # Directory for keys:
      os.makedirs(os.path.join(self.Directory, 'private'),
                  mode     = 0o700,
                  exist_ok = True)

      # ====== Create index.txt, serial and crlnumber files =================
      self.IndexFileName = os.path.join(self.Directory, 'index.txt')
      if not os.path.isfile(self.IndexFileName):
         indexFile = open(self.IndexFileName, 'w', encoding='utf-8')
         indexFile.close()

      self.SerialFileName = os.path.join(self.Directory, 'serial')
      if not os.path.isfile(self.SerialFileName):
         serialFile = open(self.SerialFileName, 'w', encoding='utf-8')
         serialFile.write('00')
         serialFile.close()

      self.CRLNumberFileName = os.path.join(self.Directory, 'crlnumber')
      if not os.path.isfile(self.CRLNumberFileName):
         crlnumberFile = open(self.CRLNumberFileName, 'w', encoding='utf-8')
         crlnumberFile.write('00')
         crlnumberFile.close()

      # ====== Generate CA configuration file ===============================
      self.ConfigFileName = os.path.join(self.Directory, name + '.conf')
      if not os.path.isfile(self.ConfigFileName):
         configFile = open(os.path.join(self.ConfigFileName), 'w', encoding='utf-8')
         configFile.write("""
# ###########################################################################
# #### Test CA Configuration. See `man ca` for details!                  ####
# ###########################################################################

[ ca ]
default_ca                      = CA_default

[ CA_default ]
dir                             = """ + self.Directory          + """
certs                           = """ + self.CertsDirectory     + """
crl_dir                         = """ + self.CRLDirectory       + """
new_certs_dir                   = """ + self.NewCertsDirectory  + """
database                        = """ + self.IndexFileName      + """
serial                          = """ + self.SerialFileName     + """
RANDFILE                        = """ + self.Directory          + """

# The root key and root certificate.
private_key                     = """ + self.KeyFileName        + """
certificate                     = """ + self.CertFileName       + """

# For certificate revocation lists.
crlnumber                       = """ + self.CRLNumberFileName  + """
crl                             = """ + self.CRLFileName        + """
crl_extensions                  = crl_ext

default_days                    = """ + str(self.DefaultDays)   + """
default_crl_days                = 30
default_md                      = sha512

policy                          = policy_any
email_in_dn                     = yes

name_opt                        = ca_default   # Subject name display option
cert_opt                        = ca_default   # Certificate display option
copy_extensions                 = none         # Don't copy extensions from request

[ policy_any ]
countryName                     = supplied
stateOrProvinceName             = optional
organizationName                = optional
organizationalUnitName          = optional
commonName                      = supplied
emailAddress                    = optional

# ====== PKCS#10 certificate request and certificate settings ===============
[ req ]
# See `man req` for details!
default_bits                    = 4096
default_md                      = sha512
distinguished_name              = req_distinguished_name
string_mask                     = utf8only

# Extension to add when the -x509 option is used:
x509_extensions                 = v3_ca

[ req_distinguished_name ]
countryName                     = Country Name (2 letter code)
stateOrProvinceName             = State or Province Name
localityName                    = Locality Name
0.organizationName              = Organization Name
organizationalUnitName          = Organizational Unit Name
commonName                      = Common Name
emailAddress                    = Email Address

# Optionally, specify some defaults.
countryName_default             = NO
stateOrProvinceName_default     = Norway
localityName_default            = Oslo
0.organizationName_default      =
organizationalUnitName_default  =
emailAddress_default            =

# ====== Extensions for CRLs ================================================
[ crl_ext ]
# See `man x509v3_config` for details!
authorityKeyIdentifier = keyid:always

# ====== Settings for root CA ===============================================
[ v3_ca ]
# See `man x509v3_config` for details!
subjectKeyIdentifier   = hash
basicConstraints       = critical, CA:true              # <<-- CA certificate
keyUsage               = critical, digitalSignature, cRLSign, keyCertSign

# ====== Settings for intermediate CA =======================================
[ v3_intermediate_ca ]
# See `man x509v3_config` for details!
subjectKeyIdentifier   = hash
authorityKeyIdentifier = keyid:always,issuer
basicConstraints       = critical, CA:true              # <<-- CA certificate
keyUsage               = critical, digitalSignature, cRLSign, keyCertSign

# ====== Settings for a leaf CA =============================================
[ v3_leaf_ca ]
# See `man x509v3_config` for details!
subjectKeyIdentifier   = hash
authorityKeyIdentifier = keyid:always,issuer
basicConstraints       = critical, CA:true, pathlen:0   # <<-- CA, but no sub-CAs
keyUsage               = critical, digitalSignature, cRLSign, keyCertSign

# ====== Settings for a user certificate ====================================
[ user_cert ]
# Extensions for client certificates (`man x509v3_config`).
basicConstraints       = CA:FALSE
subjectKeyIdentifier   = hash
keyUsage               = critical, nonRepudiation, digitalSignature, keyEncipherment
extendedKeyUsage       = clientAuth, emailProtection

# ====== Settings for a server certificate ==================================
[ server_cert ]
# Extensions for server certificates (`man x509v3_config`).
basicConstraints       = CA:FALSE
subjectKeyIdentifier   = hash
keyUsage               = critical, digitalSignature, keyEncipherment
extendedKeyUsage       = serverAuth
subjectAltName         = ${ENV::SAN}
""")
         configFile.close()


      # ====== Generate CA password =========================================
      if not os.path.isfile(self.PasswordFileName):
         sys.stdout.write('\x1b[33mGenerating CA password ' + self.PasswordFileName + ' ...\x1b[0m\n')
         execute('pwgen -sy 128 >' + self.PasswordFileName)
         assert os.path.isfile(self.PasswordFileName)


      # ====== Generate CA key ==============================================
      if not os.path.isfile(self.KeyFileName):
         sys.stdout.write('\x1b[33mGenerating CA key ' + self.KeyFileName + ' ...\x1b[0m\n')
         execute('openssl genrsa'  +
                 ' -aes256 '       +   # Use AES-256 encryption
                 ' -out '          + self.KeyFileName      +
                 ' -passout file:' + self.PasswordFileName +
                 ' ' + str(self.KeyLength))
         assert os.path.isfile(self.KeyFileName)

         # Make sure invalid files are removed:
         for fileName in [ self.CSRFileName, self.CertFileName, self.ChainFileName, self.CRLFileName ]:
            if os.path.exists(fileName):
               os.remove(fileName)


      # ====== Generate self-signed root CA certificate =====================
      if parentCA == None:
         if not os.path.isfile(self.CertFileName):
            sys.stdout.write('\x1b[33mGenerating self-signed root CA certificate ' + self.CertFileName + ' ...\x1b[0m\n')
            execute('SAN="" openssl req' +
                    ' -x509'             +   # Self-signed
                    ' -config '          + self.ConfigFileName      +
                    ' -extensions v3_ca' +
                    ' -utf8 -subj "'     + str(self.Subject) + '"'  +
                    ' -days '            + str(self.DefaultDays)    +
                    ' -key '             + self.KeyFileName         +
                    ' -passin file:'     + self.PasswordFileName    +
                    ' -out '             + self.CertFileName)
            assert os.path.isfile(self.CertFileName)

            # Get the whole chain (just this certificate):
            shutil.copy2(self.CertFileName, self.ChainFileName)

         # ------ Set reference to root CA (i.e. to itself) -----------------
         self.RootCA = self

      # ====== Generate CA certificate signed by parent CA ==================
      else:
         # ------ Create CSR ------------------------------------------------
         if not os.path.isfile(self.CertFileName):
            self.CSRFileName = os.path.join(self.Directory, name + '.csr')
            sys.stdout.write('\x1b[33mGenerating CSR ' + self.CSRFileName + ' ...\x1b[0m\n')
            execute('SAN="" openssl req' +
                    ' -new'              +   # Not self-signed
                    ' -config '          + self.ConfigFileName     +
                    ' -extensions v3_ca' +
                    ' -utf8 -subj "'     + str(self.Subject) + '"' +
                    ' -key '             + self.KeyFileName        +
                    ' -passin file:'     + self.PasswordFileName   +
                    ' -out '             + self.CSRFileName)
            assert os.path.isfile(self.CSRFileName)

         # ------ Sign CSR --------------------------------------------------
         if not os.path.isfile(self.CertFileName):
            sys.stdout.write('\x1b[33mGetting CSR ' + self.CSRFileName + ' signed by ' + parentCA.CAName + ' ...\x1b[0m\n')
            execute('SAN="" openssl ca' +
                    ' -batch'            +
                    ' -notext'           +
                    ' -config '          + parentCA.ConfigFileName    +
                    ' -extensions '      + self.Extension             +
                    ' -utf8 -subj "'     + str(self.Subject) + '"'    +
                    ' -days '            + str(parentCA.DefaultDays)  +
                    ' -passin file:'     + parentCA.PasswordFileName  +
                    ' -in '              + self.CSRFileName           +
                    ' -out '             + self.CertFileName)
            assert os.path.isfile(self.CertFileName)

            # ------ Get the whole chain ------------------------------------
            shutil.copy2(parentCA.ChainFileName, self.ChainFileName)
            chainFile = open(self.ChainFileName, 'a', encoding='utf-8')
            certFile  = open(self.CertFileName,  'r', encoding='utf-8')
            for line in certFile:
               chainFile.write(line)
            certFile.close()
            chainFile.close()

         # ------ Set reference to root CA ----------------------------------
         if self.ParentCA == None:
            self.RootCA = self
         else:
            self.RootCA = self.ParentCA
            while self.RootCA.ParentCA != None:
               self.RootCA = self.RootCA.ParentCA


      # ====== Generate initial CRL =========================================
      GlobalCRLSet.add(self.CRLFileName)
      self.generateCRL()


      # ====== Verify certificate ===========================================
      # Print certificate:
      execute('openssl x509 '                  +
              ' -noout'                        +   # Do not dump the encoded certificate
              ' -subject -ext subjectAltName ' +   # Print basic information
              ' -in ' + self.CertFileName)

      sys.stdout.write('\x1b[33mVerifying certificate ' + self.CertFileName + ' via ' +
                       self.ChainFileName + ' ...\x1b[0m\n')

      # Check chain -> certificate
      execute('openssl verify ' +
              ' -show_chain' +
              ' -verbose'    +
              ' -CAfile '    + self.ChainFileName +
              ' ' + self.CertFileName)

      # Check root CA -> ... -> certificate
      # NOTE: Using option "-untrusted" to mark the whole chain as untrusted
      #       works! The CAfile is always trusted, and OpenSSL will verify
      #       all certificates of the chain.
      #       -> https://stackoverflow.com/questions/25482199/verify-a-certificate-chain-using-openssl-verify
      execute('openssl verify ' +
              ' -show_chain' +
              ' -verbose'    +
              ' -CAfile '    + self.RootCA.CertFileName +
              ' -untrusted ' + self.ChainFileName +
              ' ' + self.CertFileName)


   # ###### Sign certificate ################################################
   def signCertificate(self, certificate):
      sys.stdout.write('\x1b[33mGetting CSR ' + certificate.CSRFileName + ' signed by ' + self.CAName + ' ...\x1b[0m\n')
      assert os.path.isfile(self.CSRFileName)
      execute('SAN="' + certificate.SubjectAltName + '" openssl ca' +
              ' -batch'            +
              ' -notext'           +
              ' -config '          + self.ConfigFileName            +
              ' -extensions '      + certificate.Extension          +
              ' -utf8 -subj "'     + str(certificate.Subject) + '"' +
              ' -days '            + str(self.DefaultDays)          +
              ' -passin file:'     + self.PasswordFileName          +
              ' -in '              + certificate.CSRFileName        +
              ' -out '             + certificate.CertFileName)
      assert os.path.isfile(self.CertFileName)


   # ###### Revoke certificate ##############################################
   def revokeCertificate(self, certificate):
      sys.stdout.write('\x1b[33mRevoking certificate ' + certificate.CertFileName + ' ...\x1b[0m\n')
      assert os.path.isfile(self.CertFileName)
      execute('SAN="" openssl ca' +
              ' -revoke '      + certificate.CertFileName  +
              ' -config '      + certificate.CA.ConfigFileName   +
              ' -passin file:' + certificate.CA.PasswordFileName)
      self.generateCRL()


   # ###### Generate CRL ####################################################
   def generateCRL(self):
      sys.stdout.write('\x1b[33mGenerating CRL ' + self.CRLFileName + ' ...\x1b[0m\n')
      execute('SAN="" openssl ca'     +
              ' -gencrl'       +
              ' -config '      + self.ConfigFileName     +
              ' -passin file:' + self.PasswordFileName +
              ' -out '         + self.CRLFileName)
      assert(os.path.isfile(self.CRLFileName))

      # ====== Update global CRL ============================================
      self.generateGlobalCRL(self.GlobalCRLFileName)


   # ###### Generate global CRL #############################################
   def generateGlobalCRL(self, globalCRLFileName):
      if len(GlobalCRLSet) > 0:
         cmd = 'cat'
         for crlFileName in sorted(GlobalCRLSet):
            cmd = cmd + ' ' + crlFileName
         cmd = cmd + ' >' + globalCRLFileName
         execute(cmd)
         assert(os.path.isfile(globalCRLFileName))


# ###### Server/User Certificate ############################################
class Certificate:
   def __init__(self, mainDirectory, name, ca, subjectWithoutCN, subjectAltName,
                certType = CRT_Server, keyLength = DefaultCertKeyLength):
      sys.stdout.write('\x1b[34mCreating server ' + name + ' ...\x1b[0m\n')

      self.Directory      = os.path.join(os.path.abspath(mainDirectory), name)
      self.CA             = ca
      self.Subject        = subjectWithoutCN + '/CN=' + name
      self.SubjectAltName = subjectAltName
      self.CertType       = certType
      if self.CertType == CRT_Server:
         self.Extension = 'server_cert'
      elif self.CertType == CRT_User:
         self.Extension = 'user_cert'
      else:
         raise Exception('Invalid certificate type')

      self.KeyLength        = keyLength
      self.KeyFileName      = os.path.join(self.Directory, name + '.key')
      self.CSRFileName      = os.path.join(self.Directory, name + '.csr')
      self.CertFileName     = os.path.join(self.Directory, name + '.crt')
      self.ChainFileName    = os.path.join(self.Directory, name + '-chain.pem')
      self.ChainKeyFileName = os.path.join(self.Directory, name + '-chain+key.pem')

      os.makedirs(self.Directory, exist_ok = True)

      # ====== Revoke existing certificate first ============================
      if os.path.isfile(self.CertFileName):
         self.revoke()

      # ====== Generate key =================================================
      if not os.path.isfile(self.KeyFileName):
         sys.stdout.write('\x1b[33mGenerating key ' + self.KeyFileName + ' ...\x1b[0m\n')
         execute('openssl genrsa'  +
                 ' -out '          + os.path.join(self.Directory, self.KeyFileName) +
                 ' ' + str(self.KeyLength))
         assert os.path.isfile(self.KeyFileName)

         # Make sure invalid files are removed:
         for fileName in [ self.CSRFileName, self.CertFileName, self.ChainFileName, self.ChainKeyFileName ]:
            if os.path.exists(fileName):
               os.remove(fileName)

      # ====== Generate CSR =================================================
      if not os.path.isfile(self.CSRFileName):
         sys.stdout.write('\x1b[33mGenerating CSR ' + self.CSRFileName + ' ...\x1b[0m\n')
         execute('SAN="' + self.SubjectAltName + '" openssl req' +
                 ' -new'          +   # Not self-signed
                 ' -config '      + self.CA.ConfigFileName   +
                 ' -extensions '  + self.Extension           +
                 ' -utf8 -subj "' + str(self.Subject) + '"'  +
                 ' -key '         + self.KeyFileName         +
                 ' -out '         + self.CSRFileName)
         assert os.path.isfile(self.CSRFileName)

      # ====== Obtain the CRT from the CA ===================================
      if not os.path.isfile(self.CertFileName):
         assert os.path.isfile(self.CSRFileName)
         self.CA.signCertificate(self)
         assert os.path.isfile(self.CertFileName)

         # Provide chain file:
         shutil.copy2(self.CA.ChainFileName, self.ChainFileName)

         # Combine chain and key into a single file:
         shutil.copy2(self.CA.ChainFileName, self.ChainKeyFileName)
         chainKeyFile = open(self.ChainKeyFileName, 'a', encoding='utf-8')
         keyFile      = open(self.KeyFileName,  'r', encoding='utf-8')
         for line in keyFile:
            chainKeyFile.write(line)
         keyFile.close()
         chainKeyFile.close()

      self.verify()

      # ====== Verify certificate ===========================================
      # Print certificate:
      execute('openssl x509 '                  +
              ' -noout'                        +   # Do not dump the encoded certificate
              ' -subject -ext subjectAltName ' +   # Print basic information
              ' -in ' + self.CertFileName)


   # ###### Revoke certificate ##############################################
   def revoke(self):
      sys.stdout.write('\x1b[34mRevoking certificate ' + self.CertFileName + ' ...\x1b[0m\n')

      assert os.path.isfile(self.CertFileName)
      self.CA.revokeCertificate(self)
      if self.verify() == False:
         sys.stdout.write('Successfully revoked!\n')

      # Make sure invalid files are removed:
      for fileName in [ self.CSRFileName, self.CertFileName, self.ChainFileName ]:
         if os.path.exists(fileName):
            os.remove(fileName)


   # ###### Verify certificate ##############################################
   def verify(self):
      sys.stdout.write('\x1b[34mVerifying certificate ' + self.CertFileName + ' via ' +
                       self.ChainFileName + ' ...\x1b[0m\n')
      result = execute('openssl verify ' +
                       ' -show_chain' +
                       ' -verbose'    +
                       ' -crl_check'  +
                       ' -CAfile '    + self.ChainFileName +
                       ' -CRLfile '   + self.CA.CRLFileName +
                       ' ' + self.CertFileName, mayFail = True)
      return result == 0
