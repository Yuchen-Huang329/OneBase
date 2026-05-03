#include "onebase/execution/executors/nested_loop_join_executor.h"
#include "onebase/common/exception.h"

namespace onebase {

NestedLoopJoinExecutor::NestedLoopJoinExecutor(ExecutorContext *exec_ctx,
                                                const NestedLoopJoinPlanNode *plan,
                                                std::unique_ptr<AbstractExecutor> left_executor,
                                                std::unique_ptr<AbstractExecutor> right_executor)
    : AbstractExecutor(exec_ctx), plan_(plan),
      left_executor_(std::move(left_executor)), right_executor_(std::move(right_executor)) {}

void NestedLoopJoinExecutor::Init() {
  // TODO(student): Initialize both child executors
  left_executor_->Init();
  right_executor_->Init();

  has_left_tuple_ = left_executor_->Next(&left_tuple_, &left_rid_);
}

auto NestedLoopJoinExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // TODO(student): Perform nested loop join
  // - For each left tuple, scan all right tuples
  // - Evaluate predicate on (left, right) pairs
  // - Output matching combined tuples
  Tuple right_tuple;
  RID right_rid;

  while (has_left_tuple_) {
    while (right_executor_->Next(&right_tuple, &right_rid)) {
      bool matched = true;
      const auto &predicate = plan_->GetPredicate();

      if (predicate != nullptr) {
        Value pred_val = predicate->EvaluateJoin(
            &left_tuple_, &left_executor_->GetOutputSchema(),
            &right_tuple, &right_executor_->GetOutputSchema());
        matched = pred_val.GetAsBoolean();
      }

      if (!matched) {
        continue;
      }

      const auto &left_schema = left_executor_->GetOutputSchema();
      const auto &right_schema = right_executor_->GetOutputSchema();

      std::vector<Value> output_values;
      output_values.reserve(left_schema.GetColumnCount() + right_schema.GetColumnCount());

      for (uint32_t i = 0; i < left_schema.GetColumnCount(); i++) {
        output_values.push_back(left_tuple_.GetValue(&left_schema, i));
      }
      for (uint32_t i = 0; i < right_schema.GetColumnCount(); i++) {
        output_values.push_back(right_tuple.GetValue(&right_schema, i));
      }

      *tuple = Tuple(output_values);
      if (rid != nullptr) {
        *rid = RID{};
      }
      return true;
    }

    has_left_tuple_ = left_executor_->Next(&left_tuple_, &left_rid_);
    if (has_left_tuple_) {
      right_executor_->Init();
    }
  }

  return false;
}

}  // namespace onebase
