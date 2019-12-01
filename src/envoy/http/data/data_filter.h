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

#include "src/envoy/http/data/data_filter.pb.h"
#include "extensions/filters/http/common/pass_through_filter.h"

namespace Envoy {
namespace Http {
namespace Data {

class DataTracingFilterConfig {
 public:
  DataTracingFilterConfig(
      const data::FilterConfig
          &proto_config,
      Upstream::ClusterManager &cluster_manager);

  Upstream::ClusterManager &clusterManager() { return cluster_manager_; }

 private:

  Upstream::ClusterManager &cluster_manager_;
};

using DataTracingFilterConfigSharedPtr = std::shared_ptr<DataTracingFilterConfig>;

class DataTracingFilter : public Http::PassThroughFilter,
                   Logger::Loggable<Logger::Id::filter> {
 public:
  explicit DataTracingFilter(const DataTracingFilterConfigSharedPtr &config)
      : config_(config) {}

  // Http::PassThroughDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::HeaderMap &headers,
                                          bool end_stream) override;

  // Http::PassThroughEncoderFilter
  Http::FilterHeadersStatus encodeHeaders(Http::HeaderMap &headers,
                                          bool end_stream) override;
 private:
  const DataTracingFilterConfigSharedPtr config_;
};

}  // namespace DataTracing
}  // namespace Http
}  // namespace Envoy
