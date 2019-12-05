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
#include "envoy/http/header_map.h"
#include "common/router/string_accessor_impl.h"

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
    if(map_.find(key) != map_.end()) {
        value = map_[key];
    }
    return value;
}

std::string view_to_string(absl::string_view view) {
    return std::string(view.data(), view.length());
}

Http::FilterHeadersStatus DataTracingFilter::decodeHeaders(Http::HeaderMap &headers, bool) {

    const HeaderEntry* request_entry = headers.get(Envoy::Http::LowerCaseString("x-request-id"));
    if (request_entry != nullptr) {
        std::string connection_id = std::to_string(decoder_callbacks_->connection()->id());
        ENVOY_LOG(warn, "DataTracing:OnRequest:Received with x-request-id {} and connection {}",
                  view_to_string(request_entry->value().getStringView()), connection_id);

        // Global mapping from trace / request ID to parent connection
        // only puts if no entry for the trace already exists
        map_->create("PARENT-" + view_to_string(request_entry->value().getStringView()), connection_id);

        // Stream local mapping from connection ID to trace ID
        encoder_callbacks_->streamInfo().filterState().setData(connection_id,
                std::make_unique<Router::StringAccessorImpl>(request_entry->value().getStringView()),
                StreamInfo::FilterState::StateType::Mutable);

        const HeaderEntry* data_entry = headers.get(Envoy::Http::LowerCaseString("x-data"));
        if (data_entry != nullptr) {
            ENVOY_LOG(warn, "DataTracing:OnRequest: x-request-id {} has data {}",
                      view_to_string(request_entry->value().getStringView()),
                      view_to_string(data_entry->value().getStringView()));

            // Save global mapping from trace ID to data label
            map_->put("DATA-" + view_to_string(request_entry->value().getStringView()),
                    view_to_string(data_entry->value().getStringView()));

        } else {
            ENVOY_LOG(warn, "DataTracing:OnRequest: x-request-id {} has no data", connection_id);

            // Load existing data labels for trace from global map
            // For the case where the user has not propagated the x-data header
            std::string data = std::string(map_->get(view_to_string(request_entry->value().getStringView())));
            if (data.length()) {
                headers.addCopy(Envoy::Http::LowerCaseString("x-data"), data);
            }
        }
    } else {
        ENVOY_LOG(warn, "DataTracing:OnRequest:Skipped an HTTP request with no x-request-id");
    }
    return Http::FilterHeadersStatus::Continue;
}

Http::FilterHeadersStatus DataTracingFilter::encodeHeaders(Http::HeaderMap &headers, bool) {
    std::string connection_id = std::to_string(decoder_callbacks_->connection()->id());
    if (!encoder_callbacks_->streamInfo().filterState().hasData<Router::StringAccessorImpl>(connection_id)) {
        // There is no trace associated with this connection
        ENVOY_LOG(warn, "DataTracing:OnResponse:Received an HTTP response with no x-request-id and connection {}",
                  connection_id);
        return Http::FilterHeadersStatus::Continue;
    }
    std::string trace_id = view_to_string(encoder_callbacks_->streamInfo().filterState()
            .getDataReadOnly<Router::StringAccessorImpl>(connection_id).asString());
    bool is_parent = map_->get("PARENT-" + trace_id) == connection_id;
    const HeaderEntry* data_entry = headers.get(Envoy::Http::LowerCaseString("x-data"));

    if (is_parent) {
        ENVOY_LOG(warn, "DataTracing:OnResponse:Received parent with x-request-id {} and connection {}",
                  trace_id, connection_id);

        // This is a response to the initial parent HTTP request
        if (data_entry != nullptr) {
            // The parent response already has a data label, so the responder is overriding
            // with internal label management.
            ENVOY_LOG(warn, "DataTracing:OnResponse: x-request-id {} is overriding the x-data entry",
                    trace_id);
        } else {
            // The parent response does not have data tagged, so we will tag it if its been set
            std::string data = map_->get("DATA-" + trace_id);
            if (data.length()) {
                ENVOY_LOG(warn, "DataTracing:OnResponse: x-request-id {} is being tagged with data",

                        trace_id);
                headers.addCopy(Envoy::Http::LowerCaseString("x-data"), data);
            } else {
                ENVOY_LOG(warn, "DataTracing:OnResponse: x-request-id {} has no data to tag",
                        trace_id);
            }
        }

        // Garbage collect all KVs associated with the trace, as its over
        map_->del("DATA-" + trace_id);
        map_->del("PARENT-" + trace_id);
    } else {
        // This is a response to an outbound request
        ENVOY_LOG(warn, "DataTracing:OnResponse:Received child with x-request-id {} and connection {}",
                  trace_id, connection_id);
        if (data_entry != nullptr) {
            // A data label is connected to this outbound response, so we should save it
            // No chance for memory leak because update is only performed iff the trace exists
            ENVOY_LOG(warn, "DataTracing:OnResponse: x-request-id {} has data to save", trace_id);
            map_->update("DATA-" + trace_id, view_to_string(data_entry->value().getStringView()));
        } else {
            // There is no data label connected with this outbound response
            // Therefore there is nothing to save for the current trace
            ENVOY_LOG(warn, "DataTracing:OnResponse: x-request-id {} has no data to save", trace_id);
        }
    }
    return Http::FilterHeadersStatus::Continue;
}

}  // namespace Data
}  // namespace Http
}  // namespace Envoy
