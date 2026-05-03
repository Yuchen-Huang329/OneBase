#include "onebase/execution/executors/index_scan_executor.h"
#include "onebase/common/exception.h"
#include "onebase/storage/index/b_plus_tree_iterator.h"

namespace onebase {

IndexScanExecutor::IndexScanExecutor(ExecutorContext *exec_ctx, const IndexScanPlanNode *plan)
    : AbstractExecutor(exec_ctx), plan_(plan) {}

void IndexScanExecutor::Init() {
  auto *catalog = GetExecutorContext()->GetCatalog();

  table_info_ = catalog->GetTable(plan_->GetTableOid());
  if (table_info_ == nullptr) {
    throw OneBaseException("IndexScanExecutor: table not found", ExceptionType::INVALID);
  }

  index_info_ = catalog->GetIndex(plan_->GetIndexOid());
  if (index_info_ == nullptr) {
    throw OneBaseException("IndexScanExecutor: index not found", ExceptionType::INVALID);
  }

  if (index_info_->key_attrs_.empty() || index_info_->index_ == nullptr) {
    throw OneBaseException("IndexScanExecutor: invalid index", ExceptionType::INVALID);
  }

  matching_rids_.clear();
  cursor_ = 0;

  const auto &lookup_key = plan_->GetLookupKey();
  if (lookup_key != nullptr && index_info_->SupportsPointLookup()) {
    Value key_val = lookup_key->Evaluate(nullptr, nullptr);
    int key = key_val.GetAsInteger();
    const auto *vec = index_info_->LookupInteger(key);
    if (vec != nullptr) {
      matching_rids_ = *vec;
    }
    return;
  }

  if (lookup_key != nullptr) {
    Value key_val = lookup_key->Evaluate(nullptr, nullptr);
    int key = key_val.GetAsInteger();
    index_info_->index_->GetValue(key, &matching_rids_);
    return;
  }

  for (auto it = index_info_->index_->Begin(); it != index_info_->index_->End(); ++it) {
    matching_rids_.push_back((*it).second);
  }
}

auto IndexScanExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  const auto &predicate = plan_->GetPredicate();

  while (cursor_ < matching_rids_.size()) {
    RID cur_rid = matching_rids_[cursor_++];
    Tuple current_tuple = table_info_->table_->GetTuple(cur_rid);

    if (predicate != nullptr) {
      Value pred_val = predicate->Evaluate(&current_tuple, &table_info_->schema_);
      if (!pred_val.GetAsBoolean()) {
        continue;
      }
    }

    *tuple = current_tuple;
    if (rid != nullptr) {
      *rid = cur_rid;
    }
    return true;
  }

  return false;
}

}  // namespace onebase
