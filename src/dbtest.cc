#include "databaseclient-mariadb.h"
#include "tools.h"

#include <chrono>

#include <cppconn/prepared_statement.h>


void prepareTable(MariaDBClient& client)
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
   if(!databaseConfiguration.readConfiguration("/home/dreibh/soyuz.conf")) {
      exit(1);
   }
   // databaseConfiguration.printConfiguration(std::cout);


   // ====== Connect ========================================================
   MariaDBClient client(databaseConfiguration);
   client.open();
   printf("Online!\n");


   // ====== Test ===========================================================
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


   // ====== Test ===========================================================
   prepareTable(client);

   printf("Test 3\n");
   s = std::chrono::high_resolution_clock::now();
   sql::PreparedStatement* pstmt = client.getConnection()->prepareStatement(
      "INSERT INTO test1 VALUES (?, ?)"
   );
   for(unsigned int i = 0; i < items; i++) {
      pstmt->setInt(1, i);
      pstmt->setString(2, ("Test #" + std::to_string(i)).c_str());
      pstmt->executeUpdate();
   }
   delete pstmt;
   client.commit();
   e = std::chrono::high_resolution_clock::now();

   std::cout << "Duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count() << " ms\n";


   // ====== Test ===========================================================
   prepareTable(client);

   printf("Test 2\n");
   s = std::chrono::high_resolution_clock::now();
   std::string statement = "INSERT INTO test1 VALUES ";
   for(unsigned int i = 0; i < items; i++) {
      if(i > 0) {
         statement = statement + ",";
      }
      statement = statement +
         "(" + std::to_string(i) + ", 'Test #" + std::to_string(i) + "')";
   }
   client.executeUpdate(statement);
   client.commit();
   e = std::chrono::high_resolution_clock::now();

   std::cout << "Duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count() << " ms\n";


   // ====== Test ===========================================================
   prepareTable(client);

   printf("Test 1\n");
   s = std::chrono::high_resolution_clock::now();
   for(unsigned int i = 0; i < items; i++) {
      const std::string statement =
         "INSERT INTO test1 VALUES (" + std::to_string(i) + ", 'Test #" + std::to_string(i) + "')";
      client.executeUpdate(statement);
   }
   client.commit();
   e = std::chrono::high_resolution_clock::now();

   std::cout << "Duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(e - s).count() << " ms\n";

   return 0;
}
