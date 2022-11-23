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
-- Copyright (C) 2015-2022 by Thomas Dreibholz
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


-- IMPORTANT NOTES:
-- 1. MySQL/MariaDB does not support unsigned BIGINT for the path hash.
--    The 64-bit value is stored as-is in a signed BIGINT, i.e.:
--    pathHash = (pathHash < 0) ?
--       0x10000000000000000 - abs(pathHash) : pathHash;
-- 2. MySQL/MariaDB does not support an INET datatype, just INET4 and INET6.
--    Addresses are stored as INET6, IPv4 addresses are handled as
--    IPv4-mapped IPv6 addesses (::ffff:a.b.c.d)!


-- ###### Ping ##############################################################
DROP TABLE IF EXISTS Ping;
CREATE TABLE Ping (
   TimeStamp DATETIME(6) NOT NULL,                   -- Time stamp (always UTC!)
   FromIP    INET6       NOT NULL,                   -- Source IP address
   ToIP      INET6       NOT NULL,                   -- Destination IP address
   Checksum  INTEGER     NOT NULL DEFAULT 0,         -- Checksum
   PktSize   INTEGER     NOT NULL DEFAULT 0,         -- Packet size
   TC        SMALLINT    NOT NULL DEFAULT 0,         -- Traffic Class
   Status    SMALLINT    NOT NULL,                   -- Status
   RTT       INTEGER     NOT NULL,                   -- microseconds (max. 2147s)
   PRIMARY KEY (TimeStamp, FromIP, ToIP, TC)
);

CREATE INDEX PingRelationIndex ON Ping (FromIP ASC, ToIP ASC, TimeStamp ASC);


-- ###### Traceroute ########################################################
DROP TABLE IF EXISTS Traceroute;
CREATE TABLE Traceroute (
   TimeStamp DATETIME(6) NOT NULL,                   -- Time stamp (always UTC!)
   FromIP    INET6       NOT NULL,                   -- Source IP address
   ToIP      INET6       NOT NULL,                   -- Destination IP address
   Round     INTEGER     NOT NULL DEFAULT 0,         -- Round number
   Checksum  INTEGER     NOT NULL DEFAULT 0,         -- Checksum
   PktSize   INTEGER     NOT NULL DEFAULT 0,         -- Packet size
   TC        SMALLINT    NOT NULL DEFAULT 0,         -- Traffic Class
   HopNumber SMALLINT    NOT NULL,                   -- Current hop number
   TotalHops SMALLINT    NOT NULL,                   -- Total number of hops
   Status    SMALLINT    NOT NULL,                   -- Status
   RTT       INTEGER     NOT NULL,                   -- microseconds (max. 2147s)
   HopIP     INET6       NOT NULL,                   -- Router or Destination IP address
   PathHash  BIGINT      NOT NULL,                   -- Hash over full path
   PRIMARY KEY (TimeStamp, FromIP, ToIP, TC, Round, HopNumber)
);

CREATE INDEX TracerouteRelationIndex ON Ping (FromIP ASC, ToIP ASC, TimeStamp ASC);
