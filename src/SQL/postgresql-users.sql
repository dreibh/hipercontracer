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


-- ##########################################################################
-- IMPORTANT NOTE:
-- This script requires changing the placeholders below first:
-- * DATABASE
-- * ROOT_PASSWORD
-- * MAINTAINER_PASSWORD
-- * IMPORTER_PASSWORD
-- * RESEARCHER_PASSWORD
-- ##########################################################################


ALTER USER postgres WITH PASSWORD '${ROOT_PASSWORD}';

REASSIGN OWNED BY maintainer TO postgres;
DROP OWNED BY maintainer;
DROP USER IF EXISTS maintainer;
CREATE USER maintainer WITH LOGIN ENCRYPTED PASSWORD '${MAINTAINER_PASSWORD}';
GRANT ALL PRIVILEGES ON DATABASE ${DATABASE} TO maintainer;
GRANT ALL PRIVILEGES ON SCHEMA public TO maintainer;
ALTER DATABASE ${DATABASE} OWNER TO maintainer;
ALTER TABLE Ping OWNER TO maintainer;
ALTER TABLE Traceroute OWNER TO maintainer;
ALTER TABLE Jitter OWNER TO maintainer;

REASSIGN OWNED BY importer TO postgres;
DROP OWNED BY importer;
DROP USER IF EXISTS importer;
CREATE USER importer WITH LOGIN ENCRYPTED PASSWORD '${IMPORTER_PASSWORD}';
GRANT CONNECT ON DATABASE ${DATABASE} TO importer;
GRANT INSERT, UPDATE ON TABLE Ping TO importer;
GRANT INSERT, UPDATE ON TABLE Traceroute TO importer;
GRANT INSERT, UPDATE ON TABLE Jitter TO importer;

REASSIGN OWNED BY researcher TO postgres;
DROP OWNED BY researcher;
DROP USER IF EXISTS researcher;
CREATE USER researcher WITH LOGIN ENCRYPTED PASSWORD '${RESEARCHER_PASSWORD}';
GRANT CONNECT ON DATABASE ${DATABASE} TO researcher;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO researcher;
