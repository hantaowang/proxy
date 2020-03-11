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

#pragma once

#include "common/router/string_accessor_impl.h"

namespace Envoy {
namespace Http {
namespace Data {

class ThreadSafeStringMap {
  public:
    ThreadSafeStringMap() {};
    std::string get(std::string key);

    // Sets a Key Value pair
    void put(std::string key, std::string value);

    // Performs a put iff the key exists
    bool update(std::string key, std::string value);

    // Performs a put iff the key doesnt exist
    bool create(std::string key, std::string value);

    // Deletes an existing key
    bool del(std::string key);

  private:
    std::string _unsafe_get(std::string key);
    std::map<std::string, std::string> map_;
    std::mutex m_;
};

}  // namespace Data
}  // namespace Http
}  // namespace Envoy
