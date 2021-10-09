#ifndef AFINA_STORAGE_LRU_H
#define AFINA_STORAGE_LRU_H
#include <vector>
#include <string>
#include "ThreadSafeSimpleLRU.h"

namespace Afina {
namespace Backend {

class StripedLRU : public Afina::Storage {
public:
    StripedLRU(size_t num_shards, size_t mem_lim);

    static std::shared_ptr<StripedLRU> BuildLRU(size_t num_shards = 4, size_t mem_lim = 4*1024*1024){
        if(mem_lim/num_shards<1024*1024){
             throw std::runtime_error("Too small amout of memory");
        }
        else{
             return std::make_shared<StripedLRU>(num_shards, mem_lim);
        }
    }


    ~StripedLRU() {}

    bool Put(const std::string &key, const std::string &value) override;

    bool PutIfAbsent(const std::string &key, const std::string &value) override;

    bool Set(const std::string &key, const std::string &value) override;

    bool Delete(const std::string &key) override;

    bool Get(const std::string &key, std::string &value) override;

private:

    size_t num_stripes;
    std::vector<std::unique_ptr<ThreadSafeSimplLRU>> shard;
    std::hash<std::string> hashed;
};

} // namespace Backend
} // namespace Afina

#endif
