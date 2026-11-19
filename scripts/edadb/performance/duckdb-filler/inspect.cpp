#include <duckdb.hpp>
#include <iostream>
#include <stdexcept>
#include <string>

std::string quoted(const std::string& name) {
    std::string result = "\"";
    for (char character : name) {
        result += character;
        if (character == '"') result += character;
    }
    return result + "\"";
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    try {
        duckdb::DBConfig config;
        config.options.access_mode = duckdb::AccessMode::READ_ONLY;
        duckdb::DuckDB database(argv[1], &config);
        duckdb::Connection connection(database);
        auto query = [&](const std::string& sql) {
            auto result = connection.Query(sql);
            if (!result || result->HasError()) throw std::runtime_error(result ? result->GetError() : "no result");
            return result;
        };
        std::cout << "VERSION " << duckdb::DuckDB::LibraryVersion() << '\n';
        auto settings = query("SELECT name,value FROM duckdb_settings() WHERE name IN ('threads','memory_limit') ORDER BY name");
        std::cout << settings->ToString();
        auto constraints = query("SELECT table_name,constraint_column_names FROM duckdb_constraints() WHERE constraint_type='FOREIGN KEY' ORDER BY table_name");
        for (duckdb::idx_t row = 0; row < constraints->RowCount(); ++row) {
            const std::string table = constraints->GetValue(0, row).ToString();
            const auto column_value = constraints->GetValue(1, row);
            const auto& columns = duckdb::ListValue::GetChildren(column_value);
            std::string projection;
            for (const auto& column : columns) {
                if (!projection.empty()) projection += ",";
                projection += quoted(column.ToString());
            }
            auto sample = query("SELECT " + projection + " FROM " + quoted(table) + " LIMIT 1");
            std::cout << "TABLE " << table << '\n';
            std::cout << query("SELECT count(*) FROM " + quoted(table))->ToString();
            if (sample->RowCount() == 0) continue;
            std::string sql = "SELECT * FROM " + quoted(table) + " WHERE ";
            for (duckdb::idx_t column = 0; column < columns.size(); ++column) {
                if (column) sql += " AND ";
                sql += quoted(columns[column].ToString()) + "=" + sample->GetValue(column, 0).ToSQLString();
            }
            std::cout << "SQL " << sql << '\n';
            auto plan = query("EXPLAIN " + sql);
            for (duckdb::idx_t plan_row = 0; plan_row < plan->RowCount(); ++plan_row)
                std::cout << plan->GetValue(1, plan_row).ToString() << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
