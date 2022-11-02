#include <iostream>
#include <filesystem>

// ###### Extract NodeID from path ##########################################
unsigned int getNodeIDFromPath(const std::filesystem::path& path)
{
   unsigned int nodeID = 0;

   std::filesystem::path parent = path.parent_path();
   while(parent.has_filename()) {
      if(parent.filename().string().size() >= 3) {
         const unsigned int n = atol(parent.filename().string().c_str());
         if( (n >=100) && (n <= 9999) ) {
            nodeID = n;
         }
      }
      parent = parent.parent_path();
   }

   return nodeID;
}

int main(int argc, char** argv)
{
   std::filesystem::path p1 = "data/4121/2022/01/02/12:00/nne4125-metadatacollector-20220728T021116.json";
   std::filesystem::path p2 = "data/4444//nne4444-metadatacollector-20220728T021116.json";

   unsigned int n1 = getNodeIDFromPath(p1);
   printf("%s -> NodeID=%u\n", p1.string().c_str(), n1);

   unsigned int n2 = getNodeIDFromPath(p2);
   printf("%s -> NodeID=%u\n", p2.string().c_str(), n2);

   return 0;
}
