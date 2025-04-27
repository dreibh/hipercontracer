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
-- IMPORTANT NOTE:
-- 1. PostgreSQL does not support unsigned BIGINT for the path hash.
--    The 64-bit value is stored as-is in a signed BIGINT, i.e.:
--    pathHash = (pathHash < 0) ?
--       0x10000000000000000 - abs(pathHash) : pathHash;
-- 2. PostgreSQL does not support unsigned integers.
-- ##########################################################################


-- ###### Ping ##############################################################
DROP TABLE IF EXISTS Ping CASCADE;
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
) PARTITION BY RANGE (SendTimestamp);
CREATE TABLE Ping_p2021 PARTITION OF Ping FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2021-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2022-01-01'));
CREATE TABLE Ping_p2022 PARTITION OF Ping FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2022-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2023-01-01'));
CREATE TABLE Ping_p2023 PARTITION OF Ping FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2023-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2024-01-01'));
CREATE TABLE Ping_p2024 PARTITION OF Ping FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2024-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2025-01-01'));
CREATE TABLE Ping_p2025 PARTITION OF Ping FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2025-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2026-01-01'));
CREATE TABLE Ping_p2026 PARTITION OF Ping FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2026-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2027-01-01'));
CREATE TABLE Ping_p2027 PARTITION OF Ping FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2027-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2028-01-01'));

CREATE INDEX PingRelationIndex ON Ping (MeasurementID ASC, DestinationIP ASC, SendTimestamp ASC);


-- ###### Traceroute ########################################################
DROP TABLE IF EXISTS Traceroute CASCADE;
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
) PARTITION BY RANGE (Timestamp);
CREATE TABLE Traceroute_p2021 PARTITION OF Traceroute FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2021-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2022-01-01'));
CREATE TABLE Traceroute_p2022 PARTITION OF Traceroute FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2022-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2023-01-01'));
CREATE TABLE Traceroute_p2023 PARTITION OF Traceroute FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2023-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2024-01-01'));
CREATE TABLE Traceroute_p2024 PARTITION OF Traceroute FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2024-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2025-01-01'));
CREATE TABLE Traceroute_p2025 PARTITION OF Traceroute FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2025-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2026-01-01'));
CREATE TABLE Traceroute_p2026 PARTITION OF Traceroute FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2026-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2027-01-01'));
CREATE TABLE Traceroute_p2027 PARTITION OF Traceroute FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2027-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2028-01-01'));

CREATE INDEX TracerouteRelationIndex ON Traceroute (MeasurementID ASC, DestinationIP ASC, Timestamp ASC);


-- ###### Jitter ############################################################
DROP TABLE IF EXISTS Jitter CASCADE;
CREATE TABLE Jitter (
   Timestamp            BIGINT   NOT NULL,             -- Timestamp *for sorting* (nanoseconds since 1970-01-01, 00:00:00 UTC)
   MeasurementID        INTEGER  NOT NULL DEFAULT 0,   -- MeasurementID
   SourceIP             INET     NOT NULL,             -- Source IP address
   DestinationIP        INET     NOT NULL,             -- Destination IP address
   Protocol             SMALLINT NOT NULL DEFAULT 0,   -- Protocol (ICMP, UDP, ...)
   TrafficClass         SMALLINT NOT NULL DEFAULT 0,   -- Traffic Class
   RoundNumber          INTEGER  NOT NULL DEFAULT 0,   -- Round number
   PacketSize           INTEGER  NOT NULL DEFAULT 0,   -- Packet size (bytes)
   Checksum             INTEGER  NOT NULL DEFAULT 0,   -- Checksum
   SourcePort           INTEGER  NOT NULL DEFAULT 0,   -- Source port
   DestinationPort      INTEGER  NOT NULL DEFAULT 0,   -- Destination port
   Status               SMALLINT NOT NULL,             -- Status
   JitterType           SMALLINT NOT NULL DEFAULT 0,   -- Jitter type (0 for computed based on RFC 3550, Subsubsection 6.4.1)
   TimeSource           INTEGER  NOT NULL DEFAULT 0,   -- Source of the timing information (hexadecimal) as: AAQQSSHH

   Packets_AppSend      INTEGER  NOT NULL DEFAULT 0,   -- Number of packets for application send jitter/mean RTT computation
   MeanDelay_AppSend    BIGINT   NOT NULL DEFAULT -1,  -- Mean application send delay
   Jitter_AppSend       BIGINT   NOT NULL DEFAULT -1,  -- Jitter of application send delay

   Packets_Queuing      INTEGER  NOT NULL DEFAULT 0,   -- Number of packets for queuing delay jitter/mean RTT computation
   MeanDelay_Queuing    BIGINT   NOT NULL DEFAULT -1,  -- Mean application queuing delay
   Jitter_Queuing       BIGINT   NOT NULL DEFAULT -1,  -- Jitter of application queuing delay

   Packets_AppReceive   INTEGER  NOT NULL DEFAULT 0,   -- Number of packets for application receive jitter/mean RTT computation
   MeanDelay_AppReceive BIGINT   NOT NULL DEFAULT -1,  -- Mean application receive delay
   Jitter_AppReceive    BIGINT   NOT NULL DEFAULT -1,  -- Jitter of application receive delay

   Packets_App          INTEGER  NOT NULL,             -- Number of packets for application RTT jitter/mean RTT computation
   MeanRTT_App          BIGINT   NOT NULL,             -- Mean application RTT
   Jitter_App           BIGINT   NOT NULL,             -- Jitter of application receive delay

   Packets_SW           INTEGER  NOT NULL DEFAULT 0,   -- Number of packets for kernel software RTT jitter/mean RTT computation
   MeanRTT_SW           BIGINT   NOT NULL DEFAULT -1,  -- Mean kernel software RTT
   Jitter_SW            BIGINT   NOT NULL DEFAULT -1,  -- Jitter of kernel software RTT

   Packets_HW           INTEGER  NOT NULL DEFAULT 0,   -- Number of packets for kernel hardware RTT jitter/mean RTT computation
   MeanRTT_HW           BIGINT   NOT NULL DEFAULT -1,  -- Mean kernel hardware RTT
   Jitter_HW            BIGINT   NOT NULL DEFAULT -1,  -- Jitter of kernel hardware RTT

   PRIMARY KEY (Timestamp, MeasurementID, SourceIP, DestinationIP, Protocol, TrafficClass, RoundNumber)
) PARTITION BY RANGE (Timestamp);
CREATE TABLE Jitter_p2021 PARTITION OF Jitter FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2021-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2022-01-01'));
CREATE TABLE Jitter_p2022 PARTITION OF Jitter FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2022-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2023-01-01'));
CREATE TABLE Jitter_p2023 PARTITION OF Jitter FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2023-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2024-01-01'));
CREATE TABLE Jitter_p2024 PARTITION OF Jitter FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2024-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2025-01-01'));
CREATE TABLE Jitter_p2025 PARTITION OF Jitter FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2025-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2026-01-01'));
CREATE TABLE Jitter_p2026 PARTITION OF Jitter FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2026-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2027-01-01'));
CREATE TABLE Jitter_p2027 PARTITION OF Jitter FOR VALUES FROM (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2027-01-01')) TO (1000000000 * EXTRACT(EPOCH FROM TIMESTAMP '2028-01-01'));

CREATE INDEX JitterRelationIndex ON Jitter (MeasurementID ASC, DestinationIP ASC, Timestamp ASC);
