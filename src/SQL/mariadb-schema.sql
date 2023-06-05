-- =================================================================
--          #     #                 #     #
--          ##    #   ####   #####  ##    #  ######   #####
--          # #   #  #    #  #    # # #   #  #          #
--          #  #  #  #    #  #    # #  #  #  #####      #
--          #   # #  #    #  #####  #   # #  #          #
--          #    ##  #    #  #   #  #    ##  #          #
--          #     #   ####   #    # #     #  ######     #
--
--       ---   The NorNet Testbed for Multi-Homed Systems  ---
--                       https://www.nntb.no
-- =================================================================
--
-- High-Performance Connectivity Tracer (HiPerConTracer)
-- Copyright (C) 2015-2023 by Thomas Dreibholz
--
-- This program is free software: you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/>.
--
-- Contact: dreibh@simula.no


-- ##########################################################################
-- IMPORTANT NOTES:
-- 1. MySQL/MariaDB does not support unsigned BIGINT for the path hash.
--    The 64-bit value is stored as-is in a signed BIGINT, i.e.:
--    pathHash = (pathHash < 0) ?
--       0x10000000000000000 - abs(pathHash) : pathHash;
-- 2. MySQL/MariaDB does not support an INET datatype, just INET4 and INET6.
--    Addresses are stored as INET6, IPv4 addresses are handled as
--    IPv4-mapped IPv6 addesses (::ffff:a.b.c.d)!
-- ##########################################################################


-- ###### Ping ##############################################################
DROP TABLE IF EXISTS Ping;
CREATE TABLE Ping (
   Timestamp        DATETIME(6) NOT NULL,                   -- Timestamp (always UTC!)
   MeasurementID    INTEGER     NOT NULL DEFAULT 0,         -- MeasurementID
   SourceIP         INET6       NOT NULL,                   -- Source IP address
   DestinationIP    INET6       NOT NULL,                   -- Destination IP address
   BurstSeq         INTEGER     NOT NULL DEFAULT 0,         -- Sequence number within a burst, numbered from 0.
   TrafficClass     SMALLINT    NOT NULL DEFAULT 0,         -- Traffic Class
   Protocol         SMALLINT    NOT NULL DEFAULT 0,         -- Protocol (ICMP, UDP, ...)
   PacketSize       INTEGER     NOT NULL DEFAULT 0,         -- Packet size (bytes)
   ResponseSize     INTEGER     NOT NULL DEFAULT 0,         -- Response size (bytes; 0 if unknown)
   Checksum         INTEGER     NOT NULL DEFAULT 0,         -- Checksum
   Status           SMALLINT    NOT NULL,                   -- Status

   TimeSource       INTEGER     NOT NULL DEFAULT 0,         -- Source of the timing information (hexadecimal) as: AAQQSSHH
   Delay.AppSend    BIGINT      NOT NULL,                   -- The measured application send delay (nanoseconds; -1 if not available).
   Delay.Queuing    BIGINT      NOT NULL,                   -- The measured kernel software queuing delay (decimal; -1 if not available).
   Delay.AppReceive BIGINT      NOT NULL,                   -- The measured application receive delay (nanoseconds; -1 if not available).
   RTT.App          BIGINT      NOT NULL,                   -- The measured application RTT (nanoseconds).
   RTT.SW           BIGINT      NOT NULL,                   -- The measured kernel software RTT (nanoseconds; -1 if not available).
   RTT.HW           BIGINT      NOT NULL,                   -- The measured kernel hardware RTT (nanoseconds; -1 if not available).

   PRIMARY KEY (Timestamp, MeasurementID, FromIP, ToIP, TC)
);

CREATE INDEX PingRelationIndex ON Ping (FromIP ASC, ToIP ASC, Timestamp ASC);


-- ###### Traceroute ########################################################
DROP TABLE IF EXISTS Traceroute;
CREATE TABLE Traceroute (
   Timestamp        DATETIME(6) NOT NULL,                   -- Timestamp (always UTC!)
   MeasurementID    INTEGER     NOT NULL DEFAULT 0,         -- MeasurementID
   SourceIP         INET6       NOT NULL,                   -- Source IP address
   DestinationIP    INET6       NOT NULL,                   -- Destination IP address
   Round            INTEGER     NOT NULL DEFAULT 0,         -- Round number
   TrafficClass     SMALLINT    NOT NULL DEFAULT 0,         -- Traffic Class
   Protocol         SMALLINT    NOT NULL DEFAULT 0,         -- Protocol (ICMP, UDP, ...)
   HopNumber        SMALLINT    NOT NULL,                   -- Current hop number
   TotalHops        SMALLINT    NOT NULL,                   -- Total number of hops
   PacketSize       INTEGER     NOT NULL DEFAULT 0,         -- Packet size (bytes)
   ResponseSize     INTEGER     NOT NULL DEFAULT 0,         -- Response size (bytes; 0 if unknown)
   Checksum         INTEGER     NOT NULL DEFAULT 0,         -- Checksum
   Status           SMALLINT    NOT NULL,                   -- Status
   PathHash         BIGINT      NOT NULL,                   -- Hash over full path

   SendTimestamp    DATETIME(6) NOT NULL,                   -- Send timestamp for hop (always UTC!)
   ResponseSize     INTEGER     NOT NULL DEFAULT 0,         -- Response size (bytes; 0 if unknown)
   HopIP            INET6       NOT NULL,                   -- Router or Destination IP address

   TimeSource       INTEGER     NOT NULL DEFAULT 0,         -- Source of the timing information (hexadecimal) as: AAQQSSHH
   Delay.AppSend    BIGINT      NOT NULL,                   -- The measured application send delay (nanoseconds; -1 if not available).
   Delay.Queuing    BIGINT      NOT NULL,                   -- The measured kernel software queuing delay (decimal; -1 if not available).
   Delay.AppReceive BIGINT      NOT NULL,                   -- The measured application receive delay (nanoseconds; -1 if not available).
   RTT.App          BIGINT      NOT NULL,                   -- The measured application RTT (nanoseconds).
   RTT.SW           BIGINT      NOT NULL,                   -- The measured kernel software RTT (nanoseconds; -1 if not available).
   RTT.HW           BIGINT      NOT NULL,                   -- The measured kernel hardware RTT (nanoseconds; -1 if not available).

   PRIMARY KEY (Timestamp, MeasurementID, FromIP, ToIP, TC, Round, HopNumber)
);

CREATE INDEX TracerouteRelationIndex ON Ping (FromIP ASC, ToIP ASC, Timestamp ASC);
