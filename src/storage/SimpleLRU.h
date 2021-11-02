#ifndef AFINA_STORAGE_SIMPLE_LRU_H
#define AFINA_STORAGE_SIMPLE_LRU_H

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <afina/Storage.h>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation
 * That is NOT thread safe implementaiton!!
 */
class SimpleLRU : public Afina::Storage {
  void Delete_node_from_tail();

  void Add_node_to_head(const std::string &key, const std::string &value);

  bool Move_node_to_head(const std::string &key, bool er);

 public:
  SimpleLRU(size_t max_size = 1024) : _max_size(max_size) {}

  ~SimpleLRU() {
    _lru_index.clear();
    while (_tail != _lru_head.get()) {
      _tail = _tail->prev;
      _tail->next = nullptr;
    }
    _lru_head.reset();  // TODO: Here is stack overflow
  }

  // Implements Afina::Storage interface
  bool Put(const std::string &key, const std::string &value) override;

  // Implements Afina::Storage interface
  bool PutIfAbsent(const std::string &key, const std::string &value) override;

  // Implements Afina::Storage interface
  bool Set(const std::string &key, const std::string &value) override;

  // Implements Afina::Storage interface
  bool Delete(const std::string &key) override;

  // Implements Afina::Storage interface
  bool Get(const std::string &key, std::string &value) override;

 private:
  // LRU cache node
  using lru_node = struct lru_node {
    const std::string key;
    std::string value;
    lru_node *prev;
    std::unique_ptr<lru_node> next;
  };

  // Maximum number of bytes could be stored in this cache.
  // i.e all (keys+values) must be not greater than the _max_size
  std::size_t _cur_size = 0;
  std::size_t _max_size;

  // Main storage of lru_nodes, elements in this list ordered descending by
  // "freshness": in the head
  // element that wasn't used for longest time.
  //
  // List owns all nodes
  std::unique_ptr<lru_node> _lru_head;
  // Tail
  lru_node *_tail=nullptr;

  // Index of nodes from list above, allows fast random access to elements by
  // lru_node#key
  std::map<std::reference_wrapper<const std::string>,
           std::reference_wrapper<lru_node>, std::less<std::string>> _lru_index;
};

}  // namespace Backend
}  // namespace Afina

#endif  // AFINA_STORAGE_SIMPLE_LRU_H
