#include "StripedLRU.h"
#include <iostream>

namespace Afina {
namespace Backend {

bool StripedLRU::Put(const std::string &key, const std::string &value){
	return shard[hashed(key) % num_stripes]->Put(key, value);
}

bool StripedLRU::PutIfAbsent(const std::string &key, const std::string &value){
	return shard[hashed(key) % num_stripes]->PutIfAbsent(key, value);
}

bool StripedLRU::Set(const std::string &key, const std::string &value){
	return shard[hashed(key) % num_stripes]->Set(key, value);
}

bool StripedLRU::Delete(const std::string &key){
	return shard[hashed(key) % num_stripes]->Delete(key);
}

bool StripedLRU::Get(const std::string &key, std::string &value){
	return shard[hashed(key) % num_stripes]->Get(key, value);
}

StripedLRU::StripedLRU(size_t num_shards, size_t mem_lim){
        num_stripes = num_shards;
        size_t one_lim = mem_lim / num_stripes;
        for(int i = 0; i < num_stripes; i++){
                shard.emplace_back(new ThreadSafeSimplLRU(one_lim));
        }
    }


} // namespace Backend
} // namespace Afina
