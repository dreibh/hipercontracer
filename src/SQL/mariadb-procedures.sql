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


-- ###### Function to create new partitions, according to data timestamps ###
DROP PROCEDURE IF EXISTS CreateNewPartitions;
DELIMITER $$
CREATE PROCEDURE CreateNewPartitions(schemaName    VARCHAR(32),
                                     tableName     VARCHAR(32),
                                     timestampName VARCHAR(32))
proc:BEGIN
   DECLARE tableIsPartitioned  BOOLEAN;
   DECLARE dateOfLastPartition DATE;
   DECLARE startDate           DATE;
   DECLARE endDate             DATE;
   DECLARE currentDate         DATE;
   DECLARE currentPartition    VARCHAR(32);
   DECLARE iteration           INTEGER;
   DECLARE limitNewPartitions  INTEGER DEFAULT 1000000;   -- partitions
   DECLARE futurePartitions    INTEGER DEFAULT 7;        -- days

   -- ====== Get minimum timestamp ==========================================
   SET @minTimestamp = NULL;
   SET @maxTimestamp = NULL;
   SET @sqlText = CONCAT('SELECT ',
                         'CAST(FROM_UNIXTIME(FLOOR(MIN(`', timestampName, '`)/1000000000.0)) AS DATE), ',
                         'CAST(FROM_UNIXTIME(CEIL(MAX(`', timestampName, '`)/1000000000.0)) AS DATE) ',
                         'INTO @minTimestamp, @maxTimestamp ',
                         'FROM `', schemaName, '`.`', tableName, '`;');
   PREPARE statement FROM @sqlText;
   EXECUTE statement;
   DEALLOCATE PREPARE statement;
   IF @minTimestamp IS NULL OR @maxTimestamp IS NULL THEN
      LEAVE proc;
   END IF;

   -- ====== Find latest partition ==========================================
   SET @lastPartition = NULL;
   SET @sqlText = CONCAT('SELECT PARTITION_NAME ',
                          'INTO @lastPartition ',
                          'FROM INFORMATION_SCHEMA.PARTITIONS ',
                          'WHERE ',
                             'TABLE_SCHEMA="', schemaName, '" AND ',
                             'TABLE_NAME="', tableName, '" ',
                          'ORDER BY PARTITION_NAME DESC ',
                          'LIMIT 1;');
   PREPARE statement FROM @sqlText;
   EXECUTE statement;
   DEALLOCATE PREPARE statement;

   -- ====== Get data of latest partition ===================================
   IF @lastPartition IS NOT NULL THEN
      SET tableIsPartitioned   = TRUE;
      SET @dateOfLastPartition = (
         SELECT DATE(CONCAT(SUBSTRING(@lastPartition, 2, 4), "-",
                            SUBSTRING(@lastPartition, 6, 2), "-",
                            SUBSTRING(@lastPartition, 8, 2)))
      );
   ELSE
      SET tableIsPartitioned   = FALSE;
      SET @dateOfLastPartition = @minTimeStamp;
   END IF;

   -- ====== Start partitioning, if there is no partitioning ================
   IF @dateOfLastPartition IS NULL THEN
     SET startDate = @minTimestamp;
   END IF;

   IF NOT tableIsPartitioned THEN
     SET @sqlText = CONCAT('ALTER TABLE `', schemaName, '`.`', tableName, '` PARTITION BY RANGE ( ', timestampName, ' ) ( ');
   ELSE
     SET @sqlText = CONCAT('ALTER TABLE `', schemaName, '`.`', tableName, '` ADD PARTITION ( ');
   END IF;

   SET startDate   = @dateOfLastPartition;
   SET endDate     = DATE_ADD(@maxTimestamp, INTERVAL futurePartitions DAY);
   SET currentDate = startDate;
   SET iteration   = 0;
   partitioningLoop: LOOP
      SET currentDate = DATE_ADD(currentDate, INTERVAL 1 DAY);
      IF currentDate > endDate THEN
	     LEAVE partitioningLoop;
      END IF;

      SET currentPartition = DATE_FORMAT(currentDate, "p%Y%m%d");
      IF iteration > 0 THEN
         SET @sqlText = CONCAT(@sqlText, ', ');
      END IF;
      SET @sqlText = CONCAT(@sqlText,
                               'PARTITION ', currentPartition,
                               ' VALUES LESS THAN (', 1000000000 * UNIX_TIMESTAMP(currentDate), ')');
      SET iteration = iteration + 1;
      IF tableIsPartitioned AND (iteration > limitNewPartitions) THEN
	     LEAVE partitioningLoop;
      END IF;
   END LOOP;

   IF iteration > 0 THEN
      SET @sqlText = CONCAT(@sqlText, ' );');
      PREPARE statement FROM @sqlText;
      EXECUTE statement;
      DEALLOCATE PREPARE statement;
   END IF;
END;
$$
DELIMITER ;


-- ###### Obtain first and last partition names #############################
DROP PROCEDURE IF EXISTS CheckPartitions;
DELIMITER $$
CREATE PROCEDURE CheckPartitions(schemaName VARCHAR(32),
                                 tableName  VARCHAR(32))
proc:BEGIN

   SET @sqlText1 = CONCAT('SELECT PARTITION_NAME ',
                           'INTO @firstPartition ',
                           'FROM INFORMATION_SCHEMA.PARTITIONS ',
                           'WHERE ',
                              'TABLE_SCHEMA="', schemaName, '" AND ',
                              'TABLE_NAME="', tableName, '" ',
                           'ORDER BY PARTITION_NAME ASC ',
                           'LIMIT 1;');
   SET @sqlText2 = CONCAT('SELECT PARTITION_NAME ',
                           'INTO @lastPartition ',
                           'FROM INFORMATION_SCHEMA.PARTITIONS ',
                           'WHERE ',
                              'TABLE_SCHEMA="', schemaName, '" AND ',
                              'TABLE_NAME="', tableName, '" ',
                           'ORDER BY PARTITION_NAME DESC ',
                           'LIMIT 1;');
   PREPARE statement FROM @sqlText1;
   EXECUTE statement;
   DEALLOCATE PREPARE statement;
   PREPARE statement FROM @sqlText2;
   EXECUTE statement;
   DEALLOCATE PREPARE statement;

   SELECT @firstPartition, @lastPartition;

END;
$$
DELIMITER ;


# ###### Add events to create partitions ####################################
DROP EVENT PingMaintenance;
DROP EVENT TracerouteMaintenance;
DROP EVENT JitterMaintenance;
DELIMITER $$
CREATE EVENT PingMaintenance ON SCHEDULE EVERY 1 DAY STARTS CURRENT_TIMESTAMP + INTERVAL 0 HOUR DO
   CALL CreateNewPartitions("test4hpct", "Ping", "SendTimestamp");
$$
CREATE EVENT TracerouteMaintenance ON SCHEDULE EVERY 1 DAY STARTS CURRENT_TIMESTAMP + INTERVAL 3 HOUR DO
   CALL CreateNewPartitions("test4hpct", "Traceroute", "Timestamp");
$$
CREATE EVENT JitterMaintenance ON SCHEDULE EVERY 1 DAY STARTS CURRENT_TIMESTAMP + INTERVAL 6 HOUR DO
   CALL CreateNewPartitions("test4hpct", "Jitter", "Timestamp");
$$
DELIMITER ;


# ###### Manual tests #######################################################
-- CALL CreateNewPartitions("test4hpct", "Ping", "SendTimestamp");
-- CALL CreateNewPartitions("test4hpct", "Traceroute", "Timestamp");
-- CALL CreateNewPartitions("test4hpct", "Jitter", "Timestamp");
--
-- CALL CheckPartitions("test4hpct", "Ping");
-- CALL CheckPartitions("test4hpct", "Traceroute");
-- CALL CheckPartitions("test4hpct", "Jitter");
