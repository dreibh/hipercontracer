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


-- ###### Convert Unix Timestamp in Nanoseconds to DATETIME(6) in UTC #######
-- WARNING: DATETIME(6) only provides microseconds, not nanoseconds!
--          => Nanoseconds precision is lost!
DROP FUNCTION IF EXISTS UTCDateTime2UnixTimestamp;
DELIMITER $$
CREATE FUNCTION UTCDateTime2UnixTimestamp(utcDateTime DATETIME(6)) RETURNS INT8 DETERMINISTIC
BEGIN
   RETURN UNIX_TIMESTAMP(CONVERT_TZ(utcDateTime, '+00:00', @@global.time_zone)) * 1000000000;
   -- DECLARE year INT4;
   -- SET year = YEAR(utcDateTime);
   -- RETURN (year - 1970)              * 365 * 24 * 60 * 60000000000   -- year
   --    + FLOOR((year - 1969) / 4)     * 24 * 60 * 60000000000         -- 1 day more in a leap year
   --    - FLOOR((year - 1901) / 100)   * 24 * 60 * 60000000000         -- with 100-year exception
   --    + FLOOR((year - 1601) / 400)   * 24 * 60 * 60000000000         -- with 400-year exception
   --    + (DAYOFYEAR(utcDateTime) - 1) * 24 * 60 * 60000000000         -- day
   --    + HOUR(utcDateTime)            * 60 * 60000000000              -- hour
   --    + MINUTE(utcDateTime)          * 60000000000                   -- minute
   --    + SECOND(utcDateTime)          * 1000000000                    -- second
   --    + MICROSECOND(utcDateTime)     * 1000;                         -- microsecond
END
$$
DELIMITER ;


-- ###### Convert Unix Timestamp in Nanoseconds to DATETIME(6) in UTC #######
-- WARNING: DATETIME(6) only provides microseconds, not nanoseconds!
--          => Nanoseconds precision is lost!
DROP FUNCTION IF EXISTS UnixTimestamp2UTCDateTime;
DELIMITER $$
CREATE FUNCTION UnixTimestamp2UTCDateTime(unixTimestamp INT8) RETURNS DATETIME(6) DETERMINISTIC
BEGIN
   RETURN CONVERT_TZ(FROM_UNIXTIME(CAST(unixTimestamp AS DOUBLE) / 1000000000.0), @@global.time_zone, '+00:00');
END
$$
DELIMITER ;
