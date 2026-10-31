# VideoSink HAL Test Suite

This repository contains the Vendor Test Suite (VTS) for the `IVideoSink` AIDL HAL interface. It is designed to validate the functionality of the VideoSink HAL implementation.

## Build Steps

This test suite is part of Unified VTS for AIDL HALs. For initial setup, environment configuration, and build instructions, please refer to the Unified VTS README documentation.

- **[Unified VTS README](https://github.com/rdkcentral/rdk-halif-aidl-unified-vts/blob/main/README.md)**

## Running the VideoSink Tests

While the Unified VTS can run all tests at once, you can execute the `VTS_L1_VideoSink`, `VTS_L2_VideoSink`, and `VTS_L3_VideoSink` test suite individually for targeted validation. This is useful for debugging and development of the `VideoSink` HAL.

### AIDL 0.23.0

L3 tests use operation-based names such as `VSink_Sequential`,
`VSink_Sequential_EOS` and `VSink_Concurrent_BackPressure`. The 48 mode-specific
variants are consolidated into 12 cases; existing `Sink_*` configuration keys
and stream/control settings are unchanged.

The sink is associated with its decoder before decoding starts. After decoder
`start()`, the test queries `getCurrentOperationalMode()`. Valid non-tunnelled
handles are queued to the sink; `-1` tunnelled handles are never queued or freed
by the client. The vendor owns tunnelled delivery and cleanup. Secure input is
selected only when both decoder and sink advertise `supportsSecure`.

Both paths test playback, EOS sequencing, flush/discard operations and cleanup.
Client queue back-pressure and client-side underflow injection are non-tunnelled
probes; their inapplicability to tunnelled playback is logged explicitly.
The suite depends on the corresponding AIDL 0.23.0 video-decoder test implementation.

## Reference Documents

|SNo|Document Name|Document Description|Document Link|
|---|-------------|--------------------|-------------|
|1|High Level Test Specification Document|High Level Test Specification Documentation this module|[videoSink_High-Level_TestSpecification.md](docs/pages/videoSink_High-Level_TestSpecification.md)|
|2|`L2` L2 Low Level Test Specification and Procedure Document|`L2` Test Specification and Procedure Documentation|[VideoSink_L2_Low-Level_TestSpecification.md](docs/pages/VideoSink_L2_Low-Level_TestSpecification.md)|
|3|`L3` L3 Low Level Test Specification and Procedure Document|`L3` Test Specification and Procedure Documentation|[videoSink_L3_Low-Level_TestSpecification.md](docs/pages/videoSink_L3_Low-Level_TestSpecification.md)|
