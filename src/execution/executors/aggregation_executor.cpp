#include "onebase/execution/executors/aggregation_executor.h"
#include "onebase/common/exception.h"
#include "onebase/type/type_id.h"
#include "onebase/type/value.h"

namespace onebase {

AggregationExecutor::AggregationExecutor(ExecutorContext *exec_ctx, const AggregationPlanNode *plan,
                                          std::unique_ptr<AbstractExecutor> child_executor)
    : AbstractExecutor(exec_ctx), plan_(plan), child_executor_(std::move(child_executor)) {}

void AggregationExecutor::Init() {
  // TODO(student): Initialize child and build aggregation hash table
  // - Scan all tuples from child
  // - Group by group_by expressions
  // - Compute aggregates (COUNT, SUM, MIN, MAX) per group
  child_executor_->Init();
  result_tuples_.clear();
  cursor_ = 0;

  const auto &child_schema = child_executor_->GetOutputSchema();
  const auto &group_bys = plan_->GetGroupBys();
  const auto &aggregates = plan_->GetAggregates();
  const auto &agg_types = plan_->GetAggregateTypes();

  struct AggState {
    std::vector<Value> group_vals;
    std::vector<Value> agg_vals;
  };

  std::unordered_map<std::string, AggState> groups;

  auto make_group_key = [](const std::vector<Value> &vals) -> std::string {
    std::string key;
    for (const auto &v : vals) {
      if (v.IsNull()) {
        key += "NULL";
      } else {
        key += v.ToString();
      }
      key += "|";
    }
    return key;
  };

  auto init_agg_value = [](AggregationType agg_type) -> Value {
    switch (agg_type) {
      case AggregationType::CountStarAggregate:
      case AggregationType::CountAggregate:
        return Value(TypeId::INTEGER, 0);
      case AggregationType::SumAggregate:
      case AggregationType::MinAggregate:
      case AggregationType::MaxAggregate:
        return Value(TypeId::INTEGER);  // NULL integer
    }
    return Value(TypeId::INTEGER);
  };

  auto combine_agg = [](AggregationType agg_type, Value cur, const Value &input) -> Value {
    switch (agg_type) {
      case AggregationType::CountStarAggregate:
        return Value(TypeId::INTEGER, cur.GetAsInteger() + 1);

      case AggregationType::CountAggregate:
        if (!input.IsNull()) {
          return Value(TypeId::INTEGER, cur.GetAsInteger() + 1);
        }
        return cur;

      case AggregationType::SumAggregate:
        if (input.IsNull()) {
          return cur;
        }
        if (cur.IsNull()) {
          return input;
        }
        return cur.Add(input);

      case AggregationType::MinAggregate:
        if (input.IsNull()) {
          return cur;
        }
        if (cur.IsNull()) {
          return input;
        }
        return input.CompareLessThan(cur).GetAsBoolean() ? input : cur;

      case AggregationType::MaxAggregate:
        if (input.IsNull()) {
          return cur;
        }
        if (cur.IsNull()) {
          return input;
        }
        return input.CompareGreaterThan(cur).GetAsBoolean() ? input : cur;
    }
    return cur;
  };

  Tuple child_tuple;
  RID child_rid;

  while (child_executor_->Next(&child_tuple, &child_rid)) {
    std::vector<Value> group_vals;
    group_vals.reserve(group_bys.size());

    for (const auto &expr : group_bys) {
      group_vals.push_back(expr->Evaluate(&child_tuple, &child_schema));
    }

    std::string group_key = make_group_key(group_vals);

    auto it = groups.find(group_key);
    if (it == groups.end()) {
      AggState state;
      state.group_vals = group_vals;
      state.agg_vals.reserve(agg_types.size());
      for (const auto &agg_type : agg_types) {
        state.agg_vals.push_back(init_agg_value(agg_type));
      }
      it = groups.emplace(group_key, std::move(state)).first;
    }

    auto &state = it->second;

    for (size_t i = 0; i < agg_types.size(); i++) {
      Value input_val(TypeId::INTEGER);  // NULL by default
      if (agg_types[i] != AggregationType::CountStarAggregate) {
        input_val = aggregates[i]->Evaluate(&child_tuple, &child_schema);
      }
      state.agg_vals[i] = combine_agg(agg_types[i], state.agg_vals[i], input_val);
    }
  }

  // Empty input and no GROUP BY: must still return one row
  if (groups.empty() && group_bys.empty()) {
    std::vector<Value> out_vals;
    out_vals.reserve(agg_types.size());

    for (const auto &agg_type : agg_types) {
      switch (agg_type) {
        case AggregationType::CountStarAggregate:
        case AggregationType::CountAggregate:
          out_vals.emplace_back(TypeId::INTEGER, 0);
          break;
        case AggregationType::SumAggregate:
        case AggregationType::MinAggregate:
        case AggregationType::MaxAggregate:
          out_vals.emplace_back(TypeId::INTEGER);  // NULL
          break;
      }
    }

    result_tuples_.emplace_back(out_vals);
    return;
  }

  for (auto &[key, state] : groups) {
    std::vector<Value> out_vals;
    out_vals.reserve(state.group_vals.size() + state.agg_vals.size());

    for (const auto &gv : state.group_vals) {
      out_vals.push_back(gv);
    }
    for (const auto &av : state.agg_vals) {
      out_vals.push_back(av);
    }

    result_tuples_.emplace_back(out_vals);
  }
}

auto AggregationExecutor::Next(Tuple *tuple, RID *rid) -> bool {
  // TODO(student): Return next aggregation result
  if (cursor_ >= result_tuples_.size()) {
    return false;
  }

  *tuple = result_tuples_[cursor_++];
  if (rid != nullptr) {
    *rid = RID{};
  }
  return true;
}

}  // namespace onebase
