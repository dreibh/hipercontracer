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


-- ###### Ping ##############################################################
DROP TABLE IF EXISTS Ping;
CREATE TABLE Ping (
   TimeStamp TIMESTAMP WITHOUT TIME ZONE NOT NULL,   -- Time stamp (always UTC!)
   FromIP    INET        NOT NULL,                   -- Source IP address
   ToIP      INET        NOT NULL,                   -- Destination IP address
   Checksum  INTEGER     NOT NULL DEFAULT 0,         -- Checksum
   PktSize   INTEGER     NOT NULL DEFAULT 0,         -- Packet size
   TC        SMALLINT    NOT NULL DEFAULT 0,         -- Traffic Class
   Status    SMALLINT    NOT NULL,                   -- Status
   RTT       INTEGER     NOT NULL,                   -- microseconds (max. 2147s)
   PRIMARY KEY (TimeStamp, FromIP, ToIP, TC)
);

CREATE INDEX PingRelationIndex ON PingTracerouteDB.Ping (FromIP ASC, ToIP ASC);


-- ###### Traceroute ########################################################
DROP TABLE IF EXISTS Traceroute;
CREATE TABLE Traceroute (
   TimeStamp TIMESTAMP WITHOUT TIME ZONE NOT NULL,   -- Time stamp (always UTC!)
   FromIP    INET        NOT NULL,                   -- Source IP address
   ToIP      INET        NOT NULL,                   -- Destination IP address
   Round     INTEGER     NOT NULL DEFAULT 0,         -- Round number
   Checksum  INTEGER     NOT NULL DEFAULT 0,         -- Checksum
   PktSize   INTEGER     NOT NULL DEFAULT 0,         -- Packet size
   TC        SMALLINT    NOT NULL DEFAULT 0,         -- Traffic Class
   HopNumber SMALLINT    NOT NULL,                   -- Current hop number
   TotalHops SMALLINT    NOT NULL,                   -- Total number of hops
   Status    SMALLINT    NOT NULL,                   -- Status
   RTT       INTEGER     NOT NULL,                   -- microseconds (max. 2147s)
   HopIP     INET        NOT NULL,                   -- Router or Destination IP address
   PathHash  BIGINT      NOT NULL,                   -- Hash over full path
   PRIMARY KEY (TimeStamp, FromIP, ToIP, TC, Round, HopNumber)
);

CREATE INDEX TracerouteRelationIndex ON PingTracerouteDB.Ping (FromIP ASC, ToIP ASC);
