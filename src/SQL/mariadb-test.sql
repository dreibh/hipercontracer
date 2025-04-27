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


# ###### Basic database features test #######################################
SELECT COUNT(*) FROM Ping;
SELECT * FROM Ping LIMIT 10;
SELECT COUNT(*) FROM Traceroute;
SELECT * FROM Traceroute LIMIT 30;
SELECT COUNT(*) FROM Jitter;
SELECT * FROM Jitter LIMIT 5;


# ###### Test helper functions ##############################################
SELECT UTCDateTime2UnixTimestamp('2014-09-29 11:22:33.789123');
SELECT UnixTimestamp2UTCDateTime(1411989753789123000);

SELECT UTCDateTime2UnixTimestamp('2025-06-15 15:06:40.123567');
SELECT UnixTimestamp2UTCDateTime(1750000000000000000);

SELECT UnixTimestamp2UTCDateTime(SendTimestamp), SendTimestamp FROM Ping ORDER BY SendTimestamp LIMIT 10;
SELECT TimeStamp FROM Ping_v1 ORDER BY TimeStamp LIMIT 10;

SELECT UnixTimestamp2UTCDateTime(Timestamp), Timestamp FROM Traceroute ORDER BY Timestamp LIMIT 10;
SELECT TimeStamp FROM Traceroute_v1 ORDER BY Timestamp LIMIT 10;
