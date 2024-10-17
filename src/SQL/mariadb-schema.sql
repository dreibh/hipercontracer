-- ==========================================================================
--     _   _ _ ____            ____          _____
--    | | | (_)  _ \ ___ _ __ / ___|___  _ _|_   _| __ __ _  ___ ___ _ __
--    | |_| | | |_) / _ \ '__| |   / _ \| '_ \| || '__/ _` |/ __/ _ \ '__|
--    |  _  | |  __/  __/ |  | |__| (_) | | | | || | | (_| | (_|  __/ |
--    |_| |_|_|_|   \___|_|   \____\___/|_| |_|_||_|  \__,_|\___\___|_|
--
--       ---  High-Performance Connectivity Tracer (HiPerConTracer)  ---
--                 https://www.nntb.no/~dreibh/hipercontracer/
-- ==========================================================================
--
-- High-Performance Connectivity Tracer (HiPerConTracer)
-- Copyright (C) 2015-2025 by Thomas Dreibholz
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
-- 1. MySQL/MariaDB does not support unsigned INT8   for the path hash.
--    The 64-bit value is stored as-is in a signed INT8  , i.e.:
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
   Delay_AppSend    INT8          NOT NULL DEFAULT -1,   -- The measured application send delay (nanoseconds; -1 if not available)
   Delay_Queuing    INT8          NOT NULL DEFAULT -1,   -- The measured kernel software queuing delay (decimal; -1 if not available)
   Delay_AppReceive INT8          NOT NULL DEFAULT -1,   -- The measured application receive delay (nanoseconds; -1 if not available)
   RTT_App          INT8          NOT NULL,              -- The measured application RTT (nanoseconds)
   RTT_SW           INT8          NOT NULL DEFAULT -1,   -- The measured kernel software RTT (nanoseconds; -1 if not available)
   RTT_HW           INT8          NOT NULL DEFAULT -1,   -- The measured kernel hardware RTT (nanoseconds; -1 if not available)

   PRIMARY KEY (SendTimestamp, MeasurementID, SourceIP, DestinationIP, Protocol, TrafficClass, BurstSeq)
)
PAGE_COMPRESSED=1   -- Enable page compression!
PARTITION BY RANGE ( SendTimestamp ) (
   PARTITION p2021 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2022-01-01') ),
   PARTITION p2022 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2023-01-01') ),
   PARTITION p2023 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2024-01-01') ),
   PARTITION p2024 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2025-01-01') ),
   PARTITION p2025 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2026-01-01') ),
   PARTITION p2026 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2027-01-01') ),
   PARTITION p2027 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2028-01-01') ),
   PARTITION p2028 VALUES LESS THAN MAXVALUE
);

CREATE INDEX PingRelationIndex ON Ping (MeasurementID ASC, DestinationIP ASC, SendTimestamp ASC);


-- ###### Ping version 1 view ###############################################
-- NOTE: This view is only for backwards compatibility, trying to provide the
--       same structure as old HiPerConTracer version 1 databases!
--       RTT uses the most accurate value available, i.e. HW -> SW -> App!
DROP VIEW IF EXISTS Ping_v1;
CREATE VIEW Ping_v1 AS
   SELECT
      CAST(FROM_UNIXTIME(SendTimestamp / 1000000000) AS DATETIME(6)) AS TimeStamp,
      SourceIP                                  AS FromIP,
      DestinationIP                             AS ToIP,
      PacketSize                                AS PktSize,
      TrafficClass                              AS TC,
      Status                                    AS Status,
      CAST(IF(RTT_HW > 0, RTT_HW / 1000,
              IF(RTT_SW > 0, RTT_SW / 1000,
                 RTT_App / 1000)) AS INTEGER)   AS RTT
   FROM Ping;


-- ###### Ping version 2 view ###############################################
DROP VIEW IF EXISTS Ping_v2;
CREATE VIEW Ping_v2 AS
   SELECT * FROM Ping;


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
   PathHash         INT8          NOT NULL,              -- Hash over full path
   SendTimestamp    INT8 UNSIGNED NOT NULL,              -- Send timestamp for hop (nanoseconds since 1970-01-01, 00:00:00 UTC)
   HopIP            INET6         NOT NULL,              -- Router or Destination IP address

   TimeSource       INT4 UNSIGNED NOT NULL DEFAULT  0,   -- Source of the timing information (hexadecimal) as: AAQQSSHH
   Delay_AppSend    INT8          NOT NULL DEFAULT -1,   -- The measured application send delay (nanoseconds; -1 if not available)
   Delay_Queuing    INT8          NOT NULL DEFAULT -1,   -- The measured kernel software queuing delay (decimal; -1 if not available)
   Delay_AppReceive INT8          NOT NULL DEFAULT -1,   -- The measured application receive delay (nanoseconds; -1 if not available)
   RTT_App          INT8          NOT NULL,              -- The measured application RTT (nanoseconds)
   RTT_SW           INT8          NOT NULL DEFAULT -1,   -- The measured kernel software RTT (nanoseconds; -1 if not available)
   RTT_HW           INT8          NOT NULL DEFAULT -1,   -- The measured kernel hardware RTT (nanoseconds; -1 if not available)

   PRIMARY KEY (Timestamp, MeasurementID, SourceIP, DestinationIP, Protocol, TrafficClass, RoundNumber, HopNumber)
)
PAGE_COMPRESSED=1   -- Enable page compression!
PARTITION BY RANGE ( Timestamp ) (
   PARTITION p2021 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2022-01-01') ),
   PARTITION p2022 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2023-01-01') ),
   PARTITION p2023 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2024-01-01') ),
   PARTITION p2024 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2025-01-01') ),
   PARTITION p2025 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2026-01-01') ),
   PARTITION p2026 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2027-01-01') ),
   PARTITION p2027 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2028-01-01') ),
   PARTITION p2028 VALUES LESS THAN MAXVALUE
);

CREATE INDEX TracerouteRelationIndex ON Traceroute (MeasurementID ASC, DestinationIP ASC, Timestamp ASC);


-- ###### Traceroute version 1 view #########################################
-- NOTE: This view is only for backwards compatibility, trying to provide the
--       same structure as old HiPerConTracer version 1 databases!
--       RTT uses the most accurate value available, i.e. HW -> SW -> App!
DROP VIEW IF EXISTS Traceroute_v1;
CREATE VIEW Traceroute_v1 AS
   SELECT
      CAST(FROM_UNIXTIME(Timestamp / 1000000000) AS DATETIME(6)) AS TimeStamp,
      SourceIP                                  AS FromIP,
      DestinationIP                             AS ToIP,
      PacketSize                                AS PktSize,
      TrafficClass                              AS TC,
      HopNumber                                 AS HopNumber,
      TotalHops                                 AS TotalHops,
      Status                                    AS Status,
      CAST(IF(RTT_HW > 0, RTT_HW / 1000,
              IF(RTT_SW > 0, RTT_SW / 1000,
                 RTT_App / 1000)) AS INTEGER)   AS RTT,
      HopIP                                     AS HopIP,
      PathHash                                  AS PathHash,
      RoundNumber                               AS Round
   FROM Traceroute;


-- ###### Traceroute version 2 view #########################################
DROP VIEW IF EXISTS Traceroute_v2;
CREATE VIEW Traceroute_v2 AS
   SELECT * FROM Traceroute;


-- ###### Jitter ############################################################
DROP TABLE IF EXISTS Jitter;
CREATE TABLE Jitter (
   Timestamp            INT8 UNSIGNED NOT NULL,              -- Send timestamp (nanoseconds since 1970-01-01, 00:00:00 UTC)
   MeasurementID        INT8 UNSIGNED NOT NULL DEFAULT 0,    -- MeasurementID
   SourceIP             INET6         NOT NULL,              -- Source IP address
   DestinationIP        INET6         NOT NULL,              -- Destination IP address
   Protocol             INT1 UNSIGNED NOT NULL DEFAULT 0,    -- Protocol (ICMP, UDP, ...)
   TrafficClass         INT1 UNSIGNED NOT NULL DEFAULT 0,    -- Traffic Class
   RoundNumber          INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Round number
   PacketSize           INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Packet size (bytes)
   Checksum             INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Checksum
   SourcePort           INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Source port
   DestinationPort      INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Destination port
   Status               INT1 UNSIGNED NOT NULL,              -- Status
   JitterType           INT1 UNSIGNED NOT NULL DEFAULT 0,    -- Jitter type (0 for computed based on RFC 3550, Subsubsection 6.4.1)
   TimeSource           INT4 UNSIGNED NOT NULL DEFAULT 0,    -- Source of the timing information (hexadecimal) as: AAQQSSHH

   Packets_AppSend      INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Number of packets for application send jitter/mean RTT computation
   MeanDelay_AppSend    INT8          NOT NULL DEFAULT -1,   -- Mean application send delay
   Jitter_AppSend       INT8          NOT NULL DEFAULT -1,   -- Jitter of application send delay

   Packets_Queuing      INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Number of packets for queuing delay jitter/mean RTT computation
   MeanDelay_Queuing    INT8          NOT NULL DEFAULT -1,   -- Mean application queuing delay
   Jitter_Queuing       INT8          NOT NULL DEFAULT -1,   -- Jitter of application queuing delay

   Packets_AppReceive   INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Number of packets for application receive jitter/mean RTT computation
   MeanDelay_AppReceive INT8          NOT NULL DEFAULT -1,   -- Mean application receive delay
   Jitter_AppReceive    INT8          NOT NULL DEFAULT -1,   -- Jitter of application receive delay

   Packets_App          INT2 UNSIGNED NOT NULL,              -- Number of packets for application RTT jitter/mean RTT computation
   MeanRTT_App          INT8          NOT NULL,              -- Mean application RTT
   Jitter_App           INT8          NOT NULL,              -- Jitter of application receive delay

   Packets_SW           INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Number of packets for kernel software RTT jitter/mean RTT computation
   MeanRTT_SW           INT8          NOT NULL DEFAULT -1,   -- Mean kernel software RTT
   Jitter_SW            INT8          NOT NULL DEFAULT -1,   -- Jitter of kernel software RTT

   Packets_HW           INT2 UNSIGNED NOT NULL DEFAULT 0,    -- Number of packets for kernel hardware RTT jitter/mean RTT computation
   MeanRTT_HW           INT8          NOT NULL DEFAULT -1,   -- Mean kernel hardware RTT
   Jitter_HW            INT8          NOT NULL DEFAULT -1,   -- Jitter of kernel hardware RTT

   PRIMARY KEY (Timestamp, MeasurementID, SourceIP, DestinationIP, Protocol, TrafficClass, RoundNumber)
)
PAGE_COMPRESSED=1   -- Enable page compression!
PARTITION BY RANGE ( Timestamp ) (
   PARTITION p2021 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2022-01-01') ),
   PARTITION p2022 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2023-01-01') ),
   PARTITION p2023 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2024-01-01') ),
   PARTITION p2024 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2025-01-01') ),
   PARTITION p2025 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2026-01-01') ),
   PARTITION p2026 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2027-01-01') ),
   PARTITION p2027 VALUES LESS THAN ( 1000000000 * UNIX_TIMESTAMP('2028-01-01') ),
   PARTITION p2028 VALUES LESS THAN MAXVALUE
);

CREATE INDEX JitterRelationIndex ON Jitter (MeasurementID ASC, DestinationIP ASC, Timestamp ASC);


-- ###### Jitter version 2 view #############################################
-- NOTE: There is no Jitter version 1!
DROP VIEW IF EXISTS Jitter_v2;
CREATE VIEW Jitter_v2 AS
   SELECT * FROM Jitter;
