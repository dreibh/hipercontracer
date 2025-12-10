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

from enum import Enum

class HopStatus(Enum):
   # ====== Status byte ===================================
   Unknown                   = 0

   # ====== ICMP responses (from routers) =================
   # NOTE: Status values from 1 to 199 have a given
   #       router IP in their HopIP result!

   # ------ TTL/Hop Count ---------------------------------
   TimeExceeded              = 1    # ICMP response

   # ------ Reported as "unreachable" ---------------------
   # NOTE: Status values from 100 to 199 denote unreachability
   UnreachableScope          = 100  # ICMP response
   UnreachableNetwork        = 101  # ICMP response
   UnreachableHost           = 102  # ICMP response
   UnreachableProtocol       = 103  # ICMP response
   UnreachablePort           = 104  # ICMP response
   UnreachableProhibited     = 105  # ICMP response
   UnreachableUnknown        = 110  # ICMP response

   # ====== No response  ==================================
   # NOTE: Status values from 200 to 254 have the destination
   #       IP in their HopIP field. However there is no response!
   Timeout                   = 200

   NotSentGenericError       = 210  # sendto() call failed (generic error)
   NotSentPermissionDenied   = 211  # sendto() error: EACCES
   NotSentNetworkUnreachable = 212  # sendto() error: ENETUNREACH
   NotSentHostUnreachable    = 213  # sendto() error: EHOSTUNREACH
   NotAvailableAddress       = 214  # sendto() error: EADDRNOTAVAIL
   NotValidMsgSize           = 215  # sendto() error: EMSGSIZE
   NotEnoughBufferSpace      = 216  # sendto() error: ENOBUFS

   # ====== Destination's response (from destination) =====
   # NOTE: Successful response destination is in HopIP field.
   Success                   = 255  # Success!

   # ------ Response received -----------------------------
   Flag_StarredRoute         = (1 << 8)   # Route with * (router did not respond)
   Flag_DestinationReached   = (1 << 9)   # Destination has responded


# ###### Is destination not reachable? ######################################
def statusIsUnreachable(hopStatus : HopStatus) -> bool:
   # Values 100 to 199 => the destination cannot be reached any more, since
   # a router on the way reported unreachability.
   return ( (hopStatus.value >= HopStatus.UnreachableScope.value) and
            (hopStatus.value < HopStatus.Timeout.value) )
