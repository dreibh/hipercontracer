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


-- ###### Ping ##############################################################
DROP TABLE IF EXISTS Ping;
CREATE TABLE Ping (
   Date   TIMESTAMP WITHOUT TIME ZONE NOT NULL,      -- Time stamp (always UTC!)
   FromIP INET     NOT NULL,                         -- Source IP address
   ToIP   INET     NOT NULL,                         -- Destination IP address
   Status SMALLINT NOT NULL,                         -- Status
   RTT    INTEGER  NOT NULL,                         -- microseconds (max. 2147s)
   PRIMARY KEY (Date,FromIP,ToIP)
);

CREATE INDEX PingFromIPIndex ON Ping (FromIP ASC);
CREATE INDEX PingToIPIndex ON Ping (ToIP ASC);
CREATE INDEX PingStatusIndex ON Ping (Status ASC);


-- ###### Traceroute ########################################################
DROP TABLE IF EXISTS Traceroute;
CREATE TABLE Traceroute (
   Date      TIMESTAMP WITHOUT TIME ZONE NOT NULL,   -- Time stamp (always UTC!)
   FromIP    INET     NOT NULL,                      -- Source IP address
   ToIP      INET     NOT NULL,                      -- Destination IP address
   HopNumber SMALLINT NOT NULL,                      -- Current hop number
   TotalHops SMALLINT NOT NULL,                      -- Total number of hops
   Status    SMALLINT NOT NULL,                      -- Status
   RTT       INTEGER  NOT NULL,                      -- microseconds (max. 2147s)
   HopIP     INET     NOT NULL,                      -- Router or Destination IP address
   PathHash  BIGINT   NOT NULL,                      -- Hash over full path
   PRIMARY KEY(Date,FromIP,ToIP,HopNumber)
);

CREATE INDEX TraceroutePathHashIndex ON Traceroute (PathHash ASC);
CREATE INDEX TracerouteFromIPIndex ON Traceroute (FromIP ASC);
CREATE INDEX TracerouteToIPIndex ON Traceroute (ToIP ASC);


-- ###### Address Information ###############################################
DROP TABLE IF EXISTS AddressInfo;
CREATE TABLE AddressInfo (
   IP           INET NOT NULL,                       -- IP address
   TimeStamp    DATE NOT NULL,                       -- Time stamp for information

   -- ------ NorNet ---------------------------------------------------------
   SiteID       SMALLINT,                            -- NorNet Site ID
   ProviderID   SMALLINT,                            -- NorNet Provider ID

   -- ----- Autonomous System -----------------------------------------------
   ASNumber     INTEGER,                             -- Autonomous System number

   -- ------ GeoIP ----------------------------------------------------------
   Latitude     FLOAT,                               -- Latitude
   Longitude    FLOAT,                               -- Longitude
   CountryCode  CHAR(2),                             -- Country code, e.g. DE
   PostalCode   INTEGER,                             -- Postal code, e.g. 45326
   Country      VARCHAR(30),                         -- Country name
   Region       VARCHAR(30),                         -- Region name
   City         VARCHAR(30),                         -- City name
   Organisation VARCHAR(80),                         -- Organisation name

   -- ------ DNS ------------------------------------------------------------
   FQDN        VARCHAR(253),                         -- Fully-qualified domain name
   PRIMARY KEY (IP)
);

CREATE INDEX AddressInfoASNumberIndex ON AddressInfo (ASNumber ASC NULLS LAST);


-- ###### Additional Information ############################################
DROP TABLE IF EXISTS SiteInfo;
CREATE TABLE SiteInfo (
   SiteID  SMALLINT NOT NULL,
   Name    CHAR(64) NOT NULL,
   PRIMARY KEY (SiteID)
);

DROP TABLE IF EXISTS ProviderInfo;
CREATE TABLE ProviderInfo (
   ProviderID  SMALLINT NOT NULL,
   Name        CHAR(64) NOT NULL,
   PRIMARY KEY (ProviderID)
);
