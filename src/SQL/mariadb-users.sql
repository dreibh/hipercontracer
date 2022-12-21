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


-- ##########################################################################
-- IMPORTANT NOTE:
-- This script requires changing the placeholders below first:
-- * MAINTAINER_PASSWORD
-- * IMPORTER_PASSWORD
-- * RESEARCHER_PASSWORD
-- ##########################################################################


DROP USER IF EXISTS maintainer;
CREATE USER maintainer IDENTIFIED BY '${MAINTAINER_PASSWORD}';
GRANT ALL PRIVILEGES ON * TO maintainer;

DROP USER IF EXISTS importer;
CREATE USER importer IDENTIFIED BY '${IMPORTER_PASSWORD}';
GRANT INSERT, UPDATE ON Ping TO importer;
GRANT INSERT, UPDATE ON Traceroute TO importer;

DROP USER IF EXISTS researcher;
CREATE USER researcher IDENTIFIED BY '${RESEARCHER_PASSWORD}';
GRANT SELECT, INSERT, UPDATE ON * TO researcher;

FLUSH PRIVILEGES;