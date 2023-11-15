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
-- IMPORTANT NOTES:
-- 1. MySQL/MariaDB does not support unsigned BIGINT for the path hash.
--    The 64-bit value is stored as-is in a signed BIGINT, i.e.:
--    pathHash = (pathHash < 0) ?
--       0x10000000000000000 - abs(pathHash) : pathHash;
-- 2. MySQL/MariaDB does not support an INET datatype, just INET4 and INET6.
--    Addresses are stored as INET6, IPv4 addresses are handled as
--    IPv4-mapped IPv6 addesses (::ffff:a.b.c.d)!
-- 3. Enabling page compression (PAGE_COMPRESSED=1):
--    To check space saved:
--    SHOW GLOBAL STATUS LIKE 'Innodb_page_compression_saved';
-- ##########################################################################


-- ###### Ping ##############################################################
DROP TABLE IF EXISTS Ping;
CREATE TABLE Ping (
   SendTimestamp    INT8 UNSIGNED NOT NULL,              -- Send timestamp (nanoseconds since 1970-01-01, 00:00:00 UTC)
   MeasurementID    INT8 UNSIGNED NOT NULL DEFAULT 0,    -- MeasurementID
   SourceIP         INET6         NOT NULL,              -- Source IP address
   DestinationIP    INET6         NOT NULL,              -- Destination IP address
   Protocol         INT1 UNSIGNED NOT NULL DEFAULT 0,    -- Protocol (ICMP, UDP, ...)
   TrafficClass     INT1 UNSIGNED NOT NULL DEFAULT 0,    -- Traffic Class
   BurstSeq         INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Sequence number within a burst, numbered from 0.
   PacketSize       INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Packet size (bytes)
   ResponseSize     INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Response size (bytes; 0 if unknown)
   Checksum         INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Checksum
   SourcePort       INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Source port
   DestinationPort  INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Destination port
   Status           INT1 UNSIGNED NOT NULL,              -- Status

   TimeSource       INT4 UNSIGNED NOT NULL DEFAULT  0,   -- Source of the timing information (hexadecimal) as: AAQQSSHH
   Delay_AppSend    BIGINT        NOT NULL DEFAULT -1,   -- The measured application send delay (nanoseconds; -1 if not available)
   Delay_Queuing    BIGINT        NOT NULL DEFAULT -1,   -- The measured kernel software queuing delay (decimal; -1 if not available)
   Delay_AppReceive BIGINT        NOT NULL DEFAULT -1,   -- The measured application receive delay (nanoseconds; -1 if not available)
   RTT_App          BIGINT        NOT NULL,              -- The measured application RTT (nanoseconds)
   RTT_SW           BIGINT        NOT NULL DEFAULT -1,   -- The measured kernel software RTT (nanoseconds; -1 if not available)
   RTT_HW           BIGINT        NOT NULL DEFAULT -1,   -- The measured kernel hardware RTT (nanoseconds; -1 if not available)

   PRIMARY KEY (SendTimestamp, MeasurementID, SourceIP, DestinationIP, Protocol, TrafficClass, BurstSeq)
)
PAGE_COMPRESSED=1   -- Enable page compression!
PARTITION BY RANGE ( SendTimestamp ) (
   PARTITION p2022 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2023-01-01') ),
   PARTITION p2023 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2024-01-01') ),
   PARTITION p2024 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2025-01-01') ),
   PARTITION p2025 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2026-01-01') ),
   PARTITION p2026 VALUES LESS THAN MAXVALUE
);

CREATE INDEX PingRelationIndex ON Ping (MeasurementID ASC, DestinationIP ASC, SendTimestamp ASC);


-- ###### Traceroute ########################################################
DROP TABLE IF EXISTS Traceroute;
CREATE TABLE Traceroute (
   Timestamp        INT8 UNSIGNED NOT NULL,              -- Timestamp *for sorting* (nanoseconds since 1970-01-01, 00:00:00 UTC)
   MeasurementID    INT8 UNSIGNED NOT NULL DEFAULT 0,    -- MeasurementID
   SourceIP         INET6         NOT NULL,              -- Source IP address
   DestinationIP    INET6         NOT NULL,              -- Destination IP address
   Protocol         INT1 UNSIGNED NOT NULL DEFAULT 0,    -- Protocol (ICMP, UDP, ...)
   TrafficClass     INT1 UNSIGNED NOT NULL DEFAULT 0,    -- Traffic Class
   RoundNumber      INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Round number
   HopNumber        INT2 UNSIGNED NOT NULL,              -- Current hop number
   TotalHops        INT2 UNSIGNED NOT NULL,              -- Total number of hops
   PacketSize       INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Packet size (bytes)
   ResponseSize     INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Response size (bytes; 0 if unknown)
   Checksum         INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Checksum
   SourcePort       INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Source port
   DestinationPort  INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Destination port
   Status           INT2 UNSIGNED NOT NULL,              -- Status
   PathHash         BIGINT        NOT NULL,              -- Hash over full path
   SendTimestamp    INT8 UNSIGNED NOT NULL,              -- Send timestamp for hop (nanoseconds since 1970-01-01, 00:00:00 UTC)
   HopIP            INET6         NOT NULL,              -- Router or Destination IP address

   TimeSource       INT4 UNSIGNED NOT NULL DEFAULT  0,   -- Source of the timing information (hexadecimal) as: AAQQSSHH
   Delay_AppSend    BIGINT        NOT NULL DEFAULT -1,   -- The measured application send delay (nanoseconds; -1 if not available)
   Delay_Queuing    BIGINT        NOT NULL DEFAULT -1,   -- The measured kernel software queuing delay (decimal; -1 if not available)
   Delay_AppReceive BIGINT        NOT NULL DEFAULT -1,   -- The measured application receive delay (nanoseconds; -1 if not available)
   RTT_App          BIGINT        NOT NULL,              -- The measured application RTT (nanoseconds)
   RTT_SW           BIGINT        NOT NULL DEFAULT -1,   -- The measured kernel software RTT (nanoseconds; -1 if not available)
   RTT_HW           BIGINT        NOT NULL DEFAULT -1,   -- The measured kernel hardware RTT (nanoseconds; -1 if not available)

   PRIMARY KEY (Timestamp, MeasurementID, SourceIP, DestinationIP, Protocol, TrafficClass, RoundNumber, HopNumber)
)
PAGE_COMPRESSED=1   -- Enable page compression!
PARTITION BY RANGE ( Timestamp ) (
   PARTITION p2022 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2023-01-01') ),
   PARTITION p2023 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2024-01-01') ),
   PARTITION p2024 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2025-01-01') ),
   PARTITION p2025 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2026-01-01') ),
   PARTITION p2026 VALUES LESS THAN MAXVALUE
);

CREATE INDEX TracerouteRelationIndex ON Traceroute (MeasurementID ASC, DestinationIP ASC, Timestamp ASC);
