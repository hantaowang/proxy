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

DataPolicyResults* DataTracingFilter::apply_policy_functions(std::string data_contents, data::FilterConfig_When when, std::string overrides) {
    LabelSet *l = new LabelSet(data_contents, DELIM);
    DataPolicyResults *results = new DataPolicyResults{};
    ENVOY_LOG(warn, "Config has size {}", config_->size());
    for (int i = 0; i < config_->size(); i++) {
        ENVOY_LOG(warn, "Operation: {}, Member: {}", config_->getOperation(i), config_->getMember(i));
        if (config_->getWhen(i) == when) {
            if (apply_function(l, config_->getOperation(i), config_->getMember(i))
                    == Http::FilterHeadersStatus::StopIteration) {
                results->status = Http::FilterHeadersStatus::StopIteration;
                delete l;
                return results;
            }
        }
    }
    if (overrides.length() > 0) {
        DataTracingFilterConfig dfov = DataTracingFilterConfig(overrides);
        for (int i = 0; i < dfov.size(); i++) {
            ENVOY_LOG(warn, "Applying Override Operation: {}, Member: {}", dfov.getOperation(i), dfov.getMember(i));
            if (apply_function(l, dfov.getOperation(i), dfov.getMember(i))
                    == Http::FilterHeadersStatus::StopIteration) {
                results->status = Http::FilterHeadersStatus::StopIteration;
                delete l;
                return results;
            }
        }
    }

    if (l->size() == 0) {
        results->data =  DEFAULT_NO_DATA;
    } else {
        results->data = l->toString();
    }
    delete l;
    results->status = Http::FilterHeadersStatus::Continue;
    return results;
}

Http::FilterHeadersStatus DataTracingFilter::apply_function(LabelSet *l, data::FilterConfig_Operation op, std::string member) {
    switch (op) {
        case data::FilterConfig::ADD:
            l->put(member);
            break;
        case data::FilterConfig::REMOVE:
            l->remove(member);
            break;
        case data::FilterConfig::CHECK_INCLUDE:
            if (!l->contains(member)) {
                return Http::FilterHeadersStatus::StopIteration;
            }
            break;
        case data::FilterConfig::CHECK_EXCLUDE:
            if (l->contains(member)) {
                return Http::FilterHeadersStatus::StopIteration;
            }
            break;
        default:
            break;
    }
    return Http::FilterHeadersStatus::Continue;
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
        bool new_mapping = map_->create("DATA-" + request_id, DEFAULT_NO_DATA);

        // Set flag if this is new inbound parent request or outbound child request
        data::FilterConfig_When when = data::FilterConfig::INBOUND;
        if (!new_mapping) {
            when = data::FilterConfig::OUTBOUND;
        }

        // Stream local mapping from connection ID to trace ID
        encoder_callbacks_->streamInfo().filterState().setData(connection_id,
                std::make_unique<Router::StringAccessorImpl>(request_entry->value().getStringView()),
                StreamInfo::FilterState::StateType::Mutable);

        HeaderEntry* data_entry = headers.get(Envoy::Http::LowerCaseString("x-data"));
        std::string data_contents;
        if (data_entry != nullptr && view_to_string(data_entry->value().getStringView()).length() > 0) {
            data_contents = view_to_string(data_entry->value().getStringView());
            ENVOY_LOG(warn, "DataTracing:OnRequest: x-request-id {} has data {}",
                      request_id, data_contents);
        } else {
            ENVOY_LOG(warn, "DataTracing:OnRequest: x-request-id {} has no data", request_id);

            // Load existing data labels for trace from global map
            // For the case where the user has not propagated the x-data header
            std::string data_contents = map_->get("DATA-" + request_id);
        }

        // Find any override methods i.e. ADD(label), REMOVE(label), etc
        const HeaderEntry* override_entry = headers.get(Envoy::Http::LowerCaseString("x-data-override"));
        std::string override_methods;
        if (override_entry != nullptr) {
            override_methods = view_to_string(override_entry->value().getStringView());
            headers.remove(Envoy::Http::LowerCaseString("x-data-override"));
        }

        DataPolicyResults *results = apply_policy_functions(data_contents, when, override_methods);
        if (results->status ==  Http::FilterHeadersStatus::StopIteration) {
            decoder_callbacks_->resetStream();
            delete results;
            return Http::FilterHeadersStatus::StopIteration;
        }

        ENVOY_LOG(warn, "DataTracing:OnRequest: x-request-id {} had labels {} loaded",
                  request_id, results->data);
        headers.remove(Envoy::Http::LowerCaseString("x-data"));
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

    // Find any override methods i.e. ADD(label), REMOVE(label), etc
    const HeaderEntry* override_entry = headers.get(Envoy::Http::LowerCaseString("x-data-override"));
    std::string override_methods;
    if (override_entry != nullptr) {
        override_methods = view_to_string(override_entry->value().getStringView());
        headers.remove(Envoy::Http::LowerCaseString("x-data-override"));
    }

    if (is_parent) {
        ENVOY_LOG(warn, "DataTracing:OnResponse:Received parent with x-request-id {} and connection {}",
                  trace_id, connection_id);

        // This is a response to the initial parent HTTP request
        if (data_entry != nullptr && view_to_string(data_entry->value().getStringView()).length() > 0) {
            // The parent response already has a data label, so the responder is overriding
            // with internal label management.
            ENVOY_LOG(warn, "DataTracing:OnResponse: x-request-id {} is overriding the x-data entry",
                    trace_id);
            data_contents = view_to_string(data_entry->value().getStringView());
        } else {
            // The parent response does not have data tagged, so we will tag it if its been set
            data_contents = map_->get("DATA-" + trace_id);
        }



        DataPolicyResults *results = apply_policy_functions(data_contents, data::FilterConfig::OUTBOUND, override_methods);
        if (results->status ==  Http::FilterHeadersStatus::StopIteration) {
            encoder_callbacks_->resetStream();
            delete results;
            return Http::FilterHeadersStatus::StopIteration;
        }

        ENVOY_LOG(warn, "DataTracing:OnResponse: x-request-id {} had labels {} loaded",
                  trace_id, results->data);
        headers.remove(Envoy::Http::LowerCaseString("x-data"));
        headers.addCopy(Envoy::Http::LowerCaseString("x-data"), results->data);

        // Garbage collect all KVs associated with the trace, as its over
        map_->del("DATA-" + trace_id);
        map_->del("PARENT-" + trace_id);
    } else {
        // This is a response to an outbound request
        ENVOY_LOG(warn, "DataTracing:OnResponse:Received child with x-request-id {} and connection {}",
                  trace_id, connection_id);

        if (data_entry != nullptr && view_to_string(data_entry->value().getStringView()).length() > 0) {
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

        DataPolicyResults *results = apply_policy_functions(data_contents, data::FilterConfig::INBOUND, override_methods);
        if (results->status ==  Http::FilterHeadersStatus::StopIteration) {
            encoder_callbacks_->resetStream();
            delete results;
            return Http::FilterHeadersStatus::StopIteration;
        }

        ENVOY_LOG(warn, "DataTracing:OnResponse: x-request-id {} had labels {} loaded",
                  trace_id, results->data);
        headers.remove(Envoy::Http::LowerCaseString("x-data"));
        headers.addCopy(Envoy::Http::LowerCaseString("x-data"), results->data);
        map_->update("DATA-" + trace_id, results->data);
    }
    return Http::FilterHeadersStatus::Continue;
}

}  // namespace Data
}  // namespace Http
}  // namespace Envoy
