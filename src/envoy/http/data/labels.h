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

const std::string DELIM = ";";
const std::string DEFAULT_NO_DATA = "__NONE__";

class LabelSet {
public:
    LabelSet(std::string delim) {
        _delim = delim;
    };

    LabelSet(std::string str, std::string delim) {
        _delim = delim;
        size_t pos = 0;
        std::string s = str;
        std::string token;
        while ((pos = s.find(delim)) != std::string::npos) {
            token = s.substr(0, pos);
            if (token != DEFAULT_NO_DATA) {
                _labels.insert(token);
            }
            s.erase(0, pos + delim.length());
        }
        if (s.length() > 0 && s != DEFAULT_NO_DATA) {
            _labels.insert(s);
        }
    };

    bool contains(std::string key) {
        return _labels.find(key) != _labels.end();
    };

    void put(std::string key) {
        _labels.insert(key);
    };

    bool remove(std::string key) {
        std::set<std::string>::iterator it = _labels.find(key);
        if (it == _labels.end()) {
            return false;
        }
        _labels.erase(it);
        return true;
    };

    int size() {
        return _labels.size();
    };

    std::string toString() {
        std::set<std::string>::iterator it;
        std::string ret = "";
        int count = 1;
        for (it = _labels.begin(); it != _labels.end(); ++it)
        {
            ret += *it;
            if (count < size()) {
                ret += _delim;
                count++;
            }
        }
        return ret;
    }

private:
    std::set<std::string> _labels;
    std::string _delim;
};

}  // namespace Data
}  // namespace Http
}  // namespace Envoy