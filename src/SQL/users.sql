-- STEP 3: Create Users
-- sudo -u postgres psql pingtraceroutedb <users.postgres

REVOKE ALL ON DATABASE pingtraceroutedb FROM importer;
REVOKE ALL ON Ping FROM importer;
REVOKE ALL ON Traceroute FROM importer;
DROP ROLE importer;
CREATE ROLE importer WITH LOGIN ENCRYPTED PASSWORD '!import!';
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
GRANT ALL PRIVILEGES ON DATABASE pingtraceroutedb TO maintainer;
