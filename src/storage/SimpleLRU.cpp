#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

bool SimpleLRU::Is_overflow(const std::string &key, const std::string &value)
{
 if (key.size() + value.size() > _max_size)
 {
  return true;
 }
 return false;
}

bool SimpleLRU::Is_not_enough(const std::string &key, const std::string &value)
{
 if (_max_size - _cur_size < (key.size() + value.size()))
 {
  return true;
 }
 return false;
}

bool SimpleLRU::Is_not_enough_for_val(const std::string &value)
{
 if (_max_size - _cur_size < (value.size()))
 {
  return true;
 }
 return false;
}

bool SimpleLRU::Is_in_dict(const std::string &key)
{
 auto found = _lru_index.find(key);
 if (found != _lru_index.end())
 {
  return true;
 }
 return false;
}

void SimpleLRU::delete_node_from_tail()
{
 if (_lru_head != nullptr)
 {
  if (_tail != _lru_head.get())
  {
   _cur_size -= ((_tail->key).size() + (_tail->value).size());
   _lru_index.erase(_tail->key);
   _tail = _tail->prev;
   _tail->next.reset(nullptr);
  }
  else
  {
   _cur_size -= ((_lru_head->key).size() + (_lru_head->value).size());
   _lru_index.erase(_lru_head->key);
   _tail = nullptr;
   _lru_head = nullptr;
  }
 }
}

void SimpleLRU::Add_node_to_head(const std::string &key, const std::string &value)
{
 _cur_size += (key.size() + value.size());
 if (_lru_head.get() != nullptr)
 {
  lru_node *node = new lru_node{ key, value, nullptr, nullptr };
  _lru_head->prev = node;
  node->next = std::move(_lru_head);
  _lru_index.insert({ std::reference_wrapper<const std::string>(node->key), std::reference_wrapper<lru_node>(*node) });
  _lru_head.reset(node);
 }
 else
 {
  lru_node *node = new lru_node{ key, value, nullptr, nullptr };
  _lru_head.reset(node);
  _tail = node;
  _lru_index.insert({ std::reference_wrapper<const std::string>(node->key),
  std::reference_wrapper<lru_node>(*node) });
 }
}

void SimpleLRU::move_node_to_head(const std::string &key)
{
 lru_node *node = &(_lru_index.find(key)->second.get());
 if (node == _lru_head.get())
 {
  return;
 }
 if (node->next != nullptr)
 {
  std::unique_ptr<lru_node> tmp = std::move(_lru_head);
  _lru_head = std::move(node->prev->next);
  node->prev->next = std::move(node->next);
  tmp->prev = node;
  node->next = std::move(tmp);
  node->prev->next->prev = node->prev;
  node->prev = nullptr;
 }
 else
 {
  std::unique_ptr<lru_node> tmp = std::move(_lru_head);
  _tail = node->prev;
  _lru_head = std::move(node->prev->next);
  tmp->prev = node;
  node->next = std::move(tmp);
  node->prev = nullptr;
 }
}

void SimpleLRU::update_key(const std::string &key, const std::string &value)
{
 lru_node *node = &(_lru_index.find(key)->second.get());
 _cur_size -= (node->value).size();
 _cur_size += value.size();
 node->value = value;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value)
{
 if (Is_overflow(key, value))
 {
  return false;
 }
 if (Is_in_dict(key))
 {
  move_node_to_head(key);
  while (Is_not_enough_for_val(value))
  {
   delete_node_from_tail();
  }
  update_key(key, value);
  return true;
 }
 else
 {
  while (Is_not_enough(key, value))
  {
   delete_node_from_tail();
  }
  Add_node_to_head(key, value);
  return true;
 }
 return false;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value)
{
 if (!Is_in_dict(key))
 {
  if (Is_overflow(key, value))
  {
   return false;
  }
  while (Is_not_enough(key, value))
  {
   delete_node_from_tail();
  }
  Add_node_to_head(key, value);
  return true;
 }
 return false;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value)
{
 if (Is_in_dict(key))
 {
  if (Is_overflow(key, value))
  {
   return false;
  }
  move_node_to_head(key);
  while (Is_not_enough_for_val(value))
  {
   delete_node_from_tail();
  }
  update_key(key, value);
  return true;
 }
 return false;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key)
{
 if (!Is_in_dict(key))
 {
  return false;
 }
 move_node_to_head(key);
 _lru_index.erase(_lru_head->key);
 _cur_size -= (key.size() + (_lru_head->value).size());
 if (_lru_head->next == nullptr)
 {
  _lru_head.reset();
  _tail = nullptr;
 }
 else
 {
  _lru_head->next->prev = nullptr;
  _lru_head = std::move(_lru_head->next);
 }
 return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value)
{
 if (Is_in_dict(key))
 {
  move_node_to_head(key);
  value = _lru_head->value;
  return true;
 }
 return false;
}

} // namespace Backend
} // namespace Afina
