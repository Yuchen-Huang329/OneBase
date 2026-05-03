#include "onebase/buffer/buffer_pool_manager.h"
#include "onebase/common/exception.h"
#include "onebase/common/logger.h"

namespace onebase {

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager, size_t replacer_k)
    : pool_size_(pool_size), disk_manager_(disk_manager) {
  pages_ = new Page[pool_size_];
  replacer_ = std::make_unique<LRUKReplacer>(pool_size, replacer_k);
  for (size_t i = 0; i < pool_size_; ++i) {
    free_list_.emplace_back(static_cast<frame_id_t>(i));
  }
}

BufferPoolManager::~BufferPoolManager() { delete[] pages_; }

auto BufferPoolManager::NewPage(page_id_t *page_id) -> Page * {
  // TODO(student): Allocate a new page in the buffer pool
  // 1. Pick a victim frame from free list or replacer
  // 2. If victim is dirty, write it back to disk
  // 3. Allocate a new page_id via disk_manager_
  // 4. Update page_table_ and page metadata
  std::lock_guard<std::mutex> guard(latch_);

  frame_id_t frame_id = INVALID_FRAME_ID;

  if (!free_list_.empty()) {
    frame_id = free_list_.front();
    free_list_.pop_front();
  } else {
    if (!replacer_->Evict(&frame_id)) {
      return nullptr;
    }
  }

  Page *page = &pages_[frame_id];

  if (page->page_id_ != INVALID_PAGE_ID) {
    if (page->is_dirty_) {
      disk_manager_->WritePage(page->page_id_, page->GetData());
    }
    page_table_.erase(page->page_id_);
  }

  *page_id = disk_manager_->AllocatePage();

  page->ResetMemory();
  page->page_id_ = *page_id;
  page->pin_count_ = 1;
  page->is_dirty_ = false;

  page_table_[*page_id] = frame_id;

  replacer_->RecordAccess(frame_id);
  replacer_->SetEvictable(frame_id, false);

  return page;
}

auto BufferPoolManager::FetchPage(page_id_t page_id) -> Page * {
  // TODO(student): Fetch a page from the buffer pool
  // 1. Search page_table_ for existing mapping
  // 2. If not found, pick a victim frame
  // 3. Read page from disk into the frame
  std::lock_guard<std::mutex> guard(latch_);

  auto it = page_table_.find(page_id);
  if (it != page_table_.end()) {
    frame_id_t frame_id = it->second;
    Page *page = &pages_[frame_id];
    page->pin_count_++;
    replacer_->RecordAccess(frame_id);
    replacer_->SetEvictable(frame_id, false);
    return page;
  }

  frame_id_t frame_id = INVALID_FRAME_ID;

  if (!free_list_.empty()) {
    frame_id = free_list_.front();
    free_list_.pop_front();
  } else {
    if (!replacer_->Evict(&frame_id)) {
      return nullptr;
    }
  }

  Page *page = &pages_[frame_id];

  if (page->page_id_ != INVALID_PAGE_ID) {
    if (page->is_dirty_) {
      disk_manager_->WritePage(page->page_id_, page->GetData());
    }
    page_table_.erase(page->page_id_);
  }

  page->ResetMemory();
  disk_manager_->ReadPage(page_id, page->GetData());

  page->page_id_ = page_id;
  page->pin_count_ = 1;
  page->is_dirty_ = false;

  page_table_[page_id] = frame_id;

  replacer_->RecordAccess(frame_id);
  replacer_->SetEvictable(frame_id, false);

  return page;
}

auto BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty) -> bool {
  // TODO(student): Unpin a page, decrementing pin count
  // - If pin_count reaches 0, set evictable in replacer
  std::lock_guard<std::mutex> guard(latch_);

  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    return false;
  }

  frame_id_t frame_id = it->second;
  Page *page = &pages_[frame_id];

  if (page->pin_count_ <= 0) {
    return false;
  }

  page->pin_count_--;

  if (is_dirty) {
    page->is_dirty_ = true;
  }

  if (page->pin_count_ == 0) {
    replacer_->SetEvictable(frame_id, true);
  }

  return true;
}

auto BufferPoolManager::DeletePage(page_id_t page_id) -> bool {
  // TODO(student): Delete a page from the buffer pool
  // - Page must have pin_count == 0
  // - Remove from page_table_, reset memory, add frame to free_list_
  std::lock_guard<std::mutex> guard(latch_);

  disk_manager_->DeallocatePage(page_id);

  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    return true;
  }

  frame_id_t frame_id = it->second;
  Page *page = &pages_[frame_id];

  if (page->pin_count_ > 0) {
    return false;
  }

  replacer_->Remove(frame_id);
  page_table_.erase(it);

  page->ResetMemory();
  page->page_id_ = INVALID_PAGE_ID;
  page->pin_count_ = 0;
  page->is_dirty_ = false;

  free_list_.push_back(frame_id);

  return true;
}

auto BufferPoolManager::FlushPage(page_id_t page_id) -> bool {
  // TODO(student): Force flush a page to disk regardless of dirty flag
  std::lock_guard<std::mutex> guard(latch_);

  auto it = page_table_.find(page_id);
  if (it == page_table_.end()) {
    return false;
  }

  frame_id_t frame_id = it->second;
  Page *page = &pages_[frame_id];

  disk_manager_->WritePage(page_id, page->GetData());
  page->is_dirty_ = false;

  return true;
}

void BufferPoolManager::FlushAllPages() {
  // TODO(student): Flush all pages in the buffer pool to disk
  std::lock_guard<std::mutex> guard(latch_);

  for (auto &[page_id, frame_id] : page_table_) {
    Page *page = &pages_[frame_id];
    disk_manager_->WritePage(page_id, page->GetData());
    page->is_dirty_ = false;
  }
}

}  // namespace onebase
