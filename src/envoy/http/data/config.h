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
#include "src/istio/data/data_filter.pb.h"
#include "src/envoy/http/data/data_filter.h"
#include "src/envoy/http/data/map.h"

namespace Envoy {
namespace Http {
namespace Data {

ThreadSafeStringMapSharedPtr dataMap = std::make_shared<ThreadSafeStringMap>();

/**
 * Config registration for the data filter.
 */
class DataTracingFilterFactory
    : public Server::Configuration::NamedHttpFilterConfigFactory,
      Logger::Loggable<Logger::Id::filter> {

public:
    Http::FilterFactoryCb createFilterFactory(
            const Json::Object &config, const std::string &stat_prefix,
            Server::Configuration::FactoryContext &context) override;

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
