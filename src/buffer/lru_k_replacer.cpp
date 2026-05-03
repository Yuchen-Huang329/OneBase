#include "onebase/buffer/lru_k_replacer.h"
#include "onebase/common/exception.h"
#include <limits>

namespace onebase {

LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k)
    : max_frames_(num_frames), k_(k) {}

auto LRUKReplacer::Evict(frame_id_t *frame_id) -> bool {
  // TODO(student): Implement LRU-K eviction policy
  // - Find the frame with the largest backward k-distance
  // - Among frames with fewer than k accesses, evict the one with earliest first access
  // - Only consider evictable frames
    std::lock_guard<std::mutex> guard(latch_);

  if (frame_id == nullptr || curr_size_ == 0) {
    return false;
  }

  bool found_inf = false;
  bool found_finite = false;

  frame_id_t victim = INVALID_FRAME_ID;

  size_t earliest_first_access = std::numeric_limits<size_t>::max();
  size_t max_k_distance = 0;
  size_t victim_kth_timestamp = std::numeric_limits<size_t>::max();

  for (const auto &[fid, entry] : entries_) {
    if (!entry.is_evictable_) {
      continue;
    }

    if (entry.history_.size() < k_) {
      size_t first_access = entry.history_.front();
      if (!found_inf || first_access < earliest_first_access) {
        found_inf = true;
        earliest_first_access = first_access;
        victim = fid;
      }
      continue;
    }

    if (!found_inf) {
      size_t kth_timestamp = entry.history_.front();
      size_t k_distance = current_timestamp_ - kth_timestamp;

      if (!found_finite ||
          k_distance > max_k_distance ||
          (k_distance == max_k_distance && kth_timestamp < victim_kth_timestamp)) {
        found_finite = true;
        max_k_distance = k_distance;
        victim_kth_timestamp = kth_timestamp;
        victim = fid;
      }
    }
  }

  if (victim == INVALID_FRAME_ID) {
    return false;
  }

  entries_.erase(victim);
  curr_size_--;
  *frame_id = victim;
  return true;
}

void LRUKReplacer::RecordAccess(frame_id_t frame_id) {
  // TODO(student): Record a new access for frame_id at current timestamp
  // - If frame_id is new, create an entry
  // - Add current_timestamp_ to the frame's history
  // - Increment current_timestamp_
  std::lock_guard<std::mutex> guard(latch_);

  if (frame_id < 0 || static_cast<size_t>(frame_id) >= max_frames_) {
    throw OneBaseException("frame_id out of range", ExceptionType::OUT_OF_RANGE);
  }

  auto &entry = entries_[frame_id];
  entry.history_.push_back(current_timestamp_);

  if (entry.history_.size() > k_) {
    entry.history_.pop_front();
  }

  current_timestamp_++;
}

void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  // TODO(student): Set whether a frame is evictable
  // - Update curr_size_ accordingly
  std::lock_guard<std::mutex> guard(latch_);

  auto it = entries_.find(frame_id);
  if (it == entries_.end()) {
    return;
  }

  if (it->second.is_evictable_ != set_evictable) {
    if (set_evictable) {
      curr_size_++;
    } else {
      curr_size_--;
    }
    it->second.is_evictable_ = set_evictable;
  }
}

void LRUKReplacer::Remove(frame_id_t frame_id) {
  // TODO(student): Remove a frame from the replacer
  // - The frame must be evictable; throw if not
  std::lock_guard<std::mutex> guard(latch_);

  auto it = entries_.find(frame_id);
  if (it == entries_.end()) {
    return;
  }

  if (!it->second.is_evictable_) {
    throw OneBaseException("cannot remove a non-evictable frame", ExceptionType::INVALID);
  }

  entries_.erase(it);
  curr_size_--;
}

auto LRUKReplacer::Size() const -> size_t {
  // TODO(student): Return the number of evictable frames
  return curr_size_;
}

}  // namespace onebase
