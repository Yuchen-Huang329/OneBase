#include "onebase/storage/page/b_plus_tree_leaf_page.h"
#include <functional>
#include "onebase/common/exception.h"

namespace onebase {


template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_LEAF_PAGE_TYPE::Init(int max_size) {
  SetPageType(IndexPageType::LEAF_PAGE);
  SetMaxSize(max_size);
  SetSize(0);
  next_page_id_ = INVALID_PAGE_ID;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_LEAF_PAGE_TYPE::KeyAt(int index) const -> KeyType {
  return array_[index].first;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_LEAF_PAGE_TYPE::ValueAt(int index) const -> ValueType {
  return array_[index].second;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_LEAF_PAGE_TYPE::KeyIndex(const KeyType &key, const KeyComparator &comparator) const -> int {
  // TODO(student): Binary search for the index of key
  int left = 0;
  int right = GetSize();

  while (left < right) {
    int mid = left + (right - left) / 2;
    if (comparator(array_[mid].first, key)) {
      left = mid + 1;
    } else {
      right = mid;
    }
  }

  return left;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_LEAF_PAGE_TYPE::Lookup(const KeyType &key, ValueType *value,
                                         const KeyComparator &comparator) const -> bool {
  // TODO(student): Look up a key and return its associated value
  int idx = KeyIndex(key, comparator);

  if (idx < GetSize() &&
      !comparator(key, array_[idx].first) &&
      !comparator(array_[idx].first, key)) {
    *value = array_[idx].second;
    return true;
  }

  return false;
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_LEAF_PAGE_TYPE::Insert(const KeyType &key, const ValueType &value,
                                         const KeyComparator &comparator) -> int {
  // TODO(student): Insert a key-value pair in sorted order
  int idx = KeyIndex(key, comparator);

  if (idx < GetSize() &&
      !comparator(key, array_[idx].first) &&
      !comparator(array_[idx].first, key)) {
    return GetSize();
  }

  for (int i = GetSize(); i > idx; i--) {
    array_[i] = array_[i - 1];
  }

  array_[idx] = {key, value};
  IncreaseSize(1);
  return GetSize();
}

template <typename KeyType, typename ValueType, typename KeyComparator>
auto B_PLUS_TREE_LEAF_PAGE_TYPE::RemoveAndDeleteRecord(const KeyType &key,
                                                        const KeyComparator &comparator) -> int {
  // TODO(student): Remove a key-value pair
  int idx = KeyIndex(key, comparator);

  if (idx >= GetSize() ||
      comparator(key, array_[idx].first) ||
      comparator(array_[idx].first, key)) {
    return GetSize();
  }

  for (int i = idx; i < GetSize() - 1; i++) {
    array_[i] = array_[i + 1];
  }

  IncreaseSize(-1);
  return GetSize();
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_LEAF_PAGE_TYPE::MoveHalfTo(BPlusTreeLeafPage *recipient) {
  // TODO(student): Move second half of entries to recipient during split
  int split = GetSize() / 2;
  int j = 0;

  for (int i = split; i < GetSize(); i++, j++) {
    recipient->array_[j] = array_[i];
  }

  recipient->SetSize(j);
  recipient->SetNextPageId(GetNextPageId());
  SetSize(split);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_LEAF_PAGE_TYPE::MoveAllTo(BPlusTreeLeafPage *recipient) {
  // TODO(student): Move all entries to recipient during merge
  int start = recipient->GetSize();

  for (int i = 0; i < GetSize(); i++) {
    recipient->array_[start + i] = array_[i];
  }

  recipient->IncreaseSize(GetSize());
  recipient->SetNextPageId(GetNextPageId());
  SetSize(0);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_LEAF_PAGE_TYPE::MoveFirstToEndOf(BPlusTreeLeafPage *recipient) {
  // TODO(student): Move first entry to end of recipient
  int rsize = recipient->GetSize();
  recipient->array_[rsize] = array_[0];
  recipient->IncreaseSize(1);

  for (int i = 0; i < GetSize() - 1; i++) {
    array_[i] = array_[i + 1];
  }

  IncreaseSize(-1);
}

template <typename KeyType, typename ValueType, typename KeyComparator>
void B_PLUS_TREE_LEAF_PAGE_TYPE::MoveLastToFrontOf(BPlusTreeLeafPage *recipient) {
  // TODO(student): Move last entry to front of recipient
  for (int i = recipient->GetSize(); i > 0; i--) {
    recipient->array_[i] = recipient->array_[i - 1];
  }

  recipient->array_[0] = array_[GetSize() - 1];
  recipient->IncreaseSize(1);
  IncreaseSize(-1);
}
template class BPlusTreeLeafPage<int, RID, std::less<int>>;
}  // namespace onebase
