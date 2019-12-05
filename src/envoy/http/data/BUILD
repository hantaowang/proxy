# Copyright 2019 Istio Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################
#

load(
    "@envoy//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "data_filter",
    srcs = ["data_filter.cc"],
    hdrs = ["data_filter.h"],
    repository = "@envoy",
    deps = [
        "@envoy//include/envoy/http:filter_interface",
        "@envoy//include/envoy/http:header_map_interface",
        "@envoy//include/envoy/network:connection_interface",
        "@envoy//source/common/network:application_protocol_lib",
        "@envoy//source/common/router:string_accessor_lib",
        "@envoy//source/extensions/filters/http/common:pass_through_filter_lib",
    ],
)

envoy_cc_library(
    name = "config_lib",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    repository = "@envoy",
    visibility = ["//visibility:public"],
    deps = [
        ":data_filter",
        "//src/envoy/utils:filter_names_lib",
        "@envoy//include/envoy/registry",
        "@envoy//source/exe:envoy_common_lib",
        "@envoy//source/extensions/filters/http/common:factory_base_lib",
        "@envoy//source/extensions/filters/http/common:empty_http_filter_config_lib",
    ],
)
