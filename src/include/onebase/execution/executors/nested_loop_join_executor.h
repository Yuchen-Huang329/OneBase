#pragma once
#include <memory>
#include "onebase/execution/executors/abstract_executor.h"
#include "onebase/execution/plans/plan_nodes.h"
#include "onebase/storage/table/tuple.h"

namespace onebase {

class NestedLoopJoinExecutor : public AbstractExecutor {
 public:
  NestedLoopJoinExecutor(ExecutorContext *exec_ctx, const NestedLoopJoinPlanNode *plan,
                         std::unique_ptr<AbstractExecutor> left_executor,
                         std::unique_ptr<AbstractExecutor> right_executor);
  void Init() override;
  auto Next(Tuple *tuple, RID *rid) -> bool override;
  auto GetOutputSchema() const -> const Schema & override { return plan_->GetOutputSchema(); }

 private:
  const NestedLoopJoinPlanNode *plan_;
  std::unique_ptr<AbstractExecutor> left_executor_;
  std::unique_ptr<AbstractExecutor> right_executor_;
  bool has_left_tuple_{false};
  Tuple left_tuple_{};
  RID left_rid_{};
};

}  // namespace onebase
