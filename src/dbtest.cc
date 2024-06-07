#include "databaseclient-mariadb.h"
#include "tools.h"

#include <chrono>
#include <iostream>

// #define TEST1
#define TEST2
#define TEST3
#define TEST4
// #define TEST5


void prepareTable(DatabaseClientBase& client)
{
   client.executeUpdate("DROP TABLE IF EXISTS test1");
   client.commit();
   client.executeUpdate("CREATE TABLE test1(id INT, label VARCHAR(64), PRIMARY KEY(id))");
   client.commit();
}


// ###### Main program ######################################################
int main(int argc, char** argv)
{
   std::chrono::high_resolution_clock::time_point s;
   std::chrono::high_resolution_clock::time_point e;
   const unsigned int                             items = 100000;

   // ====== Read database configuration ====================================
   DatabaseConfiguration databaseConfiguration;
   if(!databaseConfiguration.readConfiguration("/home/dreibh/testdb-users-mariadb-maintainer.conf")) {
      exit(1);
   }


   // ====== Connect ========================================================
   MariaDBClient client(databaseConfiguration);
   client.open();
   printf("Online!\n");


   // ====== Test 4 ==========================================================
#ifdef TEST4
   prepareTable(client);

   printf("Test 4\n");
   Statement& stm = client.getStatement("test1", false);
   s = std::chrono::high_resolution_clock::now();
   stm << "INSERT INTO test1 VALUES";
   for(unsigned int i = 0; i < items; i++) {
      stm.beginRow();
      stm << i << stm.sep() << "\"Test #" + std::to_string(i) + "\"";
      stm.endRow();
   }
   ((DatabaseClientBase&)client).executeUpdate(stm);
   client.commit();
   e = std::chrono::high_resolution_clock::now();

   std::cout << "Duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count() << " ms\n";
#endif


   // ====== Test 3 =========================================================
#ifdef TEST3
   prepareTable(client);

   // Based on: https://mariadb.com/kb/en/bulk-insert-row-wise-binding/

   printf("Test 3\n");
   s = std::chrono::high_resolution_clock::now();

   MYSQL_STMT* stmt= mysql_stmt_init(client.getConnection());
   const char* insert = "INSERT INTO test1 VALUES (?,?)";
   if(mysql_stmt_prepare(stmt, insert, strlen(insert))) {
      puts("PREPARE FAILED!");
      exit(1);
   }

   struct DataDefinition {
      unsigned long ID;
      char ID_ind;
      char Label[32];
      char Label_ind;
   };
   DataDefinition data[items];
   // memset(data, 0, sizeof(data));
   for(unsigned int i = 0; i < items; i++) {
      data[i].ID = i;
      data[i].ID_ind = STMT_INDICATOR_NULL;
      // memset((char*)&data[i].Label, 0, sizeof(data[i].Label));
      snprintf((char*)&data[i].Label, sizeof(data[i].Label), "Test #%u", i);
      data[i].Label_ind = STMT_INDICATOR_NTS;
   }

   MYSQL_BIND bind[2];
   memset(bind, 0, sizeof(bind));

   bind[0].buffer_type= MYSQL_TYPE_LONG;
   bind[0].buffer  = &data[0].ID;
   bind[0].is_null = 0;
//    bind[0].length  = 0;
//    bind[0].u.indicator= &data[0].ID_ind;

   bind[1].buffer_type= MYSQL_TYPE_STRING;
   bind[1].buffer  = &data[0].Label;
   bind[1].is_null = 0;
   bind[1].u.indicator= &data[0].Label_ind;

   const unsigned int array_size = items;
   mysql_stmt_attr_set(stmt, STMT_ATTR_ARRAY_SIZE, &array_size);
   const size_t row_size = sizeof(struct DataDefinition);   // size_t !!!
   mysql_stmt_attr_set(stmt, STMT_ATTR_ROW_SIZE, &row_size);

   mysql_stmt_bind_param(stmt, bind);

   if(mysql_stmt_execute(stmt)) {
      puts("EXECUTE FAILED!");
   }
   mysql_stmt_close(stmt);

   e = std::chrono::high_resolution_clock::now();

   std::cout << "Duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count() << " ms\n";
#endif


   // ====== Test 2 =========================================================
   Statement statement(DatabaseBackendType::SQL_MariaDB);
#ifdef TEST2
   prepareTable(client);

   printf("Test 2\n");
   s = std::chrono::high_resolution_clock::now();
   statement << "INSERT INTO test1 VALUES ";
   for(unsigned int i = 0; i < items; i++) {
      if(i > 0) {
         statement << ",";
      }
      statement << "(" + std::to_string(i) + ", 'Test #" + std::to_string(i) + "')";
   }
   client.executeUpdate(statement);
   client.commit();
   e = std::chrono::high_resolution_clock::now();

   std::cout << "Duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count() << " ms\n";
#endif


   // ====== Test 1 =========================================================
#ifdef TEST1
   prepareTable(client);

   printf("Test 1\n");
   s = std::chrono::high_resolution_clock::now();
   for(unsigned int i = 0; i < items; i++) {
      statement << "INSERT INTO test1 VALUES (" + std::to_string(i) + ", 'Test #" + std::to_string(i) + "')";
      client.executeUpdate(statement);
   }
   client.commit();
   e = std::chrono::high_resolution_clock::now();

   std::cout << "Duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count() << " ms\n";
#endif


   // ====== Test ===========================================================
#ifdef TEST5
   printf("Test 5\n");
   ((DatabaseClientBase&)client).executeQuery("SELECT * FROM test1");
   while(client.fetchNextTuple()) {
      const int id            = client.getInteger(1);
      const std::string label = client.getString(2);
      printf("%6d: %s\n", id, label.c_str());
   }
#endif

   return 0;
}
