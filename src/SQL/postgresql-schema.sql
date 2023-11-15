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
-- Copyright (C) 2015-2024 by Thomas Dreibholz
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
-- IMPORTANT NOTE:
-- 1. PostgreSQL does not support unsigned BIGINT for the path hash.
--    The 64-bit value is stored as-is in a signed BIGINT, i.e.:
--    pathHash = (pathHash < 0) ?
--       0x10000000000000000 - abs(pathHash) : pathHash;
-- 2. PostgreSQL does not support unsigned integers.
-- ##########################################################################


-- ###### Ping ##############################################################
DROP TABLE IF EXISTS Ping;
CREATE TABLE Ping (
   SendTimestamp    BIGINT      NOT NULL,              -- Send timestamp (nanoseconds since 1970-01-01, 00:00:00 UTC)
   MeasurementID    INTEGER     NOT NULL DEFAULT 0,    -- MeasurementID
   SourceIP         INET        NOT NULL,              -- Source IP address
   DestinationIP    INET        NOT NULL,              -- Destination IP address
   Protocol         SMALLINT    NOT NULL DEFAULT 0,    -- Protocol (ICMP, UDP, ...)
   TrafficClass     SMALLINT    NOT NULL DEFAULT 0,    -- Traffic Class
   BurstSeq         INTEGER     NOT NULL DEFAULT 0,    -- Sequence number within a burst, numbered from 0.
   PacketSize       INTEGER     NOT NULL DEFAULT 0,    -- Packet size (bytes)
   ResponseSize     INTEGER     NOT NULL DEFAULT 0,    -- Response size (bytes; 0 if unknown)
   Checksum         INTEGER     NOT NULL DEFAULT 0,    -- Checksum
   SourcePort       INTEGER     NOT NULL DEFAULT 0,    -- Source port
   DestinationPort  INTEGER     NOT NULL DEFAULT 0,    -- Destination port
   Status           SMALLINT    NOT NULL,              -- Status

   TimeSource       INTEGER     NOT NULL DEFAULT  0,   -- Source of the timing information (hexadecimal) as: AAQQSSHH
   Delay_AppSend    BIGINT      NOT NULL DEFAULT -1,   -- The measured application send delay (nanoseconds; -1 if not available)
   Delay_Queuing    BIGINT      NOT NULL DEFAULT -1,   -- The measured kernel software queuing delay (decimal; -1 if not available)
   Delay_AppReceive BIGINT      NOT NULL DEFAULT -1,   -- The measured application receive delay (nanoseconds; -1 if not available)
   RTT_App          BIGINT      NOT NULL,              -- The measured application RTT (nanoseconds)
   RTT_SW           BIGINT      NOT NULL DEFAULT -1,   -- The measured kernel software RTT (nanoseconds; -1 if not available)
   RTT_HW           BIGINT      NOT NULL DEFAULT -1,   -- The measured kernel hardware RTT (nanoseconds; -1 if not available)

   PRIMARY KEY (SendTimestamp, MeasurementID, SourceIP, DestinationIP, Protocol, TrafficClass, BurstSeq)
);

CREATE INDEX PingRelationIndex ON PingTracerouteDB.Ping (MeasurementID ASC, DestinationIP ASC, SendTimestamp ASC);


-- ###### Traceroute ########################################################
DROP TABLE IF EXISTS Traceroute;
CREATE TABLE Traceroute (
   Timestamp        BIGINT      NOT NULL,              -- Timestamp *for sorting* (nanoseconds since 1970-01-01, 00:00:00 UTC)
   MeasurementID    INTEGER     NOT NULL DEFAULT 0,    -- MeasurementID
   SourceIP         INET        NOT NULL,              -- Source IP address
   DestinationIP    INET        NOT NULL,              -- Destination IP address
   Protocol         SMALLINT    NOT NULL DEFAULT 0,    -- Protocol (ICMP, UDP, ...)
   TrafficClass     SMALLINT    NOT NULL DEFAULT 0,    -- Traffic Class
   RoundNumber      INTEGER     NOT NULL DEFAULT 0,    -- Round number
   HopNumber        SMALLINT    NOT NULL,              -- Current hop number
   TotalHops        SMALLINT    NOT NULL,              -- Total number of hops
   PacketSize       INTEGER     NOT NULL DEFAULT 0,    -- Packet size (bytes)
   ResponseSize     INTEGER     NOT NULL DEFAULT 0,    -- Response size (bytes; 0 if unknown)
   Checksum         INTEGER     NOT NULL DEFAULT 0,    -- Checksum
   SourcePort       INTEGER     NOT NULL DEFAULT 0,    -- Source port
   DestinationPort  INTEGER     NOT NULL DEFAULT 0,    -- Destination port
   Status           SMALLINT    NOT NULL,              -- Status
   PathHash         BIGINT      NOT NULL,              -- Hash over full path
   SendTimestamp    BIGINT      NOT NULL,              -- Send timestamp (nanoseconds since 1970-01-01, 00:00:00 UTC)
   HopIP            INET        NOT NULL,              -- Router or Destination IP address

   TimeSource       INTEGER     NOT NULL DEFAULT  0,   -- Source of the timing information (hexadecimal) as: AAQQSSHH
   Delay_AppSend    BIGINT      NOT NULL DEFAULT -1,   -- The measured application send delay (nanoseconds; -1 if not available)
   Delay_Queuing    BIGINT      NOT NULL DEFAULT -1,   -- The measured kernel software queuing delay (decimal; -1 if not available)
   Delay_AppReceive BIGINT      NOT NULL DEFAULT -1,   -- The measured application receive delay (nanoseconds; -1 if not available)
   RTT_App          BIGINT      NOT NULL,              -- The measured application RTT (nanoseconds)
   RTT_SW           BIGINT      NOT NULL DEFAULT -1,   -- The measured kernel software RTT (nanoseconds; -1 if not available)
   RTT_HW           BIGINT      NOT NULL DEFAULT -1,   -- The measured kernel hardware RTT (nanoseconds; -1 if not available)

   PRIMARY KEY (Timestamp, MeasurementID, SourceIP, DestinationIP, Protocol, TrafficClass, RoundNumber, HopNumber)
);

CREATE INDEX TracerouteRelationIndex ON PingTracerouteDB.Traceroute (MeasurementID ASC, DestinationIP ASC, Timestamp ASC);
