/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "common/router/string_accessor_impl.h"
#include "src/envoy/http/data/map.h"

namespace Envoy {
namespace Http {
namespace Data {

std::string ThreadSafeStringMap::get(std::string key) {
    m_.lock();
    std::string value = _unsafe_get(key);
    m_.unlock();
    return value;
}

void ThreadSafeStringMap::put(std::string key, std::string value) {
    m_.lock();
    map_[key] = value;
    m_.unlock();
}

bool ThreadSafeStringMap::update(std::string key, std::string value) {
    m_.lock();
    bool updated = false;
    if (_unsafe_get(key).length() != 0) {
        map_[key] = value;
        updated = true;
    }
    m_.unlock();
    return updated;
}

bool ThreadSafeStringMap::create(std::string key, std::string value) {
    m_.lock();
    bool created = false;
    if (_unsafe_get(key).length() == 0) {
        map_[key] = value;
        created = true;
    }
    m_.unlock();
    return created;
}

bool ThreadSafeStringMap::del(std::string key) {
    m_.lock();
    bool found = false;
    if (_unsafe_get(key).length() != 0) {
        map_.erase(key);
        found = true;
    }
    m_.unlock();
    return found;
}

std::string ThreadSafeStringMap::_unsafe_get(std::string key) {
    std::string value = "";
    if (map_.find(key) != map_.end()) {
        value = map_[key];
    }
    return value;
}

}  // namespace Data
}  // namespace Http
}  // namespace Envoy