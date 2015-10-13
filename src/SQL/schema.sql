-- STEP 2: Create Schema
-- sudo -u postgres psql pingtraceroutedb <schema.sql
--
--
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
   Date   TIMESTAMP WITHOUT TIME ZONE NOT NULL,      -- Time stamp (always UTC!)
   FromIP INET     NOT NULL,                         -- Source IP address
   ToIP   INET     NOT NULL,                         -- Destination IP address
   Status SMALLINT NOT NULL,                         -- Status
   RTT    INTEGER  NOT NULL,                         -- microseconds (max. 2147s)
   PRIMARY KEY (Date,FromIP,ToIP)
);

DROP TABLE Traceroute;
CREATE TABLE Traceroute (
   Date      TIMESTAMP WITHOUT TIME ZONE NOT NULL,   -- Time stamp (always UTC!)
   FromIP    INET NOT NULL,                          -- Source IP address
   ToIP      INET NOT NULL,                          -- Destination IP address
   HopNumber SMALLINT(0..255) NOT NULL,              -- Current hop number
   TotalHops SMALLINT(0..255) NOT NULL,              -- Total number of hops
   Status    SMALLINT NOT NULL,                      -- Status
   RTT       INTEGER  NOT NULL,                      -- microseconds (max. 2147s)
   HopIP     INET     NOT NULL,                      -- Router or Destination IP address
   PRIMARY KEY(Date,FromIP,ToIP,HopNumber)
);

DROP TABLE AddressInfo;
CREATE TABLE AddressInfo (
   IP         INET NOT NULL,                         -- IP address
   TimeStamp  DATE NOT NULL,                         -- Time stamp for information
   SiteID     SHORTINT,                              -- NorNet Site ID
   ProviderID SHORTINT,                              -- NorNet Provider ID
   ASNumber   INTEGER,                               -- Autonomous System number
   FQDN       CHAR(253),                             -- Fully-qualified domain name
   PRIMARY KEY (IP)
);

DROP TABLE SiteInfo;
CREATE TABLE SiteInfo (
   SiteID  SHORTINT NOT NULL,
   Name    CHAR(64),
   PRIMARY KEY (SiteID)
);

DROP TABLE ProviderInfo;
CREATE TABLE ProviderInfo (
   ProviderID  SHORTINT NOT NULL,
   Name    CHAR(64),
   PRIMARY KEY (ProviderID)
);
