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


-- ##########################################################################
-- IMPORTANT NOTE:
-- This script requires changing the placeholders below first:
-- * DATABASE
-- * MAINTAINER_PASSWORD
-- * IMPORTER_PASSWORD
-- * RESEARCHER_PASSWORD
-- ##########################################################################


REASSIGN OWNED BY maintainer TO postgres;
DROP USER IF EXISTS maintainer;
CREATE USER maintainer WITH LOGIN ENCRYPTED PASSWORD '${MAINTAINER_PASSWORD}';
GRANT ALL PRIVILEGES ON DATABASE ${DATABASE} TO maintainer;
GRANT ALL PRIVILEGES ON SCHEMA public TO maintainer;

REASSIGN OWNED BY importer TO postgres;
DROP USER IF EXISTS importer;
CREATE USER importer WITH LOGIN ENCRYPTED PASSWORD '${IMPORTER_PASSWORD}';
GRANT CONNECT ON DATABASE ${DATABASE} TO importer;
GRANT INSERT, UPDATE ON TABLE Ping TO importer;
GRANT INSERT, UPDATE ON TABLE Traceroute TO importer;
GRANT INSERT, UPDATE ON TABLE Jitter TO importer;

REASSIGN OWNED BY researcher TO postgres;
DROP USER IF EXISTS researcher;
CREATE USER researcher WITH LOGIN ENCRYPTED PASSWORD '${RESEARCHER_PASSWORD}';
GRANT CONNECT ON DATABASE ${DATABASE} TO researcher;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO researcher;
