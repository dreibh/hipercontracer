#!/usr/bin/env python3
# ==========================================================================
#         ____            _                     _____           _
#        / ___| _   _ ___| |_ ___ _ __ ___     |_   _|__   ___ | |___
#        \___ \| | | / __| __/ _ \ '_ ` _ \ _____| |/ _ \ / _ \| / __|
#         ___) | |_| \__ \ ||  __/ | | | | |_____| | (_) | (_) | \__ \
#        |____/ \__, |___/\__\___|_| |_| |_|     |_|\___/ \___/|_|___/
#               |___/
#                             --- System-Tools ---
#                  https://www.nntb.no/~dreibh/system-tools/
# ==========================================================================
#
# X.509 CA and Certificate Helper Library
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
# Contact: thomas.dreibholz@gmail.com

import ipaddress
import netifaces
import os
import re
import shutil
import socket
import sys
from typing import Final
from enum   import Enum


# Certificate Types:
class CertificateType(Enum):
   RootCA          = 1
   IntermediateCA  = 2
   LeafCA          = 3
   Server          = 4
   Client          = 5
   User            = 6

# Some defaults:
DefaultCAKeyLength   : Final[int] = 16384
DefaultCertKeyLength : Final[int] = 16384

# ***** TEST ONLY *******************************
# These settings are for fast testing only:
# DefaultCAKeyLength   : Final[int] = 1024
# DefaultCertKeyLength : Final[int] = 1024
# ***********************************************

# Enable verbose logging for debugging here:
VerboseMode : bool = False



# ###### Execute command ####################################################
def execute(command : str, mayFail : bool = False) -> int:
   result = 1
   try:
      if VerboseMode:
         sys.stdout.write('\x1b[37m' + command + '\x1b[0m\n')
      result = os.system(command)
   except Exception as e:
      sys.stderr.write('FAILED COMMAND:\n' + command + '\n')
      sys.exit(1)
   if not mayFail:
      assert result == 0
   return result


# ###### Make "subjectAltName" string #######################################
RE_USEREMAIL : re.Pattern = \
   re.compile(r'^(.*)( <)([a-zA-Z0–9. _%+-]+@[a-zA-Z0–9. -]+\.[a-zA-Z]{2,})(>)$')
def prepareSubjectAltName(certType : CertificateType,
                          name     : str,
                          hint     : str | None) -> tuple[str,str]:

   # ====== Server or client cerfificate ====================================
   if (certType == CertificateType.Server) or (certType == CertificateType.Client):

      # ====== Prepare subjectAltName: current hostname+all addresses =======
      subjectAltName : str                      = 'DNS:' + name
      addresses      : set[ipaddress.ipaddress] = set()

      # ====== Get local addresses ==========================================
      if (hint == None) or (hint == 'LOCAL'):
         # ------ Add FQDN of current hostname ------------------------------
         fqdn : str = socket.getfqdn()
         if ((fqdn != name) and (fqdn != 'localhost')):
            subjectAltName = subjectAltName + ',DNS:' + fqdn

         # ------ Add all IP addresses --------------------------------------
         interfaces = netifaces.interfaces()
         for family in [ netifaces.AF_INET, netifaces.AF_INET6 ]:
            for interface in interfaces:
               interfaceAddresses = netifaces.ifaddresses(interface).get(family)
               if interfaceAddresses:
                  for interfaceAddress in interfaceAddresses:
                     address = ipaddress.ip_address(interfaceAddress['addr'])
                     if (not address.is_link_local) and (not address.is_loopback):
                        addresses.add(address)

      # ====== Look up addresses ============================================
      elif hint == 'LOOKUP':
         print('Resolving ' + name + ' ...\n')
         try:
            resolveResults = socket.getaddrinfo(name, 80)
         except socket.gaierror as e:
            sys.stderr.write('ERROR: Resolving of ' + name + ' failed: ' + str(e) + '\n')
            sys.exit(1)
         for resolveEntry in resolveResults:
            address = ipaddress.ip_address(resolveEntry[4][0])
            addresses.add(address)

      # ====== Append IP addresses to subjectAltName ========================
      addresses = sorted(addresses, key = lambda item: (item.version, int(item)))
      for address in addresses:
         subjectAltName = subjectAltName + ',IP:' + str(address)

      return ( name, subjectAltName )

   # ====== User certificate ================================================
   elif certType == CertificateType.User:

      match = RE_USEREMAIL.match(name)
      if match:
         name           = match.group(1)
         subjectAltName = 'email:' + match.group(3)
         return ( name, subjectAltName )

   return ( name, '' )


# ###### Make subject string for user #######################################
RE_TITLE : re.Pattern = re.compile(r'^([A-Z][a-z]+\.)$')
RE_EMAIL : re.Pattern = re.compile(r'^<[^<>]+@[^<>]+>$')
def prepareUserSubject(name : str) -> str:
   parts    : list[str] = name.split(' ')
   titles   : list[str] = [ ]
   names    : list[str] = [ ]
   initials : list[str] = [ ]
   email    : str       = ''
   for part in parts:
      if RE_TITLE.match(part):
         titles.append(part)
      elif RE_EMAIL.match(part):
          email = part
      else:
         names.append(part)
         initials.append(part[0] + '.')

   if (len(names) < 2) or (len(initials) < 2):
      sys.stderr.write(f'ERROR: Unsupported syntax for user: {name}\n')
      sys.exit(1)

   subject = ''
   if len(titles) > 0:
      subject += '/title=' + ' '.join(titles)
   subject += '/givenName=' + ' '.join(names[0:len(names) - 1])
   subject += '/initials='  + ' '.join(initials[0:len(initials) - 1])
   subject += '/surname='   + names[len(names) - 1]
   if email != '':
      subject += '/emailAddress=' + email

   print("==> " + subject)
   return subject


# ###### CA #################################################################
GlobalCRLSet             : set[os.PathLike] = set([])
DefaultGlobalCRLFileName : os.PathLike      = 'Test.crl'

class CA:

   # ###### Constructor #####################################################
   def __init__(self,
                mainDirectory     : Final[os.PathLike],
                name              : Final[str],
                parentCA          : 'CA',
                subject           : Final[str],
                certType          : Final[CertificateType],
                days              : Final[int]         = 10 * 365,
                keyLength         : Final[int]         = DefaultCAKeyLength,
                globalCRLFileName : Final[os.PathLike] = DefaultGlobalCRLFileName):

      safeName               : Final[str] = re.sub(r'[^a-zA-Z0-9+-\.]', '_', name)
      self.MainDirectory     : Final[os.PathLike]     = os.path.abspath(mainDirectory)
      self.Directory         : Final[os.PathLike]     = os.path.join(self.MainDirectory, safeName)
      self.GlobalCRLFileName : Final[os.PathLike]     = os.path.join(self.MainDirectory, globalCRLFileName)
      self.Subject           : Final[str]             = subject
      self.CAName            : Final[str]             = name
      self.CertType          : Final[CertificateType] = certType
      self.Extension         : str
      if self.CertType == CertificateType.RootCA:
         self.Extension = 'v3_ca'
      elif self.CertType == CertificateType.IntermediateCA:
         self.Extension = 'v3_intermediate_ca'
      elif self.CertType == CertificateType.LeafCA:
         self.Extension = 'v3_leaf_ca'
      else:
         raise Exception('Invalid certificate type')

      self.ParentCA          : 'CA'       = parentCA
      self.DefaultDays       : Final[int] = days
      self.KeyLength         : Final[int] = keyLength

      self.CertsDirectory    : Final[os.PathLike] = os.path.join(self.Directory, 'certs')
      self.NewCertsDirectory : Final[os.PathLike] = os.path.join(self.Directory, 'newcerts')
      self.CRLDirectory      : Final[os.PathLike] = os.path.join(self.Directory, 'crl')
      self.PrivateDirectory  : Final[os.PathLike] = os.path.join(self.Directory, 'private')

      self.IndexFileName     : Final[os.PathLike] = os.path.join(self.Directory, 'index.txt')
      self.SerialFileName    : Final[os.PathLike] = os.path.join(self.Directory, 'serial')
      self.CRLNumberFileName : Final[os.PathLike] = os.path.join(self.Directory, 'crlnumber')
      self.ConfigFileName    : Final[os.PathLike] = os.path.join(self.Directory, safeName + '.conf')

      self.KeyFileName       : Final[os.PathLike] = os.path.join(self.PrivateDirectory, safeName + '.key')
      self.PasswordFileName  : Final[os.PathLike] = os.path.join(self.PrivateDirectory, safeName + '.password')
      self.CertFileName      : Final[os.PathLike] = os.path.join(self.CertsDirectory,   safeName + '.crt')
      self.CRLFileName       : Final[os.PathLike] = os.path.join(self.CRLDirectory,     safeName + '.crl')

      if VerboseMode or not os.path.isfile(self.CertFileName):
         sys.stdout.write('\x1b[34mCreating CA ' + name + ' ...\x1b[0m\n')

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
      if not os.path.isfile(self.IndexFileName):
         indexFile = open(self.IndexFileName, 'w', encoding='ascii')
         indexFile.close()

      if not os.path.isfile(self.SerialFileName):
         serialFile = open(self.SerialFileName, 'w', encoding='ascii')
         serialFile.write('00')
         serialFile.close()

      if not os.path.isfile(self.CRLNumberFileName):
         crlnumberFile = open(self.CRLNumberFileName, 'w', encoding='ascii')
         crlnumberFile.write('00')
         crlnumberFile.close()

      # ====== Generate CA configuration file ===============================
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
countryName                     = optional
stateOrProvinceName             = optional
localityName                    = optional
organizationName                = optional
organizationalUnitName          = optional
streetAddress                   = optional
postalCode                      = optional
title                           = optional
surname                         = optional
givenName                       = optional
initials                        = optional
emailAddress                    = optional
commonName                      = supplied

# ====== PKCS#10 certificate request and certificate settings ===============
[ req ]
# See `man req` for details!
default_bits                    = 4096
default_md                      = sha512
distinguished_name              = req_distinguished_name
utf8                            = yes
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
stateOrProvinceName_default     = Oslo
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
keyUsage               = critical, cRLSign, keyCertSign

# ====== Settings for intermediate CA =======================================
[ v3_intermediate_ca ]
# See `man x509v3_config` for details!
subjectKeyIdentifier   = hash
authorityKeyIdentifier = keyid:always,issuer
basicConstraints       = critical, CA:true              # <<-- CA certificate
keyUsage               = critical, cRLSign, keyCertSign

# ====== Settings for a leaf CA =============================================
[ v3_leaf_ca ]
# See `man x509v3_config` for details!
subjectKeyIdentifier   = hash
authorityKeyIdentifier = keyid:always,issuer
basicConstraints       = critical, CA:true, pathlen:0   # <<-- CA, but no sub-CAs
keyUsage               = critical, cRLSign, keyCertSign

# ====== Settings for a server certificate ==================================
[ server_cert ]
# Extensions for server certificates (`man x509v3_config`).
basicConstraints       = CA:FALSE
subjectKeyIdentifier   = hash
keyUsage               = critical, digitalSignature, keyEncipherment
extendedKeyUsage       = critical, serverAuth
subjectAltName         = ${ENV::SAN}

# ====== Settings for a server certificate ==================================
[ client_cert ]
# Extensions for server certificates (`man x509v3_config`).
basicConstraints       = CA:FALSE
subjectKeyIdentifier   = hash
keyUsage               = critical, digitalSignature, keyEncipherment
extendedKeyUsage       = critical, clientAuth
subjectAltName         = ${ENV::SAN}

# ====== Settings for a user certificate ====================================
[ user_cert ]
# Extensions for client certificates (`man x509v3_config`).
basicConstraints       = CA:FALSE
subjectKeyIdentifier   = hash
keyUsage               = critical, nonRepudiation, digitalSignature, keyEncipherment
extendedKeyUsage       = critical, clientAuth, emailProtection, codeSigning
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
         for fileName in [ self.CertFileName, self.CRLFileName ]:
            if os.path.exists(fileName):
               os.remove(fileName)


      # ====== Generate self-signed root CA certificate =====================
      self.RootCA : 'CA' = None
      if parentCA == None:
         # ------ Set reference to root CA (i.e. to itself) -----------------
         self.RootCA = self

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


      # ====== Generate CA certificate signed by parent CA ==================
      else:
         # ------ Set reference to root CA ----------------------------------
         if self.ParentCA == None:
            self.RootCA = self
         else:
            self.RootCA = self.ParentCA
            while self.RootCA.ParentCA != None:
               self.RootCA = self.RootCA.ParentCA

         if not os.path.isfile(self.CertFileName):
            # ------ Generate CSR -------------------------------------------
            csrFileName : Final[os.PathLike] = self.CertFileName + '.csr'
            sys.stdout.write('\x1b[33mGenerating CSR ' + csrFileName + ' ...\x1b[0m\n')
            execute('SAN="" openssl req' +
                  ' -new'              +   # Not self-signed
                  ' -config '          + self.ConfigFileName     +
                  ' -extensions v3_ca' +
                  ' -utf8 -subj "'     + str(self.Subject) + '"' +
                  ' -key '             + self.KeyFileName        +
                  ' -passin file:'     + self.PasswordFileName   +
                  ' -out '             + csrFileName)
            assert os.path.isfile(csrFileName)

            # ------ Get CSR signed by parent CA ----------------------------
            sys.stdout.write('\x1b[33mGetting CSR ' + csrFileName + ' signed by ' + parentCA.CAName + ' ...\x1b[0m\n')

            tmpCertFileName = self.CertFileName + '.tmp'
            execute('SAN="" openssl ca' +
                    ' -batch'            +
                    ' -notext'           +
                    ' -config '          + parentCA.ConfigFileName    +
                    ' -extensions '      + self.Extension             +
                    ' -utf8 -subj "'     + str(self.Subject) + '"'    +
                    ' -days '            + str(parentCA.DefaultDays)  +
                    ' -passin file:'     + parentCA.PasswordFileName  +
                    ' -in '              + csrFileName           +
                    ' -out '             + tmpCertFileName)
            assert os.path.isfile(tmpCertFileName)

            # ------ Add the whole certificate chain ------------------------
            certFile  = open(tmpCertFileName, 'a', encoding='ascii')
            chainFile = open(self.ParentCA.CertFileName, 'r', encoding='ascii')
            for line in chainFile:
               certFile.write(line)
            os.rename(tmpCertFileName, self.CertFileName)

            # ------ Remove CSR ---------------------------------------------
            if os.path.exists(csrFileName):
               os.remove(csrFileName)


            # Check root CA -> ... -> certificate
            # NOTE: Using option "-untrusted" to mark the whole chain as untrusted
            #       works! The CAfile is always trusted, and OpenSSL will verify
            #       all certificates of the chain.
            #       -> https://stackoverflow.com/questions/25482199/verify-a-certificate-chain-using-openssl-verify
            cmd = 'openssl verify ' + \
                  ' -show_chain'    + \
                  ' -verbose'       + \
                  ' -CAfile '       + self.RootCA.CertFileName
            if self.ParentCA:
               cmd += ' -untrusted ' + self.ParentCA.CertFileName
            cmd +=' ' + self.CertFileName
            execute(cmd)


      # ====== Generate initial CRL =========================================
      GlobalCRLSet.add(self.CRLFileName)
      self.generateCRL()


      # ====== Print certificate ============================================
      if VerboseMode:
         execute('openssl x509 '                  +
               ' -noout'                        +   # Do not dump the encoded certificate
               ' -subject -ext subjectAltName ' +   # Print basic information
               ' -in ' + self.CertFileName)


   # ###### Sign certificate ################################################
   def signCertificate(self,
                       certificate : 'Certificate',
                       csrFileName : os.PathLike) -> None:

      # ------ Sign CSR -----------------------------------------------------
      sys.stdout.write('\x1b[33mGetting CSR ' + csrFileName + ' signed by ' + self.CAName + ' ...\x1b[0m\n')
      assert os.path.isfile(csrFileName)

      tmpCertFileName = certificate.CertFileName + '.tmp'
      execute('SAN="' + certificate.SubjectAltName + '" openssl ca' +
              ' -batch'            +
              ' -notext'           +
              ' -config '          + self.ConfigFileName            +
              ' -extensions '      + certificate.Extension          +
              ' -utf8 -subj "'     + str(certificate.Subject) + '"' +
              ' -days '            + str(self.DefaultDays)          +
              ' -passin file:'     + self.PasswordFileName          +
              ' -in '              + csrFileName                    +
              ' -out '             + tmpCertFileName)
      assert os.path.isfile(tmpCertFileName)

      # ------ Add the whole chain ------------------------------------------
      certFile  = open(tmpCertFileName, 'a', encoding='ascii')
      chainFile = open(self.CertFileName, 'r', encoding='ascii')
      for line in chainFile:
         certFile.write(line)
      os.rename(tmpCertFileName, certificate.CertFileName)


   # ###### Verify certificate ##############################################
   def verifyCertificate(self, certificate : 'Certificate') -> int:
      sys.stdout.write('\x1b[33mVerifying certificate ' + certificate.CertFileName + ' ...\x1b[0m\n')
      result = execute('openssl verify ' +
                       ' -show_chain' +
                       ' -verbose'    +
                       ' -crl_check'  +
                       ' -CAfile '    + self.RootCA.CertFileName +
                       ' -untrusted ' + self.CertFileName +
                       ' -CRLfile '   + self.CRLFileName +
                       ' ' + certificate.CertFileName, mayFail = True)
      return result == 0


   # ###### Revoke certificate ##############################################
   def revokeCertificate(self, certificate : 'Certificate') -> None:
      sys.stdout.write('\x1b[33mRevoking certificate ' + certificate.CertFileName + ' ...\x1b[0m\n')
      assert os.path.isfile(self.CertFileName)
      execute('SAN="" openssl ca' +
              ' -revoke '      + certificate.CertFileName  +
              ' -config '      + certificate.CA.ConfigFileName   +
              ' -passin file:' + certificate.CA.PasswordFileName)
      self.generateCRL()


   # ###### Generate CRL ####################################################
   def generateCRL(self) -> None:
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
   def generateGlobalCRL(self, globalCRLFileName : os.PathLike) -> None:
      if len(GlobalCRLSet) > 0:
         cmd = 'cat'
         for crlFileName in sorted(GlobalCRLSet):
            cmd = cmd + ' ' + crlFileName
         cmd = cmd + ' >' + globalCRLFileName
         execute(cmd)
         assert(os.path.isfile(globalCRLFileName))


# ###### Server/Client/User Certificate #####################################
class Certificate:

   # ###### Constructor #####################################################
   def __init__(self,
                mainDirectory    : Final[os.PathLike],
                name             : Final[str],
                ca               : CA,
                subjectWithoutCN : Final[str],
                subjectAltName   : Final[str],
                certType         : Final[CertificateType] = CertificateType.Server,
                keyLength        : Final[int]             = DefaultCertKeyLength,
                revokeIfExisting : Final[bool]            = False):

      sys.stdout.write('\x1b[34mCreating certificate ' + name + ' ...\x1b[0m\n')

      safeName            : Final[str] = re.sub(r'[^a-zA-Z0-9+-\.]', '_', name)
      self.CA             : CA                     = ca
      self.Directory      : Final[os.PathLike]     = os.path.join(os.path.abspath(mainDirectory), safeName)
      self.Subject        : Final[str]             = subjectWithoutCN + '/CN=' + name
      self.SubjectAltName : Final[str]             = subjectAltName
      self.CertType       : Final[CertificateType] = certType
      self.Extension      : str
      if self.CertType == CertificateType.Server:
         self.Extension = 'server_cert'
      elif self.CertType == CertificateType.Client:
         self.Extension = 'client_cert'
      elif self.CertType == CertificateType.User:
         self.Extension = 'user_cert'
      else:
         raise Exception('Invalid certificate type')


      self.KeyLength    : Final[os.PathLike] = keyLength
      self.KeyFileName  : Final[os.PathLike] = os.path.join(self.Directory, safeName + '.key')
      self.CertFileName : Final[os.PathLike] = os.path.join(self.Directory, safeName + '.crt')

      os.makedirs(self.Directory, exist_ok = True)

      # ====== Revoke existing certificate first ============================
      if revokeIfExisting:
         if os.path.isfile(self.CertFileName):
            self.revoke()

      # ====== Generate key =================================================
      if not os.path.isfile(self.KeyFileName):
         sys.stdout.write('\x1b[33mGenerating key ' + self.KeyFileName + ' ...\x1b[0m\n')
         execute('openssl genrsa'  +
                 ' -out '          + os.path.join(self.Directory, self.KeyFileName) +
                 ' ' + str(self.KeyLength))
         assert os.path.isfile(self.KeyFileName)

         #  Make sure that an invalid cerfificate file is removed:
         if os.path.exists(self.CertFileName):
            os.remove(self.CertFileName)

      # ====== Generate certificate signed by CA ============================
      if not os.path.isfile(self.CertFileName):
         # ------ Generate CSR ----------------------------------------------
         csrFileName : Final[os.PathLike] = os.path.join(self.Directory, safeName + '.csr')
         sys.stdout.write('\x1b[33mGenerating CSR ' + csrFileName + ' ...\x1b[0m\n')
         execute('SAN="' + self.SubjectAltName + '" openssl req' +
                 ' -new'          +   # Not self-signed
                 ' -config '      + self.CA.ConfigFileName   +
                 ' -extensions '  + self.Extension           +
                 ' -utf8 -subj "' + str(self.Subject) + '"'  +
                 ' -key '         + self.KeyFileName         +
                 ' -out '         + csrFileName)
         assert os.path.isfile(csrFileName)

         # ------ Get CSR signed by CA --------------------------------------
         assert os.path.isfile(csrFileName)
         self.CA.signCertificate(self, csrFileName)
         assert os.path.isfile(self.CertFileName)

         # ------ Remove CSR ------------------------------------------------
         if os.path.exists(csrFileName):
            os.remove(csrFileName)

         assert self.verify()

      # ====== Verify certificate ===========================================
      # Print certificate:
      execute('openssl x509 '                  +
              ' -noout'                        +   # Do not dump the encoded certificate
              ' -subject -ext subjectAltName ' +   # Print basic information
              ' -in ' + self.CertFileName)


   # ###### Revoke certificate ##############################################
   def revoke(self) -> None:
      sys.stdout.write('\x1b[34mRevoking certificate ' + self.CertFileName + ' ...\x1b[0m\n')

      assert os.path.isfile(self.CertFileName)
      self.CA.revokeCertificate(self)
      if self.verify() == False:
         sys.stdout.write('Successfully revoked!\n')

      # Remove the now-invalid certificate file:
      if os.path.exists(self.CertFileName):
         os.remove(self.CertFileName)


   # ###### Verify certificate ##############################################
   def verify(self) -> bool:

      return self.CA.verifyCertificate(self)
