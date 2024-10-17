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

#############################################################################
#### EXAMPLES                                                            ####
#############################################################################

INSTALL(FILES
           CertificateHelper.py
           README.md
           hpct-users.conf.example
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hipercontracer/TestDB)
INSTALL(PROGRAMS
           0-make-configurations
           1-install-database
           2-initialise-database
           3-test-database
           4-clean-database
           5-perform-hpct-importer-test
           6-perform-hpct-query-test
           9-uninstall-database
           generate-test-certificates
           name-in-etc-hosts
           run-full-test
           test-tls-connection
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hipercontracer/TestDB)
