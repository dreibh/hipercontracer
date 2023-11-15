-- STEP 4: Create Users
-- sudo -u postgres psql pingtraceroutedb <users.sql
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


-- ##########################################################
-- !!! IMPORTANT: Change the passwords before deployment! !!!
-- ##########################################################


REVOKE ALL ON DATABASE pingtraceroutedb FROM importer;
REVOKE ALL ON Ping FROM importer;
REVOKE ALL ON Traceroute FROM importer;
DROP ROLE importer;
CREATE ROLE importer WITH LOGIN ENCRYPTED PASSWORD '!importer!';
GRANT CONNECT ON DATABASE pingtraceroutedb TO importer;
GRANT INSERT ON TABLE Ping TO importer;
GRANT INSERT ON TABLE Traceroute TO importer;


REVOKE ALL ON DATABASE pingtraceroutedb FROM researcher;
REVOKE ALL ON ALL TABLES IN SCHEMA public FROM researcher;
DROP ROLE researcher;
CREATE ROLE researcher WITH LOGIN ENCRYPTED PASSWORD '!researcher!';
GRANT CONNECT ON DATABASE pingtraceroutedb TO researcher;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO researcher;


REVOKE ALL ON DATABASE pingtraceroutedb FROM maintainer;
DROP ROLE maintainer;
CREATE ROLE maintainer WITH LOGIN ENCRYPTED PASSWORD '!maintainer!';
GRANT CONNECT ON DATABASE pingtraceroutedb TO maintainer;
GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA public TO maintainer;
