#include "onebase/execution/executors/update_executor.h"
#include "onebase/common/exception.h"
#include "onebase/type/type_id.h"
#include "onebase/type/value.h"

namespace onebase {

UpdateExecutor::UpdateExecutor(ExecutorContext *exec_ctx, const UpdatePlanNode *plan,
                               std::unique_ptr<AbstractExecutor> child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void UpdateExecutor::Init() {
  // TODO(student): Initialize child executor
  if (child_executor_ != nullptr) {
    child_executor_->Init();
  }
  has_updated_ = false;
}

auto UpdateExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // TODO(student): Update tuples using update expressions
  // - Get tuples from child, evaluate update expressions, update table_heap
  // - Return count of updated rows
  if (has_updated_) {
    return false;
  }

  auto *catalog = GetExecutorContext()->GetCatalog();
  auto *table_info = catalog->GetTable(plan_->GetTableOid());
  if (table_info == nullptr) {
    throw OneBaseException("UpdateExecutor: table not found", ExceptionType::INVALID);
  }
  auto indexes = catalog->GetTableIndexes(table_info->name_);

  int32_t update_count = 0;
  Tuple child_tuple;
  RID child_rid;

  const auto &update_exprs = plan_->GetUpdateExpressions();

  while (child_executor_->Next(&child_tuple, &child_rid)) {
    std::vector<Value> new_values;
    new_values.reserve(update_exprs.size());

    for (const auto &expr : update_exprs) {
      new_values.push_back(expr->Evaluate(&child_tuple, &table_info->schema_));
    }

    Tuple new_tuple(std::move(new_values));

    for (auto *index_info : indexes) {
      if (index_info == nullptr || index_info->index_ == nullptr || index_info->key_attrs_.empty()) {
        continue;
      }
      uint32_t key_attr = index_info->key_attrs_[0];
      int old_key = child_tuple.GetValue(&table_info->schema_, key_attr).GetAsInteger();
      index_info->RemoveEntry(old_key, child_rid);
      const auto *remaining_old = index_info->LookupInteger(old_key);
      if (remaining_old == nullptr || remaining_old->empty()) {
        index_info->index_->Remove(old_key);
      }
    }

    if (table_info->table_->UpdateTuple(child_rid, new_tuple)) {
      for (auto *index_info : indexes) {
        if (index_info == nullptr || index_info->index_ == nullptr || index_info->key_attrs_.empty()) {
          continue;
        }
        uint32_t key_attr = index_info->key_attrs_[0];
        int new_key = new_tuple.GetValue(&table_info->schema_, key_attr).GetAsInteger();
        index_info->InsertEntry(new_key, child_rid);
        index_info->index_->Insert(new_key, child_rid);
      }

      update_count++;
    }
  }

  *tuple = Tuple({Value(TypeId::INTEGER, update_count)});
  if (rid != nullptr) {
    *rid = RID{};
  }

  has_updated_ = true;
  return true;
}

}  // namespace onebase
