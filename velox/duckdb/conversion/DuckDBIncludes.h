/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

// On MSVC, Type.h defines BOOLEAN() as a function-like macro to resolve
// the winnt.h typedef conflict. DuckDB headers declare
// Value::BOOLEAN(int8_t) which collides with this macro. Save and restore it.
#ifdef _MSC_VER
#pragma push_macro("BOOLEAN")
#undef BOOLEAN
#endif

#include <duckdb.hpp> // @manual

#ifdef _MSC_VER
#pragma pop_macro("BOOLEAN")
#endif
