/**
 * @file DbFacadeTransactions.cpp
 * @brief Facade batch, transaction, and composite-key contract tests.
 */

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "DbTestUtils.h"
#include "edadb.h"

struct TxChild {
    int id = 0;
    std::string value;
};

struct TxRoot {
    int id = 0;
    std::string value;
    std::vector<TxChild> children;
};

struct TxSibling {
    int id = 0;
    std::string value;
};

TABLE4CLASS(TxChild, "facade_tx_child", (id, value));
TABLE4CLASS_WVEC(TxRoot, "facade_tx_root", (id, value), (children));
TABLE4CLASS(TxSibling, "facade_tx_sibling", (id, value));

namespace {

bool same(const TxRoot& lhs, const TxRoot& rhs) {
    if (lhs.id != rhs.id || lhs.value != rhs.value ||
            lhs.children.size() != rhs.children.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.children.size(); ++i) {
        if (lhs.children[i].id != rhs.children[i].id ||
                lhs.children[i].value != rhs.children[i].value) {
            return false;
        }
    }
    return true;
}

bool readAndMatch(const TxRoot& expected) {
    TxRoot actual;
    actual.id = expected.id;
    return edadb::readByPrimaryKey(&actual) == 1 && same(actual, expected);
}

bool isMissing(int id) {
    TxRoot actual;
    actual.id = id;
    return edadb::readByPrimaryKey(&actual) == 0;
}

int runBatchInsertCases(TxRoot& first, TxRoot& second) {
    std::vector<TxRoot*> pointer_batch{&first, &second};
    const std::vector<TxRoot*>& pointer_batch_ref = pointer_batch;
    if (!edadb::insertVector(pointer_batch_ref) ||
            !readAndMatch(first) || !readAndMatch(second)) {
        std::cerr << "successful pointer insertVector failed" << std::endl;
        return -1;
    }

    std::vector<TxRoot> empty_values;
    std::vector<TxRoot*> empty_pointers;
    const std::vector<TxRoot*>& empty_pointers_ref = empty_pointers;
    if (!edadb::insertVector(empty_values) ||
            !edadb::insertVector(empty_pointers_ref)) {
        std::cerr << "empty insertVector should be a successful no-op"
                  << std::endl;
        return -1;
    }

    std::vector<TxRoot> duplicate_root_batch{
        {3, "first attempt", {{1, "child"}}},
        {3, "duplicate", {{2, "child"}}}
    };
    if (edadb::insertVector(duplicate_root_batch) || !isMissing(3)) {
        std::cerr << "duplicate root batch was not rolled back" << std::endl;
        return -1;
    }

    TxRoot duplicate_child{4, "duplicate child", {
        {1, "first"}, {1, "second"}
    }};
    if (edadb::insertObject(&duplicate_child) || !isMissing(4)) {
        std::cerr << "duplicate child graph was not rolled back" << std::endl;
        return -1;
    }
    return 0;
}

int runBatchUpdateCases(TxRoot& first, TxRoot& second) {
    std::vector<TxRoot> value_updates{
        {first.id, "value update one", {{7, "same local key updated"}}},
        {second.id, "value update two", {{7, "same local key updated"}}}
    };
    if (!edadb::updateVector(value_updates) ||
            !readAndMatch(value_updates[0]) || !readAndMatch(value_updates[1])) {
        std::cerr << "value updateVector failed" << std::endl;
        return -1;
    }
    first = value_updates[0];
    second = value_updates[1];

    first.value = "pointer update one";
    second.value = "pointer update two";
    std::vector<TxRoot*> pointer_updates{&first, &second};
    const std::vector<TxRoot*>& pointer_updates_ref = pointer_updates;
    if (!edadb::updateVector(pointer_updates_ref) ||
            !readAndMatch(first) || !readAndMatch(second)) {
        std::cerr << "pointer updateVector failed" << std::endl;
        return -1;
    }

    const TxRoot before_failed_value_update = first;
    TxRoot changed = first;
    changed.value = "must roll back";
    TxRoot missing{99, "missing", {{1, "missing"}}};
    std::vector<TxRoot> failed_value_batch{changed, missing};
    if (edadb::updateVector(failed_value_batch) ||
            !readAndMatch(before_failed_value_update) || !isMissing(99)) {
        std::cerr << "failed value updateVector left partial changes"
                  << std::endl;
        return -1;
    }

    const TxRoot before_failed_pointer_update = second;
    TxRoot changed_pointer = second;
    changed_pointer.value = "must also roll back";
    std::vector<TxRoot*> failed_pointer_batch{&changed_pointer, nullptr};
    const std::vector<TxRoot*>& failed_pointer_batch_ref = failed_pointer_batch;
    if (edadb::updateVector(failed_pointer_batch_ref) ||
            !readAndMatch(before_failed_pointer_update)) {
        std::cerr << "failed pointer updateVector left partial changes"
                  << std::endl;
        return -1;
    }
    return 0;
}

int runExplicitTransactionCases() {
    TxRoot probe{5, "explicit transaction", {{5, "child"}}};
    if (!edadb::beginTransaction() ||
            !edadb::insertObject(&probe, false) ||
            !edadb::rollbackTransaction() || !isMissing(probe.id)) {
        std::cerr << "explicit rollback contract failed" << std::endl;
        return -1;
    }

    if (!edadb::beginTransaction() ||
            !edadb::insertObject(&probe, false) ||
            !edadb::commitTransaction() || !readAndMatch(probe)) {
        std::cerr << "explicit commit contract failed" << std::endl;
        return -1;
    }
    return 0;
}

int runExplicitMultiRootTransactionCase() {
    if (!edadb::createTable<TxSibling>()) {
        std::cerr << "failed to create sibling transaction table" << std::endl;
        return -1;
    }

    TxRoot root{6, "multi-root transaction", {{6, "child"}}};
    TxSibling sibling{1, "sibling"};
    TxSibling duplicate{1, "duplicate sibling"};
    if (!edadb::beginTransaction()
            || !edadb::insertObject(&root, false)
            || !edadb::insertObject(&sibling, false)
            || edadb::insertObject(&duplicate, false)
            || !edadb::rollbackTransaction()
            || !isMissing(root.id)) {
        std::cerr << "multi-root rollback contract failed" << std::endl;
        return -1;
    }

    TxSibling sibling_probe;
    sibling_probe.id = sibling.id;
    if (edadb::readByPrimaryKey(&sibling_probe) != 0) {
        std::cerr << "multi-root rollback left sibling data" << std::endl;
        return -1;
    }

    if (!edadb::beginTransaction()
            || !edadb::insertObject(&root, false)
            || !edadb::insertObject(&sibling, false)
            || !edadb::commitTransaction()
            || !readAndMatch(root)) {
        std::cerr << "multi-root commit contract failed" << std::endl;
        return -1;
    }

    sibling_probe = {};
    sibling_probe.id = sibling.id;
    if (edadb::readByPrimaryKey(&sibling_probe) != 1
            || sibling_probe.value != sibling.value) {
        std::cerr << "multi-root sibling commit failed" << std::endl;
        return -1;
    }
    return 0;
}

int runExplicitSchemaTransactionCases() {
    const auto* root_def = edadb::getTableDef<TxRoot>();
    if (root_def == nullptr || root_def->getChildCount() != 1) {
        std::cerr << "unexpected schema transaction table definition" << std::endl;
        return -1;
    }
    const auto* child_def = root_def->getChild(0);

    if (!edadb::beginTransaction()
            || !edadb::createTable<TxRoot>(false)
            || edadb::executeSql("CREATE TABLE")
            || !edadb::rollbackTransaction()
            || edadb::tableExists(root_def->getTableName())
            || edadb::tableExists(child_def->getTableName())) {
        std::cerr << "explicit schema rollback contract failed" << std::endl;
        return -1;
    }

    if (!edadb::beginTransaction()
            || !edadb::createTable<TxRoot>(false)
            || !edadb::commitTransaction()
            || !edadb::tableExists(root_def->getTableName())
            || !edadb::tableExists(child_def->getTableName())) {
        std::cerr << "explicit schema commit contract failed" << std::endl;
        return -1;
    }
    return 0;
}

int runReusableReadAllCase() {
    auto reader = edadb::makeReadAllOp<TxRoot>();
    int count = 0;
    while (true) {
        TxRoot row;
        const int rc = edadb::readNext(reader, &row);
        if (rc < 0) {
            std::cerr << "reusable READ_ALL failed" << std::endl;
            return -1;
        }
        if (rc == 0) {
            break;
        }
        ++count;
    }
    if (count != 4 || edadb::readNext(reader, static_cast<TxRoot*>(nullptr)) >= 0) {
        std::cerr << "reusable READ_ALL state/validation failed" << std::endl;
        return -1;
    }
    return 0;
}

} // namespace

int main() {
    const std::string db_path = testOutputPath("DbFacadeTransactions_test.sqlite3");
    std::remove(db_path.c_str());
    if (!edadb::initDatabase(db_path)) {
        return -1;
    }

    if (runExplicitSchemaTransactionCases() < 0) {
        return -1;
    }

    auto create_op = edadb::makeCreateTableOp<TxRoot>();
    if (create_op.exec() < 0 || create_op.exec() < 0) {
        std::cerr << "reusable/idempotent CREATE failed" << std::endl;
        return -1;
    }

    // Child local id 7 is valid under both parents because the physical PK is
    // the ancestor-first pair (root id, child id).
    TxRoot first{1, "one", {{7, "first parent"}}};
    TxRoot second{2, "two", {{7, "second parent"}}};
    if (runBatchInsertCases(first, second) < 0 ||
        runBatchUpdateCases(first, second) < 0 ||
        runExplicitTransactionCases() < 0 ||
        runExplicitMultiRootTransactionCase() < 0 ||
        runReusableReadAllCase() < 0) {
        return -1;
    }

    if (validateSqliteForeignKeyContent() < 0) {
        return -1;
    }
    auto drop_op = edadb::makeDropTableOp<TxRoot>();
    auto drop_sibling_op = edadb::makeDropTableOp<TxSibling>();
    if (drop_op.exec() < 0 || drop_op.exec() < 0 ||
            drop_sibling_op.exec() < 0 ||
            !edadb::closeDatabase()) {
        std::cerr << "reusable/idempotent DROP or close failed" << std::endl;
        return -1;
    }
    return 0;
}
