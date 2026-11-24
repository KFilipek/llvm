<%
    OneApi=tags['$OneApi']
    x=tags['$x']
    X=x.upper()
%>

.. _experimental-graph:

================================================================================
Graph
================================================================================

.. warning::

    Experimental features:

    *   May be replaced, updated, or removed at any time.
    *   Do not require maintaining API/ABI stability of their own additions over
        time.


Motivation
--------------------------------------------------------------------------------
This extension introduces functionality for recording enqueued operations into a
graph for later execution. Queues can enter graph capture mode, where operations
enqueued to them are recorded into a graph instead of being executed immediately.
This graph can then be instantiated as an executable graph and appended to a queue
multiple times for repeated execution.

API
--------------------------------------------------------------------------------

Enums
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* ${x}_device_info_t
    * ${X}_DEVICE_INFO_GRAPH_RECORD_AND_REPLAY_SUPPORT_EXP
    * ${X}_DEVICE_INFO_GRAPH_EXTERNAL_EVENTS_SUPPORT_EXP
* ${x}_exp_enqueue_ext_flags_t
    * ${X}_EXP_ENQUEUE_EXT_FLAG_GRAPH_EXTERNAL

Functions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
* Graph
   * ${x}GraphCreateExp
   * ${x}GraphInstantiateGraphExp
   * ${x}GraphDestroyExp
   * ${x}GraphExecutableGraphDestroyExp
   * ${x}GraphIsEmptyExp
   * ${x}GraphDumpContentsExp
   * ${x}GraphGetIdExp
* Queue
   * ${x}QueueBeginGraphCaptureExp
   * ${x}QueueBeginCaptureIntoGraphExp
   * ${x}QueueEndGraphCaptureExp
   * ${x}QueueIsGraphCaptureEnabledExp
   * ${x}QueueGetGraphExp
   * ${x}EnqueueGraphExp

Graph-External Events
--------------------------------------------------------------------------------

Events signaled by recorded commands are internal to the graph. To synchronize
a graph with work outside of it, for example to wait on the host for a point in
the graph, or to make the graph wait on work submitted to another queue, pass
${X}_EXP_ENQUEUE_EXT_FLAG_GRAPH_EXTERNAL in the properties of
${x}EnqueueEventsWaitWithBarrierExt while the queue is in graph capture mode.
For that command only, the events in ``phEventWaitList`` and the reusable
event passed in ``*phEvent`` are recorded as graph-external: each time the
graph is executed, the command waits on events signaled outside of the graph
and signals ``*phEvent`` so that it can be waited on outside of the graph.

The flag applies to this one command, not to the event objects, so the same
event may be used as a graph-internal event by other recorded commands. The
graph keeps referring to the events recorded this way, so the lifetime rule
described for ${X}_EXP_EVENT_FLAG_UNPOOLED applies to them.

The flag is only supported if the device reports
${X}_DEVICE_INFO_GRAPH_EXTERNAL_EVENTS_SUPPORT_EXP as true, otherwise
${x}EnqueueEventsWaitWithBarrierExt returns
${X}_RESULT_ERROR_UNSUPPORTED_FEATURE. If the queue is not in graph capture
mode, it returns ${X}_RESULT_ERROR_COMMAND_LIST_NOT_CAPTURING.

Error Codes
--------------------------------------------------------------------------------

The following result codes report misuse of graph capture. See
${x}QueueBeginGraphCaptureExp for the rules that apply while a queue is in graph
capture mode.

* ${X}_RESULT_ERROR_GRAPH_CAPTURE_UNSUPPORTED - an operation that is not
  supported during graph capture was called. This includes operations that
  synchronize the host with recorded work, such as ${x}QueueFinish,
  ${x}EventWait or a blocking enqueue.
* ${X}_RESULT_ERROR_GRAPH_CAPTURE_MERGE_ATTEMPT - a command enqueued to a queue
  that is already capturing waited on an event recorded by a different, still
  active, graph capture.
* ${X}_RESULT_ERROR_GRAPH_INTERNAL_EVENT - an event recorded into a graph was
  used outside of that graph after the capture ended, or a command recorded
  into a graph waited on an event signaled outside of it.
* ${X}_RESULT_ERROR_COMMAND_LIST_NOT_CAPTURING - ${x}QueueEndGraphCaptureExp was
  called on a queue that is not in graph capture mode, or
  ${X}_EXP_ENQUEUE_EXT_FLAG_GRAPH_EXTERNAL was passed to
  ${x}EnqueueEventsWaitWithBarrierExt on such a queue.
* ${X}_RESULT_ERROR_GRAPH_UNJOINED_FORKS - ${x}QueueEndGraphCaptureExp was called
  while a forked queue was not joined back.
* ${X}_RESULT_ERROR_INVALID_GRAPH - an invalid graph, such as one whose capture
  ended with ${X}_RESULT_ERROR_GRAPH_UNJOINED_FORKS, was passed to
  ${x}GraphInstantiateGraphExp or ${x}GraphIsEmptyExp.

Changelog
--------------------------------------------------------------------------------

+-----------+---------------------------------------------+
| Revision  | Changes                                     |
+===========+=============================================+
| 1.0       | Initial Draft                               |
+-----------+---------------------------------------------+
| 1.1       | Extend ${x}_device_info_t enumerator with   |
|           | graph record and replay entry.              |
+-----------+---------------------------------------------+
| 1.2       | Extend ${x}_command_t enumerator with       |
|           | enqueue graph event entry. Cleanup spec     |
|           | entry descriptions and return values.       |
|           | Rename QueueAppendGraphExp into             |
|           | EnqueueGraphExp.                            |
+-----------+---------------------------------------------+
| 1.3       | Add ${x}QueueGetGraphExp to retrieve graph  |
|           | handle from queue in capture mode.          |
+-----------+---------------------------------------------+
| 1.4       | Add ${x}GraphSetDestructionCallbackExp to   |
|           | register user callbacks invoked on graph    |
|           | destruction.                                |
+-----------+---------------------------------------------+
| 1.5       | Add ${x}GraphGetIdExp to retrieve a         |
|           | process-unique identifier for a graph.      |
+-----------+---------------------------------------------+
| 1.6       | Document when graph capture error codes are |
|           | returned.                                   |
+-----------+---------------------------------------------+
| 1.7       | Add graph-external events: the              |
|           | ${X}_EXP_ENQUEUE_EXT_FLAG_GRAPH_EXTERNAL    |
|           | enqueue flag and a device info to query     |
|           | support for it.                             |
+-----------+---------------------------------------------+

Support
--------------------------------------------------------------------------------

Adapters which support this experimental feature *must* return true for the new
``${X}_DEVICE_INFO_GRAPH_RECORD_AND_REPLAY_SUPPORT_EXP`` device info query.

Adapters which also support graph-external events *must* return true for the
``${X}_DEVICE_INFO_GRAPH_EXTERNAL_EVENTS_SUPPORT_EXP`` device info query.

Contributors
--------------------------------------------------------------------------------

* Krzysztof, Filipek `krzysztof.filipek@intel.com <krzysztof.filipek@intel.com>`_
* Krzysztof, Swiecicki `krzysztof.swiecicki@intel.com <krzysztof.swiecicki@intel.com>`_
* Matthew, Michel `matthew.michel@intel.com <matthew.michel@intel.com>`_
