// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions. See https://llvm.org/LICENSE.txt for license information.
//
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "fixtures.h"

#include <cstring>
#include <sstream>
#include <vector>

// Tests for UR_EXP_ENQUEUE_EXT_FLAG_GRAPH_EXTERNAL, which records the events of
// one urEnqueueEventsWaitWithBarrierExt call as graph-external.
struct urEnqueueEventsWaitWithBarrierExtGraphExternalBaseTest
    : uur::urGraphSupportedExpTest {
  void SetUp() override {
    UUR_RETURN_ON_FATAL_FAILURE(urGraphSupportedExpTest::SetUp());

    ur_bool_t reusableEventsSupported = false;
    ASSERT_SUCCESS(urDeviceGetInfo(
        device, UR_DEVICE_INFO_REUSABLE_EVENTS_SUPPORT_EXP,
        sizeof(reusableEventsSupported), &reusableEventsSupported, nullptr));
    if (!reusableEventsSupported) {
      GTEST_SKIP() << "Reusable events are not supported";
    }

    ASSERT_SUCCESS(urDeviceGetInfo(
        device, UR_DEVICE_INFO_GRAPH_EXTERNAL_EVENTS_SUPPORT_EXP,
        sizeof(graphExternalSupported), &graphExternalSupported, nullptr));

    // Host USM lets the tests check from the host what the graph wrote, without
    // any synchronization other than the event under test.
    ur_device_usm_access_capability_flags_t hostUSMSupport = 0;
    ASSERT_SUCCESS(uur::GetDeviceUSMHostSupport(device, hostUSMSupport));
    if (!hostUSMSupport) {
      GTEST_SKIP() << "Host USM is not supported";
    }
    for (uint32_t **mem : {&hostMem, &srcMem}) {
      ASSERT_SUCCESS(urUSMHostAlloc(context, nullptr, nullptr, allocationSize,
                                    reinterpret_cast<void **>(mem)));
      std::memset(*mem, 0, allocationSize);
    }

    ASSERT_SUCCESS(urGraphCreateExp(context, &graph));
  }

  void TearDown() override {
    bool capturing = false;
    if (queue &&
        urQueueIsGraphCaptureEnabledExp(queue, &capturing) ==
            UR_RESULT_SUCCESS &&
        capturing) {
      ur_exp_graph_handle_t capturedGraph = nullptr;
      // The result is ignored, the capture may end with an error.
      urQueueEndGraphCaptureExp(queue, &capturedGraph);
    }
    if (queue) {
      EXPECT_SUCCESS(urQueueFinish(queue));
    }
    if (otherQueue) {
      EXPECT_SUCCESS(urQueueFinish(otherQueue));
      EXPECT_SUCCESS(urQueueRelease(otherQueue));
    }
    if (execGraph) {
      EXPECT_SUCCESS(urGraphExecutableGraphDestroyExp(execGraph));
    }
    if (graph) {
      EXPECT_SUCCESS(urGraphDestroyExp(graph));
    }
    // Graphs refer to the events recorded into them, release the events last.
    for (ur_event_handle_t event : events) {
      EXPECT_SUCCESS(urEventRelease(event));
    }
    for (uint32_t *mem : {hostMem, srcMem}) {
      if (mem) {
        EXPECT_SUCCESS(urUSMFree(context, mem));
      }
    }

    UUR_RETURN_ON_FATAL_FAILURE(urGraphSupportedExpTest::TearDown());
  }

  ur_event_handle_t createReusableEvent() {
    ur_exp_event_desc_t desc{UR_STRUCTURE_TYPE_EXP_EVENT_DESC, nullptr, 0};
    ur_event_handle_t event = nullptr;
    EXPECT_SUCCESS(urEventCreateExp(context, device, &desc, &event));
    if (event) {
      events.push_back(event);
    }
    return event;
  }

  // A second in-order queue, which never records.
  void createOtherQueue() {
    ASSERT_SUCCESS(urQueueCreate(context, device, nullptr, &otherQueue));
  }

  // Signals event from otherQueue, outside of any graph, and waits for it.
  void signalOutsideOfGraph(ur_event_handle_t &event) {
    ASSERT_SUCCESS(urEnqueueEventsWaitWithBarrierExt(otherQueue, nullptr, 0,
                                                     nullptr, &event));
    ASSERT_SUCCESS(urEventWait(1, &event));
  }

  void fill(size_t offset, size_t numElements, uint32_t numEvents = 0,
            const ur_event_handle_t *waitEvents = nullptr) {
    ASSERT_SUCCESS(urEnqueueUSMFill(queue, hostMem + offset, sizeof(pattern),
                                    &pattern, numElements * sizeof(pattern),
                                    numEvents, waitEvents, nullptr));
  }

  void endCaptureAndInstantiate() {
    ur_exp_graph_handle_t capturedGraph = nullptr;
    ASSERT_SUCCESS(urQueueEndGraphCaptureExp(queue, &capturedGraph));
    ASSERT_EQ(graph, capturedGraph);
    ASSERT_SUCCESS(urGraphInstantiateGraphExp(graph, &execGraph));
  }

  void checkFilled(size_t offset, size_t numElements, uint32_t value) {
    for (size_t i = offset; i < offset + numElements; i++) {
      ASSERT_EQ(hostMem[i], value) << "at index " << i;
    }
  }

  void checkStatus(ur_event_handle_t event, ur_event_status_t expected) {
    ur_event_status_t status{};
    ASSERT_SUCCESS(uur::GetEventInfo<ur_event_status_t>(
        event, UR_EVENT_INFO_COMMAND_EXECUTION_STATUS, status));
    ASSERT_EQ(status, expected);
  }

  static constexpr size_t count = 64;
  static constexpr size_t allocationSize = count * sizeof(uint32_t);
  const uint32_t pattern = 0xC0FFEE;
  const ur_exp_enqueue_ext_properties_t graphExternalProps{
      UR_STRUCTURE_TYPE_EXP_ENQUEUE_EXT_PROPERTIES, nullptr,
      UR_EXP_ENQUEUE_EXT_FLAG_GRAPH_EXTERNAL};

  ur_bool_t graphExternalSupported = false;
  uint32_t *hostMem = nullptr;
  uint32_t *srcMem = nullptr;
  ur_exp_graph_handle_t graph = nullptr;
  ur_exp_executable_graph_handle_t execGraph = nullptr;
  ur_queue_handle_t otherQueue = nullptr;
  std::vector<ur_event_handle_t> events;
};

struct urEnqueueEventsWaitWithBarrierExtGraphExternalTest
    : urEnqueueEventsWaitWithBarrierExtGraphExternalBaseTest {
  void SetUp() override {
    UUR_RETURN_ON_FATAL_FAILURE(
        urEnqueueEventsWaitWithBarrierExtGraphExternalBaseTest::SetUp());
    if (!graphExternalSupported) {
      GTEST_SKIP() << "Graph-external events are not supported";
    }
  }
};

// The default printer can't name combinations of queue flags.
static std::string
graphExternalTestPrinter(const ::testing::TestParamInfo<
                         std::tuple<uur::DeviceTuple, ur_queue_flag_t>> &info) {
  const uint32_t flags = std::get<1>(info.param);
  std::stringstream ss;
  if (flags == 0) {
    ss << "default_queue_submission_mode";
  } else {
    ur::details::printFlag<ur_queue_flag_t>(ss, flags);
  }
  return uur::GetPlatformAndDeviceName(std::get<0>(info.param).device) + "__" +
         uur::GTestSanitizeString(ss.str());
}

// A graph-external barrier is lowered to a wait and a signal on every queue
// type, also on profiling queues, which otherwise use a real barrier.
#define GRAPH_EXTERNAL_QUEUE_TYPES                                             \
  ::testing::Values(                                                           \
      0 /* In-Order */, UR_QUEUE_FLAG_OUT_OF_ORDER_EXEC_MODE_ENABLE,           \
      UR_QUEUE_FLAG_SUBMISSION_BATCHED, UR_QUEUE_FLAG_PROFILING_ENABLE,        \
      UR_QUEUE_FLAG_OUT_OF_ORDER_EXEC_MODE_ENABLE |                            \
          UR_QUEUE_FLAG_PROFILING_ENABLE)

UUR_DEVICE_TEST_SUITE_WITH_QUEUE_TYPES_PRINTER(
    urEnqueueEventsWaitWithBarrierExtGraphExternalTest,
    GRAPH_EXTERNAL_QUEUE_TYPES, graphExternalTestPrinter);

// A graph-external signal can be waited on from the host after each execution
// of the graph.
TEST_P(urEnqueueEventsWaitWithBarrierExtGraphExternalTest, SuccessSignal) {
  ur_event_handle_t signal = createReusableEvent();
  ASSERT_NE(signal, nullptr);
  ur_event_handle_t original = signal;

  ASSERT_SUCCESS(urQueueBeginCaptureIntoGraphExp(queue, graph));
  UUR_RETURN_ON_FATAL_FAILURE(fill(0, count));
  ASSERT_SUCCESS(urEnqueueEventsWaitWithBarrierExt(queue, &graphExternalProps,
                                                   0, nullptr, &signal));
  ASSERT_EQ(signal, original);
  UUR_RETURN_ON_FATAL_FAILURE(endCaptureAndInstantiate());

  for (int run = 0; run < 2; run++) {
    std::memset(hostMem, 0, allocationSize);
    ASSERT_SUCCESS(urEnqueueGraphExp(queue, execGraph, 0, nullptr, nullptr));
    ASSERT_SUCCESS(urEventWait(1, &signal));
    UUR_RETURN_ON_FATAL_FAILURE(checkStatus(signal, UR_EVENT_STATUS_COMPLETE));
    // The signal follows the fill, so the data is visible without any other
    // synchronization.
    UUR_RETURN_ON_FATAL_FAILURE(checkFilled(0, count, pattern));
    ASSERT_SUCCESS(urQueueFinish(queue));
  }
}

// Negative control for SuccessSignal: without the flag the same signal is
// internal to the graph and can't be waited on from the host.
TEST_P(urEnqueueEventsWaitWithBarrierExtGraphExternalTest,
       InvalidInternalSignalWaitedOnHost) {
  UUR_GRAPH_SKIP_IF_L0_DRIVER_OLDER_THAN(
      uur::graphInternalEventErrorsMinL0DriverVersion);

  ur_event_handle_t signal = createReusableEvent();
  ASSERT_NE(signal, nullptr);

  ASSERT_SUCCESS(urQueueBeginCaptureIntoGraphExp(queue, graph));
  UUR_RETURN_ON_FATAL_FAILURE(fill(0, count));
  ur_exp_enqueue_ext_properties_t props{
      UR_STRUCTURE_TYPE_EXP_ENQUEUE_EXT_PROPERTIES, nullptr, 0};
  ASSERT_SUCCESS(
      urEnqueueEventsWaitWithBarrierExt(queue, &props, 0, nullptr, &signal));
  UUR_RETURN_ON_FATAL_FAILURE(endCaptureAndInstantiate());

  ASSERT_SUCCESS(urEnqueueGraphExp(queue, execGraph, 0, nullptr, nullptr));
  ASSERT_SUCCESS(urQueueFinish(queue));
  ASSERT_EQ_RESULT(UR_RESULT_ERROR_GRAPH_INTERNAL_EVENT,
                   urEventWait(1, &signal));
}

// A graph-external wait on an event signaled outside of the graph, by a queue
// that is not recording, after it wrote the data the graph copies. The event
// is signaled once before the capture as well, see
// InvalidInternalWaitOnEventSignaledOutside.
//
// The test can't show that the graph waits: a long-running command that would
// keep the signal back isn't available without kernels, and on the tested
// driver a blocking host task on the other queue also delays graphs that
// don't wait on it.
TEST_P(urEnqueueEventsWaitWithBarrierExtGraphExternalTest, SuccessWait) {
  UUR_RETURN_ON_FATAL_FAILURE(createOtherQueue());
  ur_event_handle_t external = createReusableEvent();
  ASSERT_NE(external, nullptr);
  UUR_RETURN_ON_FATAL_FAILURE(signalOutsideOfGraph(external));

  ASSERT_SUCCESS(urQueueBeginCaptureIntoGraphExp(queue, graph));
  ASSERT_SUCCESS(urEnqueueEventsWaitWithBarrierExt(queue, &graphExternalProps,
                                                   1, &external, nullptr));
  ASSERT_SUCCESS(urEnqueueUSMMemcpy(queue, false, hostMem, srcMem,
                                    allocationSize, 0, nullptr, nullptr));
  UUR_RETURN_ON_FATAL_FAILURE(endCaptureAndInstantiate());

  for (uint32_t run = 0; run < 2; run++) {
    const uint32_t value = pattern + run;
    std::memset(hostMem, 0, allocationSize);
    ASSERT_SUCCESS(urEnqueueUSMFill(otherQueue, srcMem, sizeof(value), &value,
                                    allocationSize, 0, nullptr, nullptr));
    ASSERT_SUCCESS(urEnqueueEventsWaitWithBarrierExt(otherQueue, nullptr, 0,
                                                     nullptr, &external));
    ASSERT_SUCCESS(urEnqueueGraphExp(queue, execGraph, 0, nullptr, nullptr));
    ASSERT_SUCCESS(urQueueFinish(queue));
    UUR_RETURN_ON_FATAL_FAILURE(
        checkStatus(external, UR_EVENT_STATUS_COMPLETE));
    UUR_RETURN_ON_FATAL_FAILURE(checkFilled(0, count, value));
  }
}

// Negative control for SuccessWait: without the flag, a recorded wait on an
// event signaled outside of the graph is rejected. This depends on the driver,
// see urQueueBeginGraphCaptureExp.
TEST_P(urEnqueueEventsWaitWithBarrierExtGraphExternalTest,
       InvalidInternalWaitOnEventSignaledOutside) {
  UUR_GRAPH_SKIP_IF_L0_DRIVER_OLDER_THAN(
      uur::graphInternalEventErrorsMinL0DriverVersion);

  UUR_RETURN_ON_FATAL_FAILURE(createOtherQueue());
  ur_event_handle_t external = createReusableEvent();
  ASSERT_NE(external, nullptr);
  UUR_RETURN_ON_FATAL_FAILURE(signalOutsideOfGraph(external));

  ASSERT_SUCCESS(urQueueBeginCaptureIntoGraphExp(queue, graph));
  ASSERT_EQ_RESULT(
      UR_RESULT_ERROR_GRAPH_INTERNAL_EVENT,
      urEnqueueEventsWaitWithBarrierExt(queue, nullptr, 1, &external, nullptr));
}

// The same reusable event is used as a graph-internal dependency and is then
// signaled as graph-external by the last command of the graph.
TEST_P(urEnqueueEventsWaitWithBarrierExtGraphExternalTest,
       SuccessInternalAndExternalUse) {
  ur_event_handle_t event = createReusableEvent();
  ASSERT_NE(event, nullptr);
  constexpr size_t half = count / 2;

  ASSERT_SUCCESS(urQueueBeginCaptureIntoGraphExp(queue, graph));
  UUR_RETURN_ON_FATAL_FAILURE(fill(0, half));
  ASSERT_SUCCESS(
      urEnqueueEventsWaitWithBarrierExt(queue, nullptr, 0, nullptr, &event));
  UUR_RETURN_ON_FATAL_FAILURE(fill(half, half, 1, &event));
  ASSERT_SUCCESS(urEnqueueEventsWaitWithBarrierExt(queue, &graphExternalProps,
                                                   0, nullptr, &event));
  UUR_RETURN_ON_FATAL_FAILURE(endCaptureAndInstantiate());

  ASSERT_SUCCESS(urEnqueueGraphExp(queue, execGraph, 0, nullptr, nullptr));
  ASSERT_SUCCESS(urEventWait(1, &event));
  UUR_RETURN_ON_FATAL_FAILURE(checkStatus(event, UR_EVENT_STATUS_COMPLETE));
  UUR_RETURN_ON_FATAL_FAILURE(checkFilled(0, count, pattern));
}

// The flag is only valid while the queue is recording. The call fails without
// changing the passed event, which stays usable.
TEST_P(urEnqueueEventsWaitWithBarrierExtGraphExternalTest,
       InvalidNotCapturing) {
  ur_event_handle_t event = createReusableEvent();
  ASSERT_NE(event, nullptr);
  ur_event_handle_t original = event;

  ASSERT_EQ_RESULT(UR_RESULT_ERROR_COMMAND_LIST_NOT_CAPTURING,
                   urEnqueueEventsWaitWithBarrierExt(queue, &graphExternalProps,
                                                     0, nullptr, &event));
  ASSERT_EQ(event, original);

  ur_event_handle_t noEvent = nullptr;
  ASSERT_EQ_RESULT(UR_RESULT_ERROR_COMMAND_LIST_NOT_CAPTURING,
                   urEnqueueEventsWaitWithBarrierExt(queue, &graphExternalProps,
                                                     1, &event, &noEvent));
  ASSERT_EQ(noEvent, nullptr);

  ASSERT_SUCCESS(
      urEnqueueEventsWaitWithBarrierExt(queue, nullptr, 0, nullptr, &event));
  ASSERT_SUCCESS(urEventWait(1, &event));
}

using urEnqueueEventsWaitWithBarrierExtGraphExternalUnsupportedTest =
    urEnqueueEventsWaitWithBarrierExtGraphExternalBaseTest;

UUR_DEVICE_TEST_SUITE_WITH_QUEUE_TYPES_PRINTER(
    urEnqueueEventsWaitWithBarrierExtGraphExternalUnsupportedTest,
    GRAPH_EXTERNAL_QUEUE_TYPES, graphExternalTestPrinter);

// Devices that support graphs but not graph-external events reject the flag
// during capture, without breaking the capture.
TEST_P(urEnqueueEventsWaitWithBarrierExtGraphExternalUnsupportedTest,
       InvalidUnsupportedFeature) {
  if (graphExternalSupported) {
    GTEST_SKIP() << "Graph-external events are supported";
  }

  ur_event_handle_t event = createReusableEvent();
  ASSERT_NE(event, nullptr);
  ur_event_handle_t original = event;

  ASSERT_SUCCESS(urQueueBeginCaptureIntoGraphExp(queue, graph));
  UUR_RETURN_ON_FATAL_FAILURE(fill(0, count));
  ASSERT_EQ_RESULT(UR_RESULT_ERROR_UNSUPPORTED_FEATURE,
                   urEnqueueEventsWaitWithBarrierExt(queue, &graphExternalProps,
                                                     0, nullptr, &event));
  ASSERT_EQ(event, original);
  UUR_RETURN_ON_FATAL_FAILURE(endCaptureAndInstantiate());

  ASSERT_SUCCESS(urEnqueueGraphExp(queue, execGraph, 0, nullptr, nullptr));
  ASSERT_SUCCESS(urQueueFinish(queue));
  UUR_RETURN_ON_FATAL_FAILURE(checkFilled(0, count, pattern));
}

// Runs on every device, also those without graph support: if the device
// doesn't support graph-external events, the flag is rejected even before the
// capture state is checked.
using urEnqueueEventsWaitWithBarrierExtGraphExternalAnyDeviceTest =
    uur::urQueueTest;
UUR_INSTANTIATE_DEVICE_TEST_SUITE(
    urEnqueueEventsWaitWithBarrierExtGraphExternalAnyDeviceTest);

TEST_P(urEnqueueEventsWaitWithBarrierExtGraphExternalAnyDeviceTest,
       InvalidUnsupportedFeature) {
  ur_bool_t supported = false;
  UUR_ASSERT_SUCCESS_OR_UNSUPPORTED(
      urDeviceGetInfo(device, UR_DEVICE_INFO_GRAPH_EXTERNAL_EVENTS_SUPPORT_EXP,
                      sizeof(supported), &supported, nullptr));
  if (supported) {
    GTEST_SKIP() << "Graph-external events are supported";
  }

  ur_exp_enqueue_ext_properties_t props{
      UR_STRUCTURE_TYPE_EXP_ENQUEUE_EXT_PROPERTIES, nullptr,
      UR_EXP_ENQUEUE_EXT_FLAG_GRAPH_EXTERNAL};
  ur_event_handle_t event = nullptr;
  ASSERT_EQ_RESULT(
      UR_RESULT_ERROR_UNSUPPORTED_FEATURE,
      urEnqueueEventsWaitWithBarrierExt(queue, &props, 0, nullptr, &event));
  ASSERT_EQ(event, nullptr);
}
