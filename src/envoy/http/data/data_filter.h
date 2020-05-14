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
#include "src/istio/data/data_filter.pb.h"

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
    };

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

    DataPolicyResults* apply_policy_functions(std::string data_contents, data::FilterConfig_When when);
};

}  // namespace Data
}  // namespace Http
}  // namespace Envoy
