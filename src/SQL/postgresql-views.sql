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


-- ###### Ping version 1 view ###############################################
-- NOTE: This view is only for backwards compatibility, trying to provide the
--       same structure as old HiPerConTracer version 1 databases!
--       RTT uses the most accurate value available, i.e. HW -> SW -> App!
DROP VIEW IF EXISTS Ping_v1;
CREATE VIEW Ping_v1 AS
   SELECT
      -- Providing new BIGINT timestamp here, to speed up queries:
      SendTimestamp                            AS TimeStampNS,
      -- WARNING:  The original v1 time stamp is TIMESTAMP WITHOUT TIME ZONE
      --           This conversion prevents index usage, which
      --           is *very* slow on SELECTs with time ranges!
      UnixTimestamp2UTCDateTime(SendTimestamp) AS TimeStamp,
      SourceIP                                 AS FromIP,
      DestinationIP                            AS ToIP,
      Checksum                                 AS Checksum,
      PacketSize                               AS PktSize,
      TrafficClass                             AS TC,
      Status                                   AS Status,
      CASE
         WHEN RTT_HW > 0 THEN RTT_HW / 1000
         WHEN RTT_SW > 0 THEN RTT_SW / 1000
         ELSE                 RTT_App / 1000
      END                                      AS RTT
   FROM Ping;


-- ###### Ping version 2 view ###############################################
DROP VIEW IF EXISTS Ping_v2;
CREATE VIEW Ping_v2 AS
   SELECT * FROM Ping;


-- ###### Traceroute version 1 view #########################################
-- NOTE: This view is only for backwards compatibility, trying to provide the
--       same structure as old HiPerConTracer version 1 databases!
--       RTT uses the most accurate value available, i.e. HW -> SW -> App!
DROP VIEW IF EXISTS Traceroute_v1;
CREATE VIEW Traceroute_v1 AS
   SELECT
      -- Providing new BIGINT timestamp here, to speed up queries:
      Timestamp                            AS TimeStampNS,
      -- WARNING:  The original v1 time stamp is TIMESTAMP WITHOUT TIME ZONE
      --           This conversion prevents index usage, which
      --           is *very* slow on SELECTs with time ranges!
      UnixTimestamp2UTCDateTime(Timestamp) AS TimeStamp,
      SourceIP                             AS FromIP,
      DestinationIP                        AS ToIP,
      Checksum                             AS Checksum,
      PacketSize                           AS PktSize,
      TrafficClass                         AS TC,
      HopNumber                            AS HopNumber,
      TotalHops                            AS TotalHops,
      Status                               AS Status,
      CASE
         WHEN RTT_HW > 0 THEN RTT_HW / 1000
         WHEN RTT_SW > 0 THEN RTT_SW / 1000
         ELSE                 RTT_App / 1000
      END                                  AS RTT,
      HopIP,
      PathHash,
      RoundNumber                              AS Round
   FROM Traceroute;


-- ###### Traceroute version 2 view #########################################
DROP VIEW IF EXISTS Traceroute_v2;
CREATE VIEW Traceroute_v2 AS
   SELECT * FROM Traceroute;


-- ###### Jitter version 2 view #############################################
-- NOTE: There is no Jitter version 1!
DROP VIEW IF EXISTS Jitter_v2;
CREATE VIEW Jitter_v2 AS
   SELECT * FROM Jitter;
