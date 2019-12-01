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

#include "src/envoy/http/data/data_filter.h"
#include "common/network/application_protocol.h"
#include "envoy/upstream/cluster_manager.h"
#include "envoy/network/connection.h"
#include "common/router/string_accessor_impl.h"

namespace Envoy {
namespace Http {
namespace Data {

std::string StringMap::get(std::string key) {
    m_.lock();
    std::string value = "";
    if(map_.find(key) != map_.end()) {
        value = map_[key];
    }
    m_.unlock();
    return value;
}

void StringMap::put(std::string key, std::string value) {
    m_.lock();
    map_[key] = value;
    m_.unlock();
}

bool StringMap::del(std::string key) {
    m_.lock();
    bool found = false;
    if(map_.find(key) != map_.end()) {
        map_.erase(key);
        found = true;
    }
    m_.unlock();
    return found;
}

Http::FilterHeadersStatus DataTracingFilter::decodeHeaders(Http::HeaderMap &, bool) {
    std::string connection_id = std::to_string(decoder_callbacks_->connection()->id());
    ENVOY_LOG(warn, "DataTracing::OnRequest::Connection::{}", connection_id);
    decoder_callbacks_->streamInfo().filterState().setData(connection_id,
            std::make_unique<Router::StringAccessorImpl>(connection_id),
            StreamInfo::FilterState::StateType::Mutable);
    ENVOY_LOG(warn, "DataTracing::OnRequest::SetFilterState");

    ENVOY_LOG(warn, "DataTracing::OnRequest::GetGlobal::{}", map_->get("key"));
    map_->put("key", "value");
    return Http::FilterHeadersStatus::Continue;
}

Http::FilterHeadersStatus DataTracingFilter::encodeHeaders(Http::HeaderMap &, bool) {
    std::string connection_id = std::to_string(encoder_callbacks_->connection()->id());
    ENVOY_LOG(warn, "DataTracing::OnResponse::Connection::{}", connection_id);
    if (encoder_callbacks_->streamInfo().filterState().hasData<Router::StringAccessorImpl>(connection_id)) {
        ENVOY_LOG(warn, "DataTracing::OnResponse::Hit");
    } else {
        ENVOY_LOG(warn, "DataTracing::OnResponse::Miss");
    }
    ENVOY_LOG(warn, "DataTracing::OnResponse::GetGlobal::{}", map_->get("key"));
    return Http::FilterHeadersStatus::Continue;
}

}  // namespace Data
}  // namespace Http
}  // namespace Envoy
