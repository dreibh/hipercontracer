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


-- ###### Partitioning by Date in Daily Partitions ##########################
-- Idea: 1 partition per day
-- Partitions:
-- * p<YYYY><mm><dd> daily partitions
-- * pMAX partition for values higher than the last daily partition
-- Procedures:
-- * ObtainDailyPartitionsOfTable()  -- obtain daily partitions of a table
-- * ObtainDailyPartitionsOfSchema() -- obtain daily partitions of a schema
-- * CreateDailyPartitionsForTable() -- create daily partitions, if necessary
-- ##########################################################################


-- ###### Obtain first and last partition names of a table ##################
DROP PROCEDURE IF EXISTS ObtainDailyPartitionsOfTable;
DELIMITER $$
CREATE PROCEDURE ObtainDailyPartitionsOfTable(pSchemaName VARCHAR(32),
                                              pTableName  VARCHAR(32))
BEGIN
   SELECT
      TABLE_SCHEMA          AS schemaName,
      TABLE_NAME            AS tableName,
      COUNT(PARTITION_NAME) AS dailyPartitions,
      MIN(PARTITION_NAME)   AS firstPartitionName,
      CAST(CONCAT(SUBSTRING(MIN(PARTITION_NAME), 2, 4), "-",
                  SUBSTRING(MIN(PARTITION_NAME), 6, 2), "-",
                  SUBSTRING(MIN(PARTITION_NAME), 8, 2)) AS DATE) AS firstPartitionDate,
      MAX(PARTITION_NAME) AS lastPartitionName,
      CAST(CONCAT(SUBSTRING(MAX(PARTITION_NAME), 2, 4), "-",
                  SUBSTRING(MAX(PARTITION_NAME), 6, 2), "-",
                  SUBSTRING(MAX(PARTITION_NAME), 8, 2)) AS DATE) AS lastPartitionDate,
      ( SELECT
           PARTITION_NAME
        FROM
           INFORMATION_SCHEMA.PARTITIONS
        WHERE
           TABLE_SCHEMA = pSchemaName AND
           TABLE_NAME   = pTableName  AND
           PARTITION_NAME = "pMAX"
      ) AS maxValuePartition
   FROM
      INFORMATION_SCHEMA.PARTITIONS
   WHERE
      TABLE_SCHEMA = pSchemaName AND
      TABLE_NAME   = pTableName  AND
      PARTITION_NAME RLIKE "p[0-9]+";
END;
$$
DELIMITER ;


-- ###### Obtain first and last partition names of a table ##################
DROP PROCEDURE IF EXISTS ObtainDailyPartitionsOfSchema;
DELIMITER $$
CREATE PROCEDURE ObtainDailyPartitionsOfSchema(pSchemaName VARCHAR(32))
BEGIN
   SELECT
      TABLE_SCHEMA          AS schemaName,
      TABLE_NAME            AS tableName,
      COUNT(PARTITION_NAME) AS dailyPartitions,
      MIN(PARTITION_NAME)   AS firstPartitionName,
      CAST(CONCAT(SUBSTRING(MIN(PARTITION_NAME), 2, 4), "-",
                  SUBSTRING(MIN(PARTITION_NAME), 6, 2), "-",
                  SUBSTRING(MIN(PARTITION_NAME), 8, 2)) AS DATE) AS firstPartitionDate,
      MAX(PARTITION_NAME) AS lastPartitionName,
      CAST(CONCAT(SUBSTRING(MAX(PARTITION_NAME), 2, 4), "-",
                  SUBSTRING(MAX(PARTITION_NAME), 6, 2), "-",
                  SUBSTRING(MAX(PARTITION_NAME), 8, 2)) AS DATE) AS lastPartitionDate
   FROM
      INFORMATION_SCHEMA.PARTITIONS
   WHERE
      TABLE_SCHEMA = pSchemaName AND
      PARTITION_NAME RLIKE "p[0-9]+"
   GROUP BY
      schemaName,
      tableName
   ORDER BY
      tableName ASC;
END;
$$
DELIMITER ;


-- ###### Function to add daily partitions for a given time range ###########
DROP PROCEDURE IF EXISTS AddDailyPartitionsForTable;
DELIMITER $$
CREATE PROCEDURE AddDailyPartitionsForTable(pSchemaName            VARCHAR(32),
                                            pTableName             VARCHAR(32),
                                            pTimestampColumn       VARCHAR(32),
                                            pMaxValuePartitionName VARCHAR(32),
                                            pPartitioningStartDate DATE,
                                            pPartitioningEndDate   DATE)
BEGIN
   DECLARE currentDate        DATE;
   DECLARE currentPartition   VARCHAR(32);
   DECLARE iteration          INTEGER;
   DECLARE limitNewPartitions INTEGER DEFAULT 4000;   -- partitions

   -- ====== Start partitioning, if there is no partitioning ================
   IF pMaxValuePartitionName IS NULL THEN
      SET @sqlText := CONCAT('ALTER TABLE `', pSchemaName, '`.`', pTableName, '` PARTITION BY RANGE ( ', pTimestampColumn, ' ) ( ');
   ELSE
      SET @sqlText := CONCAT('ALTER TABLE `', pSchemaName, '`.`', pTableName, '` REORGANIZE PARTITION `', pMaxValuePartitionName, '` INTO ( ');
   END IF;

   SET currentDate = pPartitioningStartDate;
   SET iteration   = 0;
   partitioningLoop: WHILE currentDate < pPartitioningEndDate DO
      -- ------ Check limit -------------------------------------------------
      SET iteration = iteration + 1;
      IF iteration > limitNewPartitions THEN
	     LEAVE partitioningLoop;
      END IF;

      -- ------ Add daily partition -----------------------------------------
      SET currentDate      = DATE_ADD(currentDate, INTERVAL 1 DAY);
      SET currentPartition = DATE_FORMAT(currentDate, "p%Y%m%d");
      IF iteration > 1 THEN
         SET @sqlText = CONCAT(@sqlText, ', ');
      END IF;
      SET @sqlText = CONCAT(@sqlText,
                            'PARTITION ', currentPartition,
                            ' VALUES LESS THAN (', UTCDateTime2UnixTimestamp(currentDate), ')');
   END WHILE;

   -- ------ Create partitions ----------------------------------------------
   IF iteration > 0 THEN
      SET @sqlText = CONCAT(@sqlText,
                            ', PARTITION pMAX VALUES LESS THAN MAXVALUE );');
      PREPARE statement FROM @sqlText;
      EXECUTE statement;
      DEALLOCATE PREPARE statement;
   ELSE
      SET @sqlText = NULL;
   END IF;

   -- SELECT @firstDateInData, @lastDateInData, firstPartitionName, firstPartitionDate, lastPartitionName, lastPartitionDate, pMaxValuePartitionName, @sqlText;
   SELECT iteration AS iterations, pPartitioningStartDate, pPartitioningEndDate, @sqlText;
END;
$$
DELIMITER ;


-- ###### Function to create new partitions, according to data timestamps ###
DROP PROCEDURE IF EXISTS CreateDailyPartitionsForTable;
DELIMITER $$
CREATE PROCEDURE CreateDailyPartitionsForTable(pSchemaName      VARCHAR(32),
                                               pTableName       VARCHAR(32),
                                               pTimestampColumn VARCHAR(32))
proc:BEGIN
   DECLARE done                  BOOLEAN     DEFAULT FALSE;
   DECLARE firstPartitionName    VARCHAR(32) DEFAULT NULL;
   DECLARE firstPartitionDate    DATE        DEFAULT NULL;
   DECLARE lastPartitionName     VARCHAR(32) DEFAULT NULL;
   DECLARE lastPartitionDate     DATE        DEFAULT NULL;
   DECLARE maxValuePartitionName VARCHAR(32) DEFAULT NULL;
   DECLARE partitioningStartDate DATE        DEFAULT NULL;
   DECLARE partitioningEndDate   DATE        DEFAULT NULL;
   DECLARE futurePartitions      INTEGER DEFAULT 7;      -- days

   -- ====== Handle for "not found" =========================================
   DECLARE CONTINUE HANDLER FOR NOT FOUND SET done = TRUE;

   -- ====== Get first and last dates of the data in the table ==============
   SET @firstDateInData := NULL;
   SET @lastDateInData  := NULL;
   SET @sqlText := CONCAT('SELECT ',
                             'CAST(FROM_UNIXTIME(FLOOR(MIN(`', pTimestampColumn, '`) / 1000000000.0)) AS DATE), ',
                             'CAST(FROM_UNIXTIME(CEIL(MAX(`', pTimestampColumn, '`) / 1000000000.0)) AS DATE) ',
                          'INTO @firstDateInData, @lastDateInData ',
                          'FROM `', pSchemaName, '`.`', pTableName, '`;');
   PREPARE statement FROM @sqlText;
   EXECUTE statement;
   DEALLOCATE PREPARE statement;
   IF (@firstDateInData IS NULL) OR (@lastDateInData IS NULL) THEN
      -- The table is empty => nothing to do!
      SELECT NULL;
      LEAVE proc;
   END IF;

   -- ====== Get first and last daily partitions of the table ===============
   SELECT
      MIN(PARTITION_NAME) AS firstPartitionName,
      CAST(CONCAT(SUBSTRING(MIN(PARTITION_NAME), 2, 4), "-",
                  SUBSTRING(MIN(PARTITION_NAME), 6, 2), "-",
                  SUBSTRING(MIN(PARTITION_NAME), 8, 2)) AS DATE) AS firstPartitionDate,
      MAX(PARTITION_NAME) AS lastPartitionName,
      CAST(CONCAT(SUBSTRING(MAX(PARTITION_NAME), 2, 4), "-",
                  SUBSTRING(MAX(PARTITION_NAME), 6, 2), "-",
                  SUBSTRING(MAX(PARTITION_NAME), 8, 2)) AS DATE) AS lastPartitionDate,
      ( SELECT
           PARTITION_NAME
        FROM
           INFORMATION_SCHEMA.PARTITIONS
        WHERE
           TABLE_SCHEMA = pSchemaName AND
           TABLE_NAME   = pTableName  AND
           PARTITION_NAME = "pMAX"
      ) AS maxValuePartition
   INTO
      firstPartitionName,
      firstPartitionDate,
      lastPartitionName,
      lastPartitionDate,
      maxValuePartitionName
   FROM
      INFORMATION_SCHEMA.PARTITIONS
   WHERE
      TABLE_SCHEMA = pSchemaName AND
      TABLE_NAME   = pTableName  AND
      PARTITION_NAME RLIKE "p[0-9]+";

   -- ====== Obtain date range for new partitions ===========================
   IF maxValuePartitionName IS NULL THEN
      -- There is no partitioning, yet. Use the first data item's time stamp
      -- to begin the partitioning:
      SET partitioningStartDate = @firstDateInData;
   ELSE
      -- There is already an existing partitioning:
      SET partitioningStartDate = lastPartitionDate;
   END IF;
   IF @lastDateInData < CAST((UTC_DATE() + INTERVAL 1 DAY) AS DATE) THEN
      -- Useful date in @lastDateInData (the usual case) -> use it:
      SET partitioningEndDate = DATE_ADD(@lastDateInData, INTERVAL futurePartitions DAY);
   ELSE
      -- The latest data in the table has a timestamp far in the future
      -- -> use today's date instead:
      SET partitioningEndDate = CAST((UTC_DATE() + INTERVAL futurePartitions DAY) AS DATE);
   END IF;

   CALL AddDailyPartitionsForTable(pSchemaName, pTableName, pTimestampColumn,
                                   maxValuePartitionName,
                                   partitioningStartDate, partitioningEndDate);
END;
$$
DELIMITER ;


# ###### Add events to create partitions ####################################
DROP EVENT IF EXISTS PingMaintenance;
DROP EVENT IF EXISTS TracerouteMaintenance;
DROP EVENT IF EXISTS JitterMaintenance;
DELIMITER $$
CREATE EVENT PingMaintenance ON SCHEDULE EVERY 1 DAY STARTS CURRENT_TIMESTAMP + INTERVAL 0 HOUR DO
   CALL CreateDailyPartitionsForTable("test4hpct", "Ping", "SendTimestamp");
$$
CREATE EVENT TracerouteMaintenance ON SCHEDULE EVERY 1 DAY STARTS CURRENT_TIMESTAMP + INTERVAL 3 HOUR DO
   CALL CreateDailyPartitionsForTable("test4hpct", "Traceroute", "Timestamp");
$$
CREATE EVENT JitterMaintenance ON SCHEDULE EVERY 1 DAY STARTS CURRENT_TIMESTAMP + INTERVAL 6 HOUR DO
   CALL CreateDailyPartitionsForTable("test4hpct", "Jitter", "Timestamp");
$$
DELIMITER ;
