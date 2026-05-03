#include "onebase/execution/executors/insert_executor.h"
#include "onebase/common/exception.h"
#include "onebase/type/type_id.h"
#include "onebase/type/value.h"

namespace onebase {

InsertExecutor::InsertExecutor(ExecutorContext *exec_ctx, const InsertPlanNode *plan,
                               std::unique_ptr<AbstractExecutor> child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void InsertExecutor::Init() {
  // TODO(student): Initialize child executor
  if (child_executor_ != nullptr) {
    child_executor_->Init();
  }
  has_inserted_ = false;
}

auto InsertExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // TODO(student): Insert tuples from child into the table
  // - Get tuples from child, insert into table_heap
  // - Update any indexes
  // - Return count of inserted rows as a single integer tuple
  if (has_inserted_) {
    return false;
  }

  auto *catalog = GetExecutorContext()->GetCatalog();
  auto *table_info = catalog->GetTable(plan_->GetTableOid());
  if (table_info == nullptr) {
    throw OneBaseException("InsertExecutor: table not found", ExceptionType::INVALID);
  }
  auto indexes = catalog->GetTableIndexes(table_info->name_);

  int32_t insert_count = 0;
  Tuple child_tuple;
  RID child_rid;

  while (child_executor_->Next(&child_tuple, &child_rid)) {
    auto new_rid = table_info->table_->InsertTuple(child_tuple);
    if (!new_rid.has_value()) {
      continue;
    }

    for (auto *index_info : indexes) {
      if (index_info == nullptr || index_info->index_ == nullptr || index_info->key_attrs_.empty()) {
        continue;
      }
      uint32_t key_attr = index_info->key_attrs_[0];
      int key = child_tuple.GetValue(&table_info->schema_, key_attr).GetAsInteger();
      index_info->InsertEntry(key, *new_rid);
      index_info->index_->Insert(key, *new_rid);
    }

    insert_count++;
  }

  *tuple = Tuple({Value(TypeId::INTEGER, insert_count)});
  if (rid != nullptr) {
    *rid = RID{};
  }

  has_inserted_ = true;
  return true;
}

}  // namespace onebase
