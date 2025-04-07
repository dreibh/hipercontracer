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


-- ###### Convert Unix Timestamp in Nanoseconds to TIMESTAMP in UTC ##########
-- WARNING: TIMESTAMP only provides microseconds, not nanoseconds!
--          => Nanoseconds precision is lost!
DROP FUNCTION IF EXISTS UTCDateTime2UnixTimestamp;
CREATE FUNCTION UTCDateTime2UnixTimestamp(utcDateTime TIMESTAMP WITHOUT TIME ZONE)
RETURNS BIGINT
AS $$
BEGIN
   RETURN CAST((1000000000.0 * EXTRACT(EPOCH FROM utcDateTime)) AS BIGINT);
END;
$$
LANGUAGE plpgsql;


-- ###### Convert Unix Timestamp in Nanoseconds to TIMESTAMP in UTC #########
-- WARNING: TIMESTAMP only provides microseconds, not nanoseconds!
--          => Nanoseconds precision is lost!
DROP FUNCTION IF EXISTS UnixTimestamp2UTCDateTime;
CREATE FUNCTION UnixTimestamp2UTCDateTime(unixTimestamp BIGINT)
RETURNS TIMESTAMP WITHOUT TIME ZONE
AS $$
BEGIN
   RETURN TO_TIMESTAMP(unixTimestamp / 1000000000.0) AT TIME ZONE 'UTC';
END;
$$
LANGUAGE plpgsql;
