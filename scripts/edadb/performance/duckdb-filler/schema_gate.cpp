#include <iostream>

namespace idb::edadb_adapter { int initWriteDb(const char*); }

int main() {
    const int status = idb::edadb_adapter::initWriteDb(":memory:");
    std::cout << "schema_status=" << status << '\n';
    return status == 0 ? 0 : 1;
}
