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

#include "extensions/filters/http/common/factory_base.h"
#include "extensions/filters/http/common/empty_http_filter_config.h"
#include "src/envoy/utils/filter_names.h"
#include "src/envoy/http/data/data_filter.pb.h"

namespace Envoy {
namespace Http {
namespace Data {

class DataTracingFilterConfig {
public:
    DataTracingFilterConfig(const data::FilterConfig& proto_config)
        : config_(proto_config) {
        action_i = 0;
        member_i = 0;
    };

    bool hasNextOp() {
        return config_.actions_size() - 1 > action_i;
    };

    data::Operation nextOp() {
        currentOp_ = config_.actions(action_i);
        action_i += 1;
        member_i = 0;
        return currentOp_;
    };

    bool setHasNextString() {
        if (currentOp_ == nullptr) {
            return false;
        }
        return currentOp_.members_size() - 1 > member_i;
    };

    const string& setNextString() {
        if (currentOp_ == nullptr) {
            return nullptr;
        }
        string& s = currentOp_.members(member_i);
        member_i += 1;
        return s;
    };

private:
    const data::FilterConfig& config_;
    int action_i;
    int member_i;
    data::Operation currentOp_;
};

/**
 * Config registration for the data filter.
 */
class DataTracingFilterFactory
    : public Server::Configuration::NamedHttpFilterConfigFactory {

public:

    Http::FilterFactoryCb createFilterFactoryFromProto(
            const Protobuf::Message &config, const std::string &stat_prefix,
            Server::Configuration::FactoryContext &context) override;

    ProtobufTypes::MessagePtr createEmptyConfigProto() override;

    std::string name() override;

private:
    Http::FilterFactoryCb createFilterFactory(
            const data::FilterConfig& config_pb,
            Upstream::ClusterManager &cluster_manager);

};

}  // namespace Data
}  // namespace Http
}  // namespace Envoy
