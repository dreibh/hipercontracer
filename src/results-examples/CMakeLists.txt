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

FILE(GLOB_RECURSE EXAMPLE_RESULTS "*.hpct*" "*.results*")

# This does not work with the tarball package:
# EXECUTE_PROCESS(COMMAND         git ls-files .
#                 COMMAND         grep -E "\\.(hpct|results)\\..*$"
#                 OUTPUT_VARIABLE EXAMPLE_RESULTS)
# FILE(GLOB_RECURSE EXAMPLE_RESULTS "*.hpct*" "*.results*")
# MESSAGE("Example results files: ${EXAMPLE_RESULTS}")

INSTALL(FILES HiPerConTracer.R
              ${EXAMPLE_RESULTS}
              README.md
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hipercontracer/results-examples)
INSTALL(PROGRAMS r-install-dependencies
                 r-ping-example
                 r-traceroute-example
        DESTINATION ${CMAKE_INSTALL_DATADIR}/hipercontracer/results-examples)
