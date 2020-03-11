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
#include "envoy/network/connection.h"
#include "common/router/string_accessor_impl.h"
#include "src/envoy/http/data/map.h"
#include "src/envoy/http/data/labels.h"
#include "src/istio/data/data_filter.pb.h"

namespace Envoy {
namespace Http {
namespace Data {

std::string view_to_string(absl::string_view view) {
    return std::string(view.data(), view.length());
}

DataPolicyResults* DataTracingFilter::apply_policy_functions(std::string data_contents) {
    LabelSet l = LabelSet(data_contents, DELIM);
    DataPolicyResults *results = new DataPolicyResults{};
    ENVOY_LOG(warn, "Config has size {}", config_->size());
    for (int i = 0; i < config_->size(); i++) {
        ENVOY_LOG(warn, "Operation: {}, Member: {}", config_->getOperation(i), config_->getMember(i));
        switch (config_->getOperation(i)) {
            case data::FilterConfig::ADD:
                l.put(config_->getMember(i));
                break;
            case data::FilterConfig::REMOVE:
                l.remove(config_->getMember(i));
                break;
            case data::FilterConfig::CHECK_INCLUDE:
                if (!l.contains(config_->getMember(i))) {
                    results->status = Http::FilterHeadersStatus::StopIteration;
                    return results;
                }
                break;
            case data::FilterConfig::CHECK_EXCLUDE:
                if (l.contains(config_->getMember(i))) {
                    results->status = Http::FilterHeadersStatus::StopIteration;
                    return results;
                }
                break;
            default:
                break;
        }
    }

    if (l.size() == 0) {
        results->data =  DEFAULT_NO_DATA;
    } else {
        results->data = l.toString();
    }
    results->status = Http::FilterHeadersStatus::Continue;
    return results;
}

Http::FilterHeadersStatus DataTracingFilter::decodeHeaders(Http::HeaderMap &headers, bool) {

    const HeaderEntry* request_entry = headers.get(Envoy::Http::LowerCaseString("x-request-id"));
    if (request_entry != nullptr) {
        std::string request_id = view_to_string(request_entry->value().getStringView());
        std::string connection_id = std::to_string(decoder_callbacks_->connection()->id());
        ENVOY_LOG(warn, "DataTracing:OnRequest:Received with x-request-id {} and connection {}",
                  request_id, connection_id);

        // Global mapping from trace / request ID to parent connection
        // only puts if no entry for the trace already exists
        map_->create("PARENT-" + request_id, connection_id);

        // Mapping for the uninitialized data field
        map_->create("DATA-" + request_id, DEFAULT_NO_DATA);

        // Stream local mapping from connection ID to trace ID
        encoder_callbacks_->streamInfo().filterState().setData(connection_id,
                std::make_unique<Router::StringAccessorImpl>(request_entry->value().getStringView()),
                StreamInfo::FilterState::StateType::Mutable);

        HeaderEntry* data_entry = headers.get(Envoy::Http::LowerCaseString("x-data"));
        std::string data_contents;
        if (data_entry != nullptr) {
            data_contents = view_to_string(data_entry->value().getStringView());
            ENVOY_LOG(warn, "DataTracing:OnRequest: x-request-id {} has data {}",
                      request_id, data_contents);
        } else {
            ENVOY_LOG(warn, "DataTracing:OnRequest: x-request-id {} has no data", request_id);

            // Load existing data labels for trace from global map
            // For the case where the user has not propagated the x-data header
            std::string data_contents = map_->get("DATA-" + request_id);
        }

        DataPolicyResults *results = apply_policy_functions(data_contents);
        if (results->status ==  Http::FilterHeadersStatus::StopIteration) {
            decoder_callbacks_->resetStream();
            delete results;
            return Http::FilterHeadersStatus::StopIteration;
        }

        ENVOY_LOG(warn, "DataTracing:OnRequest: x-request-id {} had labels {} loaded",
                  request_id, results->data);
        headers.addCopy(Envoy::Http::LowerCaseString("x-data"), results->data);

        // Save global mapping from trace ID to data label
        map_->put("DATA-" + request_id, results->data);
        delete results;

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
    std::string data_contents;

    if (is_parent) {
        ENVOY_LOG(warn, "DataTracing:OnResponse:Received parent with x-request-id {} and connection {}",
                  trace_id, connection_id);

        // This is a response to the initial parent HTTP request
        if (data_entry != nullptr) {
            // The parent response already has a data label, so the responder is overriding
            // with internal label management.
            ENVOY_LOG(warn, "DataTracing:OnResponse: x-request-id {} is overriding the x-data entry",
                    trace_id);
            data_contents = view_to_string(data_entry->value().getStringView());
        } else {
            // The parent response does not have data tagged, so we will tag it if its been set
            std::string data = map_->get("DATA-" + trace_id);
        }

        DataPolicyResults *results = apply_policy_functions(data_contents);
        if (results->status ==  Http::FilterHeadersStatus::StopIteration) {
            encoder_callbacks_->resetStream();
            delete results;
            return Http::FilterHeadersStatus::StopIteration;
        }

        ENVOY_LOG(warn, "DataTracing:OnResponse: x-request-id {} had labels {} loaded",
                  trace_id, results->data);
        headers.addCopy(Envoy::Http::LowerCaseString("x-data"), results->data);

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
            data_contents = view_to_string(data_entry->value().getStringView());
            ENVOY_LOG(warn, "DataTracing:OnResponse: x-request-id {} has data to save: {}", trace_id, data_contents);

        } else {
            // There is no data label connected with this outbound response
            // Therefore there is nothing to save for the current trace
            ENVOY_LOG(warn, "DataTracing:OnResponse: x-request-id {} has no data to save", trace_id);
            data_contents = DEFAULT_NO_DATA;
        }

        DataPolicyResults *results = apply_policy_functions(data_contents);
        if (results->status ==  Http::FilterHeadersStatus::StopIteration) {
            encoder_callbacks_->resetStream();
            delete results;
            return Http::FilterHeadersStatus::StopIteration;
        }

        ENVOY_LOG(warn, "DataTracing:OnResponse: x-request-id {} had labels {} loaded",
                  trace_id, results->data);
        headers.addCopy(Envoy::Http::LowerCaseString("x-data"), results->data);

        map_->update("DATA-" + trace_id, results->data);
    }
    return Http::FilterHeadersStatus::Continue;
}

}  // namespace Data
}  // namespace Http
}  // namespace Envoy
