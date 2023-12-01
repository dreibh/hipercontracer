-- STEP 2: Create Schema
-- sudo -u postgres psql pingtraceroutedb <schema.sql
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


-- ###### NorNet Information ################################################
DROP TABLE IF EXISTS SiteInfo;
CREATE TABLE SiteInfo (
   SiteIndex    SMALLINT NOT NULL,                   -- NorNet Site Index
   TimeStamp    TIMESTAMP WITHOUT TIME ZONE NOT NULL,-- Time stamp for information

   -- ------ Name -----------------------------------------------------------
   FullName      CHAR(64),                           -- Full name
   ShortName     CHAR(8),                            -- Short name, e.g. SRL

   -- ------ Location -------------------------------------------------------
   Latitude      FLOAT,                              -- Latitude
   Longitude     FLOAT,                              -- Longitude
   CountryCode   CHAR(2),                            -- Country code, e.g. DE
   Country       VARCHAR(30),                        -- Country name
   Region        VARCHAR(30),                        -- Region name
   City          VARCHAR(30),                        -- City name

   PRIMARY KEY (SiteIndex)
);

DROP TABLE IF EXISTS ProviderInfo;
CREATE TABLE ProviderInfo (
   ProviderIndex SMALLINT NOT NULL,                   -- NorNet Provider Index
   TimeStamp     TIMESTAMP WITHOUT TIME ZONE NOT NULL,-- Time stamp for information

   -- ------ Name -----------------------------------------------------------
   FullName      CHAR(64),                           -- Full name
   ShortName     CHAR(16),                           -- Short name, e.g. BKK

   PRIMARY KEY (ProviderIndex)
);


-- ###### Ping ##############################################################
DROP TABLE IF EXISTS Ping;
CREATE TABLE Ping (
   TimeStamp TIMESTAMP WITHOUT TIME ZONE NOT NULL,   -- Time stamp (always UTC!)
   FromIP    INET     NOT NULL,                      -- Source IP address
   ToIP      INET     NOT NULL,                      -- Destination IP address
   Checksum  INTEGER  NOT NULL DEFAULT 0,            -- Checksum
   PktSize   INTEGER  NOT NULL DEFAULT 0,            -- Packet size
   TC        SMALLINT NOT NULL DEFAULT 0,            -- Traffic Class
   Status    SMALLINT NOT NULL,                      -- Status
   RTT       INTEGER  NOT NULL,                      -- microseconds (max. 2147s)
   PRIMARY KEY (FromIP, ToIP, TC, TimeStamp)
);

CREATE INDEX PingTimeStampIndex ON Ping (TimeStamp ASC);
-- CREATE INDEX PingFromIPIndex ON Ping (FromIP ASC);
-- CREATE INDEX PingToIPIndex ON Ping (ToIP ASC);
-- CREATE INDEX PingStatusIndex ON Ping (Status ASC);


-- ###### Traceroute ########################################################
DROP TABLE IF EXISTS Traceroute;
CREATE TABLE Traceroute (
   TimeStamp TIMESTAMP WITHOUT TIME ZONE NOT NULL,   -- Time stamp (always UTC!)
   FromIP    INET     NOT NULL,                      -- Source IP address
   ToIP      INET     NOT NULL,                      -- Destination IP address
   Checksum  INTEGER  NOT NULL DEFAULT 0,            -- Checksum
   PktSize   INTEGER  NOT NULL DEFAULT 0,            -- Packet size
   TC        SMALLINT NOT NULL DEFAULT 0,            -- Traffic Class
   HopNumber SMALLINT NOT NULL,                      -- Current hop number
   TotalHops SMALLINT NOT NULL,                      -- Total number of hops
   Status    SMALLINT NOT NULL,                      -- Status
   RTT       INTEGER  NOT NULL,                      -- microseconds (max. 2147s)
   HopIP     INET     NOT NULL,                      -- Router or Destination IP address
   PathHash  BIGINT   NOT NULL,                      -- Hash over full path
   Round     INTEGER  NOT NULL DEFAULT 0,            -- Round number
   PRIMARY KEY (FromIP,ToIP,TC,TimeStamp,Round,HopNumber)
);

CREATE INDEX TracerouteTimeStampIndex ON Traceroute (TimeStamp ASC);
-- CREATE INDEX TraceroutePathHashIndex ON Traceroute (PathHash ASC);
-- CREATE INDEX TracerouteFromIPIndex ON Traceroute (FromIP ASC);
-- CREATE INDEX TracerouteToIPIndex ON Traceroute (ToIP ASC);
-- CREATE INDEX TracerouteHopIPIndex ON Traceroute (HopIP ASC);


-- ###### Address Information ###############################################
DROP TABLE IF EXISTS AddressInfo;
CREATE TABLE AddressInfo (
   IP           INET NOT NULL,                       -- IP address
   TimeStamp    TIMESTAMP WITHOUT TIME ZONE NOT NULL,-- Time stamp for information

   -- ------ NorNet ---------------------------------------------------------
   SiteIndex     SMALLINT REFERENCES SiteInfo(SiteIndex),         -- NorNet Site ID
   ProviderIndex SMALLINT REFERENCES ProviderInfo(ProviderIndex), -- NorNet Provider ID

   -- ----- Autonomous System -----------------------------------------------
   ASNumber      INTEGER,                            -- Autonomous System number

   -- ------ GeoIP ----------------------------------------------------------
   Latitude      FLOAT,                              -- Latitude
   Longitude     FLOAT,                              -- Longitude
   CountryCode   CHAR(2),                            -- Country code, e.g. DE
   PostalCode    CHAR(8),                            -- Postal code, e.g. 45326
   Country       VARCHAR(30),                        -- Country name
   Region        VARCHAR(30),                        -- Region name
   City          VARCHAR(48),                        -- City name
   Organisation  VARCHAR(80),                        -- Organisation name

   -- ------ DNS ------------------------------------------------------------
   FQDN         VARCHAR(253),                        -- Fully-qualified domain name
   PRIMARY KEY (IP)
);

CREATE INDEX AddressInfoASNumberIndex ON AddressInfo (ASNumber ASC NULLS LAST);
