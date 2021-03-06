/* Copyright 2018 Istio Authors. All Rights Reserved.
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

#include "src/envoy/utils/filter_names.h"

namespace Envoy {
namespace Utils {

// TODO: using more standard naming, e.g istio.jwt, istio.authn
const char IstioFilterName::kJwt[] = "jwt-auth";
const char IstioFilterName::kAuthentication[] = "istio_authn";
const char IstioFilterName::kAlpn[] = "istio.alpn";
const char IstioFilterName::kData[] = "netsys.data_tracing";

}  // namespace Utils
}  // namespace Envoy
