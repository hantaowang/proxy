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

#include "extensions/filters/http/common/pass_through_filter.h"
#include "src/envoy/http/data/map.h"
#include "src/envoy/http/data/labels.h"
#include "src/istio/data/data_filter.pb.h"
#include "envoy/http/header_map.h"
#include "envoy/network/connection.h"

namespace Envoy {
namespace Http {
namespace Data {

class DataTracingFilterConfig {
public:
    DataTracingFilterConfig(const data::FilterConfig &proto_config) {
        _size = proto_config.actions_size();
        _operations = new data::FilterConfig_Operation[_size];
        _whens = new data::FilterConfig_When[_size];
        _members = new std::string[_size];
        for (int i = 0; i < _size; i++) {
            _operations[i] = proto_config.actions(i).operation();
            _whens[i] = proto_config.actions(i).when();
            _members[i] = proto_config.actions(i).member();
        }
    }

    DataTracingFilterConfig(std::string overrides) {
        _size = std::count(overrides.begin(), overrides.end(), ';') + 1;
        _operations = new data::FilterConfig_Operation[_size];
        _whens = new data::FilterConfig_When[_size];
        _members = new std::string[_size];
        size_t pos = 0;
        int count = 0;
        std::string s = overrides;
        std::string token;
        std::string *_opStrings = new std::string[_size];
        while ((pos = s.find(DELIM)) != std::string::npos) {
            token = s.substr(0, pos);
            _opStrings[count] = token.substr(0, token.find("("));
            _members[count] = token.substr(token.find("(") + 1, token.length() - 1);
            count++;
        }
        _opStrings[count] = s.substr(0, s.find("("));
        _members[count] = s.substr(s.find("(") + 1, s.length() - 1);
        for (int i = 0; i < _size; i++) {
            if (_opStrings[i] == "ADD") {
                _operations[i] = data::FilterConfig::ADD;
            } else if (_opStrings[i] == "REMOVE") {
                _operations[i] = data::FilterConfig::REMOVE;
            } else if (_opStrings[i] == "CHECK_INCLUDE") {
                _operations[i] = data::FilterConfig::CHECK_INCLUDE;
            } else if (_opStrings[i] == "CHECK_EXCLUDE") {
                _operations[i] =  data::FilterConfig::CHECK_EXCLUDE;
            }
        }
    }

    data::FilterConfig_Operation getOperation(int i) {
        return _operations[i];
    }

    data::FilterConfig_When getWhen(int i) {
        return _whens[i];
    }

    std::string getMember(int i) {
        return _members[i];
    }

    int size() {
        return _size;
    }

private:
    data::FilterConfig_Operation *_operations;
    data::FilterConfig_When *_whens;
    std::string *_members;
    int _size;

};

using ThreadSafeStringMapSharedPtr = std::shared_ptr<ThreadSafeStringMap>;

using DataTracingFilterConfigSharedPtr = std::shared_ptr<DataTracingFilterConfig>;

class DataPolicyResults {

public:
    Http::FilterHeadersStatus status;
    std::string data;
};

class DataTracingFilter : public Http::PassThroughFilter,
                   Logger::Loggable<Logger::Id::filter> {
 public:
    DataTracingFilter(const DataTracingFilterConfigSharedPtr &config, const ThreadSafeStringMapSharedPtr &map)
            : map_(map), config_(config) {};

    // Http::PassThroughDecoderFilter
    Http::FilterHeadersStatus decodeHeaders(Http::HeaderMap &headers, bool end_stream) override;

    void setDecoderFilterCallbacks(Http::StreamDecoderFilterCallbacks& callbacks) override {
        decoder_callbacks_ = &callbacks;
    };

    // Http::PassThroughEncoderFilter
    Http::FilterHeadersStatus encodeHeaders(Http::HeaderMap &headers, bool end_stream) override;

    void setEncoderFilterCallbacks(Http::StreamEncoderFilterCallbacks& callbacks) override {
        encoder_callbacks_ = &callbacks;
    };

    const ThreadSafeStringMapSharedPtr map_;
    const DataTracingFilterConfigSharedPtr &config_;

  private:
    Http::StreamDecoderFilterCallbacks* decoder_callbacks_{};
    Http::StreamEncoderFilterCallbacks* encoder_callbacks_{};

    DataPolicyResults* apply_policy_functions(std::string data_contents, data::FilterConfig_When when, std::string overrides);
    Http::FilterHeadersStatus apply_function(LabelSet *l, data::FilterConfig_Operation op, std::string member);

};

}  // namespace Data
}  // namespace Http
}  // namespace Envoy
