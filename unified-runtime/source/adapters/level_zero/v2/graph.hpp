//===--------- graph.cpp - Level Zero Adapter -----------------------------===//
//
// Copyright (C) 2025 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#pragma once

#include "ur_api.h"
#include "common.hpp"
#include <level_zero/driver_experimental/zex_api.h>

struct ur_exp_graph_handle_t_ : ur_object {
public:
  ur_exp_graph_handle_t_(ur_context_handle_t hContext) : hGraph(nullptr), hContext(hContext) {}

  inline ze_graph_handle_t &getZeHandle() { return hGraph; }
  inline const ze_graph_handle_t &getZeHandle() const { return hGraph; }
private:
  ze_graph_handle_t hGraph;
  ur_context_handle_t hContext;
};

// Implement: ur_exp_executable_graph_handle_t
struct ur_exp_executable_graph_handle_t_ : ur_object {
public:
  ur_exp_executable_graph_handle_t_(ur_context_handle_t hContext) : hExecutableGraph(nullptr), hContext(hContext) {}
  inline ze_executable_graph_handle_t &getZeHandle() { return hExecutableGraph; }
  inline const ze_executable_graph_handle_t &getZeHandle() const { return hExecutableGraph; }
private:
  ze_executable_graph_handle_t hExecutableGraph;
  ur_context_handle_t hContext;
};
