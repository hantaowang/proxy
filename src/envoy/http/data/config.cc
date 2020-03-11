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

#include "common/protobuf/message_validator_impl.h"
#include "src/envoy/http/data/config.h"
#include "src/envoy/http/data/data_filter.h"
#include "src/envoy/http/data/map.h"
#include "src/envoy/utils/filter_names.h"

namespace Envoy {
namespace Http {
namespace Data {

Http::FilterFactoryCb
DataTracingFilterFactory::createFilterFactory(
        const Json::Object &config, const std::string &stat_prefix,
        Server::Configuration::FactoryContext &context) {
    data::FilterConfig filter_config;
    (void) stat_prefix;
    ENVOY_LOG(warn, "Create from JSON");
    MessageUtil::loadFromJson(config.asJsonString(), filter_config,
                              ProtobufMessage::getNullValidationVisitor());
    return createFilterFactory(filter_config, context.clusterManager());
}

Http::FilterFactoryCb
DataTracingFilterFactory::createFilterFactoryFromProto(
    const Protobuf::Message &config, const std::string &stat_prefix,
    Server::Configuration::FactoryContext &context) {
    (void) stat_prefix;
    ENVOY_LOG(warn, "Create from proto");
    return createFilterFactory(dynamic_cast<const data::FilterConfig &>(config), context.clusterManager());
}

ProtobufTypes::MessagePtr
DataTracingFilterFactory::createEmptyConfigProto() {
    ENVOY_LOG(warn, "Create from empty");
    return ProtobufTypes::MessagePtr{new data::FilterConfig()};
}

std::string
DataTracingFilterFactory::name() {
    return Utils::IstioFilterName::kData;
}

Http::FilterFactoryCb
DataTracingFilterFactory::createFilterFactory(const data::FilterConfig &proto_config,
                                              Upstream::ClusterManager &cluster_manager) {
    (void) cluster_manager;
    ENVOY_LOG(warn, "Config in Constructor: {}", proto_config.DebugString());

    DataTracingFilterConfigSharedPtr filter_config =
            std::make_shared<DataTracingFilterConfig>(proto_config);

    return [filter_config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
        callbacks.addStreamFilter(std::make_unique<DataTracingFilter>(
                filter_config, dataMap));
    };

}

/**
 * Static registration for the data tracing filter. @see RegisterFactory.
 */
REGISTER_FACTORY(DataTracingFilterFactory,
                 Server::Configuration::NamedHttpFilterConfigFactory);

}  // namespace Data
}  // namespace Http
}  // namespace Envoy
