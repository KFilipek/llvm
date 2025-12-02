//===--------- graph.cpp - Level Zero Adapter -----------------------------===//
//
// Copyright (C) 2025 Intel Corporation
//
// Part of the Unified-Runtime Project, under the Apache License v2.0 with LLVM
// Exceptions. See LICENSE.TXT
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "common.hpp"
#include "graph.hpp"
#include "context.hpp"
#include "../ur_interface_loader.hpp"
#include <level_zero/driver_experimental/zex_api.h>

// struct ur_exp_graph_handle_t_ : ur_object {
// public:
//   ur_exp_graph_handle_t_(ur_context_handle_t hContext) : hContext(hContext) {
//     if (!hContext->getPlatform()->ZeGraphExt.Supported) {
//       throw UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
//     }
//     ZE2UR_CALL_THROWS(hContext->getPlatform()->ZeGraphExt.zeGraphCreateExp, (
//       hContext->getZeHandle(), &getZeHandle(), nullptr));
//   }

//   inline ze_graph_handle_t &getZeHandle() { return hGraph; }
// private:
//   ze_graph_handle_t hGraph;
//   ur_context_handle_t hContext;
// };

namespace ur::level_zero {

// Graph experimental functions - not yet supported
ur_result_t urGraphCreateExp(ur_context_handle_t hContext,
                             ur_exp_graph_handle_t *phGraph) try {
  *phGraph = new ur_exp_graph_handle_t_(hContext);
  if (*phGraph == nullptr) {
    return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
  }

  // Ensure platform supports graph experimental extension and create the
  // ZE graph handle through the driver's experimental entrypoints.
  if (!hContext->getPlatform()->ZeGraphExt.Supported) {
    delete *phGraph;
    *phGraph = nullptr;
    return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ZE2UR_CALL_THROWS(hContext->getPlatform()->ZeGraphExt.zeGraphCreateExp,
                    (hContext->getZeHandle(), &(*phGraph)->getZeHandle(),
                     nullptr));

  return UR_RESULT_SUCCESS;
} catch (const ur_result_t &ex) {
  return ex;
} catch (...) {
  return UR_RESULT_ERROR_UNKNOWN;
}

ur_result_t urGraphDestroyExp(ur_exp_graph_handle_t hGraph) {
  if (nullptr == hGraph) {
    return UR_RESULT_ERROR_INVALID_NULL_HANDLE;
  }

  ur_context_handle_t ctx = hGraph->getContext();
  if (!ctx->getPlatform()->ZeGraphExt.Supported) {
    return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ZE2UR_CALL(ctx->getPlatform()->ZeGraphExt.zeGraphDestroyExp,
             (hGraph->getZeHandle()));

  delete hGraph;

  return UR_RESULT_SUCCESS;
}

ur_result_t urGraphExecutableGraphDestroyExp(
    ur_exp_executable_graph_handle_t hExecutableGraph) {
  if (nullptr == hExecutableGraph) {
    return UR_RESULT_ERROR_INVALID_NULL_HANDLE;
  }

  ur_context_handle_t ctx = hExecutableGraph->getContext();
  if (!ctx->getPlatform()->ZeGraphExt.Supported) {
    return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ZE2UR_CALL(ctx->getPlatform()->ZeGraphExt.zeExecutableGraphDestroyExp,
             (hExecutableGraph->getZeHandle()));

  delete hExecutableGraph;

  return UR_RESULT_SUCCESS;
}

ur_result_t urGraphIsEmptyExp(ur_exp_graph_handle_t hGraph, bool *pIsEmpty) {
  if (nullptr == hGraph) {
    return UR_RESULT_ERROR_INVALID_NULL_HANDLE;
  }

  ur_context_handle_t ctx = hGraph->getContext();
  if (!ctx->getPlatform()->ZeGraphExt.Supported) {
    return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
  }

  ze_result_t zeResult = ZE_CALL_NOCHECK(
      ctx->getPlatform()->ZeGraphExt.zeGraphIsEmptyExp,
      (hGraph->getZeHandle()));

  if (pIsEmpty) {
    *pIsEmpty = (zeResult == ZE_RESULT_SUCCESS);
  }

  return UR_RESULT_SUCCESS;
}

ur_result_t urGraphDumpContentsExp(ur_exp_graph_handle_t hGraph,
                                   const char *pDotFilePath) {
  std::ignore = hGraph;
  std::ignore = pDotFilePath;
  UR_LOG_LEGACY(ERR,
                logger::LegacyMessage("[UR][L0] {} function not implemented!"),
                "{} function not implemented!", __FUNCTION__);
  return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ur_result_t urGraphInstantiateGraphExp(
    ur_exp_graph_handle_t hGraph,
    ur_exp_executable_graph_handle_t *phExecutableGraph) {
  try {
    if (nullptr == hGraph) {
      return UR_RESULT_ERROR_INVALID_NULL_HANDLE;
    }

    if (phExecutableGraph)
      *phExecutableGraph = nullptr;

    ur_context_handle_t ctx = hGraph->getContext();
    if (!ctx->getPlatform()->ZeGraphExt.Supported) {
      return UR_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    *phExecutableGraph = new ur_exp_executable_graph_handle_t_(ctx);
    if (*phExecutableGraph == nullptr) {
      return UR_RESULT_ERROR_OUT_OF_HOST_MEMORY;
    }

    ZE2UR_CALL_THROWS(ctx->getPlatform()->ZeGraphExt.zeCommandListInstantiateGraphExp,
                      (hGraph->getZeHandle(), &(*phExecutableGraph)->getZeHandle(), nullptr));

    return UR_RESULT_SUCCESS;
  } catch (const ur_result_t &e) {
    if (phExecutableGraph && *phExecutableGraph) {
      delete *phExecutableGraph;
      *phExecutableGraph = nullptr;
    }
    return e;
  } catch (...) {
    if (phExecutableGraph && *phExecutableGraph) {
      delete *phExecutableGraph;
      *phExecutableGraph = nullptr;
    }
    return UR_RESULT_ERROR_UNKNOWN;
  }
}

} // namespace ur::level_zero
