// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// RUN: %with-v2 ZE_ENABLE_TRACING_LAYER=1 ./graph_external_events-test
// REQUIRES: v2

// Checks which ze calls urEnqueueEventsWaitWithBarrierExt makes during graph
// capture: with UR_EXP_ENQUEUE_EXT_FLAG_GRAPH_EXTERNAL the wait and the signal
// are appended with a ze_event_ext_flags_desc_t, without it the plain calls
// are made.

#include "uur/fixtures.h"

#include <layers/zel_tracing_api.h>
#include <layers/zel_tracing_register_cb.h>
#include <loader/ze_loader.h>

#include <memory>
#include <vector>

namespace {
// Reset before each barrier.
int waitOnEventsCount = 0;
int signalEventCount = 0;
int barrierCount = 0;
int waitOnEventsWithParamsCount = 0;
int signalEventWithParamsCount = 0;
int graphExternalDescCount = 0;

void countGraphExternalDesc(const void *pNext) {
  auto *desc = static_cast<const ze_event_ext_flags_desc_t *>(pNext);
  if (desc && desc->stype == ZE_STRUCTURE_TYPE_EVENT_EXT_FLAGS_DESC &&
      desc->pNext == nullptr &&
      desc->flags == ZE_EVENT_EXT_FLAG_APPEND_GRAPH_EXTERNAL) {
    graphExternalDescCount++;
  }
}

// The *WithParameters callbacks aren't part of zel_core_callbacks_t, so they
// are registered one by one, which is only allowed before the tracer is
// enabled.
std::shared_ptr<_zel_tracer_handle_t> tracer = [] {
  EXPECT_EQ(zeInit(ZE_INIT_FLAG_GPU_ONLY), ZE_RESULT_SUCCESS);

  zel_tracer_desc_t tracerDesc = {ZEL_STRUCTURE_TYPE_TRACER_EXP_DESC, nullptr,
                                  nullptr};
  zel_tracer_handle_t hTracer = nullptr;
  EXPECT_EQ(zelTracerCreate(&tracerDesc, &hTracer), ZE_RESULT_SUCCESS);

  EXPECT_EQ(zelTracerCommandListAppendWaitOnEventsRegisterCallback(
                hTracer, ZEL_REGISTER_PROLOGUE,
                [](ze_command_list_append_wait_on_events_params_t *,
                   ze_result_t, void *, void **) { waitOnEventsCount++; }),
            ZE_RESULT_SUCCESS);
  EXPECT_EQ(zelTracerCommandListAppendSignalEventRegisterCallback(
                hTracer, ZEL_REGISTER_PROLOGUE,
                [](ze_command_list_append_signal_event_params_t *, ze_result_t,
                   void *, void **) { signalEventCount++; }),
            ZE_RESULT_SUCCESS);
  EXPECT_EQ(zelTracerCommandListAppendBarrierRegisterCallback(
                hTracer, ZEL_REGISTER_PROLOGUE,
                [](ze_command_list_append_barrier_params_t *, ze_result_t,
                   void *, void **) { barrierCount++; }),
            ZE_RESULT_SUCCESS);
  EXPECT_EQ(
      zelTracerCommandListAppendWaitOnEventsWithParametersRegisterCallback(
          hTracer, ZEL_REGISTER_PROLOGUE,
          [](ze_command_list_append_wait_on_events_with_parameters_params_t
                 *params,
             ze_result_t, void *, void **) {
            waitOnEventsWithParamsCount++;
            countGraphExternalDesc(*params->ppNext);
          }),
      ZE_RESULT_SUCCESS);
  EXPECT_EQ(zelTracerCommandListAppendSignalEventWithParametersRegisterCallback(
                hTracer, ZEL_REGISTER_PROLOGUE,
                [](ze_command_list_append_signal_event_with_parameters_params_t
                       *params,
                   ze_result_t, void *, void **) {
                  signalEventWithParamsCount++;
                  countGraphExternalDesc(*params->ppNext);
                }),
            ZE_RESULT_SUCCESS);

  EXPECT_EQ(zelTracerSetEnabled(hTracer, true), ZE_RESULT_SUCCESS);

  return std::shared_ptr<_zel_tracer_handle_t>(
      hTracer, [](zel_tracer_handle_t t) { zelTracerDestroy(t); });
}();
} // namespace

struct urGraphExternalEventsZeCallsTest : uur::urMultiQueueTypeTest {
  void SetUp() override {
    UUR_RETURN_ON_FATAL_FAILURE(urMultiQueueTypeTest::SetUp());

    for (ur_device_info_t info :
         {UR_DEVICE_INFO_GRAPH_RECORD_AND_REPLAY_SUPPORT_EXP,
          UR_DEVICE_INFO_REUSABLE_EVENTS_SUPPORT_EXP,
          UR_DEVICE_INFO_GRAPH_EXTERNAL_EVENTS_SUPPORT_EXP}) {
      ur_bool_t supported = false;
      ASSERT_SUCCESS(urDeviceGetInfo(device, info, sizeof(supported),
                                     &supported, nullptr));
      if (!supported) {
        GTEST_SKIP() << "Device info " << info << " is false";
      }
    }

    ASSERT_SUCCESS(urQueueCreate(context, device, nullptr, &otherQueue));
    ASSERT_SUCCESS(urGraphCreateExp(context, &graph));
  }

  void TearDown() override {
    bool capturing = false;
    if (queue &&
        urQueueIsGraphCaptureEnabledExp(queue, &capturing) ==
            UR_RESULT_SUCCESS &&
        capturing) {
      ur_exp_graph_handle_t capturedGraph = nullptr;
      EXPECT_SUCCESS(urQueueEndGraphCaptureExp(queue, &capturedGraph));
    }
    if (otherQueue) {
      EXPECT_SUCCESS(urQueueFinish(otherQueue));
      EXPECT_SUCCESS(urQueueRelease(otherQueue));
    }
    if (graph) {
      EXPECT_SUCCESS(urGraphDestroyExp(graph));
    }
    for (ur_event_handle_t event : events) {
      EXPECT_SUCCESS(urEventRelease(event));
    }
    UUR_RETURN_ON_FATAL_FAILURE(urMultiQueueTypeTest::TearDown());
  }

  ur_event_handle_t createReusableEvent() {
    ur_exp_event_desc_t desc{UR_STRUCTURE_TYPE_EXP_EVENT_DESC, nullptr, 0};
    ur_event_handle_t event = nullptr;
    EXPECT_SUCCESS(urEventCreateExp(context, device, &desc, &event));
    events.push_back(event);
    return event;
  }

  void barrier(const ur_exp_enqueue_ext_properties_t *props,
               ur_event_handle_t &waitEvent, ur_event_handle_t &signalEvent) {
    waitOnEventsCount = 0;
    signalEventCount = 0;
    barrierCount = 0;
    waitOnEventsWithParamsCount = 0;
    signalEventWithParamsCount = 0;
    graphExternalDescCount = 0;

    ASSERT_SUCCESS(urEnqueueEventsWaitWithBarrierExt(queue, props, 1,
                                                     &waitEvent, &signalEvent));
  }

  ur_queue_handle_t otherQueue = nullptr;
  ur_exp_graph_handle_t graph = nullptr;
  std::vector<ur_event_handle_t> events;
};

UUR_DEVICE_TEST_SUITE_WITH_QUEUE_TYPES(
    urGraphExternalEventsZeCallsTest,
    ::testing::Values(0 /* In-Order */,
                      UR_QUEUE_FLAG_OUT_OF_ORDER_EXEC_MODE_ENABLE,
                      UR_QUEUE_FLAG_SUBMISSION_BATCHED,
                      UR_QUEUE_FLAG_PROFILING_ENABLE));

// Wait on an event signaled outside of the graph and signal a reusable event,
// both as graph-external: one wait and one signal with the descriptor, no
// plain wait, signal or barrier, also on profiling queues.
TEST_P(urGraphExternalEventsZeCallsTest, GraphExternal) {
  ur_event_handle_t external = createReusableEvent();
  ur_event_handle_t signal = createReusableEvent();
  ASSERT_SUCCESS(urEnqueueEventsWaitWithBarrierExt(otherQueue, nullptr, 0,
                                                   nullptr, &external));
  ASSERT_SUCCESS(urEventWait(1, &external));

  ASSERT_SUCCESS(urQueueBeginCaptureIntoGraphExp(queue, graph));
  const ur_exp_enqueue_ext_properties_t props{
      UR_STRUCTURE_TYPE_EXP_ENQUEUE_EXT_PROPERTIES, nullptr,
      UR_EXP_ENQUEUE_EXT_FLAG_GRAPH_EXTERNAL};
  UUR_RETURN_ON_FATAL_FAILURE(barrier(&props, external, signal));

  EXPECT_EQ(waitOnEventsWithParamsCount, 1);
  EXPECT_EQ(signalEventWithParamsCount, 1);
  EXPECT_EQ(graphExternalDescCount, 2);
  EXPECT_EQ(waitOnEventsCount, 0);
  EXPECT_EQ(signalEventCount, 0);
  EXPECT_EQ(barrierCount, 0);
}

// The same barrier without the flag, waiting on an event recorded into the
// same graph: no call carries the descriptor.
TEST_P(urGraphExternalEventsZeCallsTest, NoFlag) {
  ur_event_handle_t internal = createReusableEvent();
  ur_event_handle_t signal = createReusableEvent();

  ASSERT_SUCCESS(urQueueBeginCaptureIntoGraphExp(queue, graph));
  ASSERT_SUCCESS(
      urEnqueueEventsWaitWithBarrierExt(queue, nullptr, 0, nullptr, &internal));
  UUR_RETURN_ON_FATAL_FAILURE(barrier(nullptr, internal, signal));

  EXPECT_EQ(waitOnEventsWithParamsCount, 0);
  EXPECT_EQ(signalEventWithParamsCount, 0);
  EXPECT_EQ(graphExternalDescCount, 0);
  if (getQueueMode() & UR_QUEUE_FLAG_PROFILING_ENABLE) {
    EXPECT_EQ(barrierCount, 1);
  } else {
    EXPECT_EQ(waitOnEventsCount, 1);
    EXPECT_EQ(signalEventCount, 1);
  }
}
