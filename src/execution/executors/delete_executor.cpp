#include "onebase/execution/executors/delete_executor.h"
#include "onebase/common/exception.h"
#include "onebase/type/type_id.h"
#include "onebase/type/value.h"


namespace onebase {

DeleteExecutor::DeleteExecutor(ExecutorContext *exec_ctx, const DeletePlanNode *plan,
                               std::unique_ptr<AbstractExecutor> child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void DeleteExecutor::Init() {
  // TODO(student): Initialize child executor
  if (child_executor_ != nullptr) {
    child_executor_->Init();
  }
  has_deleted_ = false;
}

auto DeleteExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // TODO(student): Delete tuples identified by child executor
  // - Get tuples from child, delete from table_heap
  // - Update any indexes
  // - Return count of deleted rows
  if (has_deleted_) {
    return false;
  }

  auto *catalog = GetExecutorContext()->GetCatalog();
  auto *table_info = catalog->GetTable(plan_->GetTableOid());
  if (table_info == nullptr) {
    throw OneBaseException("DeleteExecutor: table not found", ExceptionType::INVALID);
  }
  auto indexes = catalog->GetTableIndexes(table_info->name_);

  int32_t delete_count = 0;
  Tuple child_tuple;
  RID child_rid;

  while (child_executor_->Next(&child_tuple, &child_rid)) {
    for (auto *index_info : indexes) {
      if (index_info == nullptr || index_info->index_ == nullptr || index_info->key_attrs_.empty()) {
        continue;
      }
      uint32_t key_attr = index_info->key_attrs_[0];
      int key = child_tuple.GetValue(&table_info->schema_, key_attr).GetAsInteger();
      index_info->RemoveEntry(key, child_rid);
      const auto *remaining = index_info->LookupInteger(key);
      if (remaining == nullptr || remaining->empty()) {
        index_info->index_->Remove(key);
      }
    }

    table_info->table_->DeleteTuple(child_rid);
    delete_count++;
  }

  *tuple = Tuple({Value(TypeId::INTEGER, delete_count)});
  if (rid != nullptr) {
    *rid = RID{};
  }

  has_deleted_ = true;
  return true;
}

}  // namespace onebase
