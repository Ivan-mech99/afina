#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

void SimpleLRU::Delete_node_from_tail() {
  if (_lru_head != nullptr) {
    if (_tail != _lru_head.get()) {
      _cur_size -= ((_tail->key).size() + (_tail->value).size());
      _lru_index.erase(_tail->key);
      _tail = _tail->prev;
      _tail->next.reset(nullptr);
    } else {
      _cur_size -= ((_lru_head->key).size() + (_lru_head->value).size());
      _lru_index.erase(_lru_head->key);
      _tail = nullptr;
      _lru_head = nullptr;
    }
  }
}

void SimpleLRU::Add_node_to_head(const std::string &key,
                                 const std::string &value) {
  _cur_size += (key.size() + value.size());
  if (_lru_head.get() != nullptr) {
    lru_node *node = new lru_node{key, value, nullptr, nullptr};
    _lru_head->prev = node;
    node->next = std::move(_lru_head);
    _lru_index.insert({std::reference_wrapper<const std::string>(node->key),
                       std::reference_wrapper<lru_node>(*node)});
    _lru_head.reset(node);
  } else {
    lru_node *node = new lru_node{key, value, nullptr, nullptr};
    _lru_head.reset(node);
    _tail = node;
    _lru_index.insert({std::reference_wrapper<const std::string>(node->key),
                       std::reference_wrapper<lru_node>(*node)});
  }
}

bool SimpleLRU::Move_node_to_head(const std::string &key, bool er) {
  auto found = _lru_index.find(key);
  if (found == _lru_index.end()) {
    return false;
  }
  lru_node *node = &(found->second.get());
  if(er){
    _lru_index.erase(found);
  }
  if (node == _lru_head.get()) {
    return true;
  }
  if (node->next != nullptr) {
    std::unique_ptr<lru_node> tmp = std::move(_lru_head);
    _lru_head = std::move(node->prev->next);
    node->prev->next = std::move(node->next);
    tmp->prev = node;
    node->next = std::move(tmp);
    node->prev->next->prev = node->prev;
    node->prev = nullptr;
    return true;
  }
  std::unique_ptr<lru_node> tmp = std::move(_lru_head);
  _tail = node->prev;
  _lru_head = std::move(node->prev->next);
  tmp->prev = node;
  node->next = std::move(tmp);
  node->prev = nullptr;
  return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
  int delta;
  if (key.size() + value.size() > _max_size) {
    return false;
  }
  if (Move_node_to_head(key, false)) {
    delta = (_lru_head->value).size()- value.size();
    while (_max_size - _cur_size < delta) {
      Delete_node_from_tail();
    }
    _cur_size -= (_lru_head->value).size();
    _cur_size += value.size();
    _lru_head->value = value;
    return true;
  }
  size_t ksz = key.size();
  size_t vsz = value.size();
  while (_max_size - _cur_size < (ksz + vsz)) {
    Delete_node_from_tail();
  }
  Add_node_to_head(key, value);
  return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
  if (key.size() + value.size() > _max_size) {
    return false;
  }
  auto found = _lru_index.find(key);
  if (found == _lru_index.end()) {
    size_t ksz = key.size();
    size_t vsz = value.size();
    while (_max_size - _cur_size < (ksz + vsz)) {
      Delete_node_from_tail();
    }
    Add_node_to_head(key, value);
    return true;
  }
  return false;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
  if (key.size() + value.size() > _max_size) {
    return false;
  }
  if (Move_node_to_head(key, false)) {
    size_t sz = value.size();
    while (_max_size - _cur_size < sz) {
      Delete_node_from_tail();
    }
    _cur_size -= (_lru_head->value).size();
    _cur_size += value.size();
    _lru_head->value = value;
    return true;
  }
  return false;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
  if (!Move_node_to_head(key, true)) {
    return false;
  }
  _cur_size -= (key.size() + (_lru_head->value).size());
  if (_lru_head->next == nullptr) {
    _lru_head.reset();
    _tail = nullptr;
  } else {
    _lru_head->next->prev = nullptr;
    _lru_head = std::move(_lru_head->next);
  }
  return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
  if (Move_node_to_head(key, false)) {
    value = _lru_head->value;
    return true;
  }
  return false;
}

}  // namespace Backend
}  // namespace Afina
