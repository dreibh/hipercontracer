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
-- Copyright (C) 2015 by Thomas Dreibholz
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

DROP TABLE Ping;
CREATE TABLE Ping (
   Date   TIMESTAMP WITHOUT TIME ZONE NOT NULL,   -- Time stamp (always UTC!)
   FromIP INET      NOT NULL,                     -- Source IP address
   ToIP   INET      NOT NULL,                     -- Destination IP address
   Status INT       NOT NULL,                     -- Status
   RTT    INTEGER,                                -- microseconds (max. 2147s)
   PRIMARY KEY (Date,FromIP,ToIP)
);

DROP TABLE Traceroute;
CREATE TABLE Traceroute (
   Date   TIMESTAMP WITHOUT TIME ZONE NOT NULL,   -- Time stamp (always UTC!)
   Version    INT  NOT NULL,
   FromSI     INT  NOT NULL,
   FromPI     INT  NOT NULL,
   FromIP     INET NOT NULL,
   ToSI       INT  NOT NULL,
   ToPI       INT  NOT NULL,
   ToIP       INET NOT NULL,
   Path       TEXT NOT NULL,
   PathID     TEXT NOT NULL,
   HopNumber  INT  NOT NULL,
   PingNumber INT  NOT NULL,
   Min        TEXT NOT NULL,
   Avg        TEXT NOT NULL,
   Max        TEXT NOT NULL,
   Std        TEXT NOT NULL,
   Scheme     INT  NOT NULL,
   TotalMin   REAL NOT NULL,
   TotalAvg   REAL NOT NULL,
   TotalMax   REAL NOT NULL,
   TotalStd   REAL NOT NULL,
   PRIMARY KEY(Date,Version,FromSI,FromPI,ToSI,ToPI,Scheme)
);
