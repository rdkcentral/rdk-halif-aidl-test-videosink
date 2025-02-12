# Video Sink High Level Test Specification Document

## Table of Contents

- [Acronyms, Terms and Abbreviations](#acronyms-terms-and-abbreviations)
- [Introduction](#introduction)
- [Module Description](#module-description)
- [L2 Test Specification](#l2-test-specification)
- [L3 Test Specification](#l3-test-specification)

## Acronyms, Terms and Abbreviations

- `HAL`    - Hardware Abstraction Layer
- `API`    - Application Programming Interface
- `AIDL`   - Android Interface Definition Language
- `L2`     - Level 2 Testing
- `L3`     - Level 3 Testing
- `NA`     - Not Applicable
- `Caller` - Any user of the interface via the `APIs`
- `CB`     - Call-back function (suffix)
- `RDK`    - Reference Design Kit
- `EOS`    - End of Stream

## Introduction

This document provides an overview of the testing requirements for the Video Sink module. It outlines the scope of testing, objectives, testing levels and approaches, specific test requirements.

Video Sink Interface Definitions - [Video Sink HAL](https://github.com/rdkcentral/rdk-halif-aidl/tree/v0.6/videosink/current/com/rdk/hal/videosink)

## Module Description

The Video Sink `HAL` service provides the interfaces for the video frame queue and deliver video frames to a video plane under the timing of the vendor AV sync.

The Input to a Video sink can be from two ways:
1. **Non-tunnel mode** - Decoded video frame buffer from RDK pipeline
2. **Tunnel mode** - Video frame queue from video decoder.

## Test Streams Specification

Test streams are local data files for the test suite to inject directly into the `HAL`.

They are read by the test suite, written into buffers and passed into the Video Sink.

The naming convention for the test streams is as follows:

```bash

<Description>_<Samplerate>_<channel>_<Bitrate kbps>.<extension> (elementary stream file)

```

The following test streams will be used for validation:

|#|Stream Name|Description |
|-|-----------|------------|
||||


## L2 Test Specification

|Test|Description|HAL APIs|
|----|-----------|--------|
|Set and Get Video Sink Properties|<ul><li>Read available Video Sinks using `IVideoSinkManager` class</li><li>Loop through available video sinks</li><li>set read-write properties (`Property`) of video sink and verify using get function</li><li>get read only properties of video sink and verify with default/supported values read from `hfp_videosink.yaml`</li></ul>|<ul><li>IVideoSinkManager - getVideoSinkIds, getVideoSink</li><li>IVideoSink - open, getProperty, close </li><li>IVideoSinkController - setProperty</li></ul>|
|States of VideoSink|<ul><li>Read supported video sinks using `IVideoSinkManager` class</li><li>Loop through supported video sinks</li><li>Register event listeners using `registerEventListener`</li><li>Ensure state transitions (`open -> start -> stop -> close`) work as expected using `getState`. Validate event listener notifications using `onStateChanged`.</li></ul>|<ul><li>IVideoSinkManager - getVideoSinkIds, getVideoSink</li><li>IVideoSink - open, getState, registerEventListener, unregisterEventListener, close </li><li>IVideoSinkController - start, stop, flush</li></ul>|

## L3 Test Specification

|Test|Description|HAL APIs|Test Stream|
|----|-----------|--------|-----------|
|Render Video FirstFrame|<ul><li>Read supported video sinks using `IVideoSinkManager` class</li><li>Loop through supported video sinks</li><li>`open` video sink</li><li>register control and event listeners</li><li>Provide first video frames to opened video sink instance</li><li>`start` the video sink</li> <li>Queue the video frame using `queueVideoFrame` in the sinkController</li><li>Verify the first frame received from `onFirstFrameRendered`</li><li>After verification `stop` and `close` the sink</li></ul>|<ul><li>IVideoSinkManager - getVideoSinkIds, getVideoSink</li><li>IVideoSink - open, registerEventListener, close </li><li>IVideoSinkController - start, queueVideoFrame, stop<li>IVideoSinkControllerListener - onFirstFrameRendered<li>IAVBuffer - createVideoPool, alloc, free, destroyPool</li></ul>|
|End of Video Stream|<ul><li>Read supported video sinks using `IVideoSinkManager` class</li><li>Loop through supported video sinks</li><li>`open` video sink</li><li>register control and event listeners</li><li>Provide End of stream frame to opened video sink instance</li><li>`start` the video sink</li> <li>Queue the video frame using `queueVideoFrame` in the sinkController</li><li>Add frames to teh queue till end of the stream.</li><li>Verify the End of stream frame received from `onEndOfStream`</li><li>After verification `stop` and `close` the sink</li></ul>|<ul><li>IVideoSinkManager - getVideoSinkIds, getVideoSink</li><li>IVideoSink - open, registerEventListener, close </li><li>IVideoSinkController - start, queueVideoFrame, stop<li>IVideoSinkControllerListener - onEndOfStream<li>IAVBuffer - createVideoPool, alloc, free, destroyPool</li></ul>|
|Video Underflow|<ul><li>Read supported video sinks using `IVideoSinkManager` class</li><li>Loop through supported video sinks</li><li>`open` video sink</li><li>register control and event listeners</li><li>Provide underflow frame to opened video sink instance</li><li>`start` `queueVideoFrame` the sink</li><li>Verify the End of stream frame received from `onVideoUnderflow`</li><li>After verification `stop` and `close` the sink</li></ul>|<ul><li>IVideoSinkManager - getVideoSinkIds, getVideoSink</li><li>IVideoSink - open, registerEventListener, close </li><li>IVideoSinkController - start, queueVideoFrame, stop<li>IVideoSinkControllerListener - onVideoUnderflow<li>IAVBuffer - createVideoPool, alloc, free, destroyPool</li></ul>|
|Video Resumed|<ul><li>Read supported video sinks using `IVideoSinkManager` class</li><li>Loop through supported video sinks</li><li>`open` video sink</li><li>register control and event listeners</li><li>Provide video frame to opened video sink instance</li><li>`start` the video sink</li> <li>Queue the video frame using `queueVideoFrame` in the sinkController</li><li>Verify the Video Resumed received from `onVideoResumed`</li><li>After verification `stop` and `close` the sink</li></ul>|<ul><li>IVideoSinkManager - getVideoSinkIds, getVideoSink</li><li>IVideoSink - open, registerEventListener, close </li><li>IVideoSinkController - start, queueVideoFrame, stop<li>IVideoSinkControllerListener - onVideoUnderflow<li>IAVBuffer - createVideoPool, alloc, free, destroyPool</li></ul>|
|Video Flush|<ul><li>Read supported video sinks using `IVideoSinkManager` class</li><li>Loop through supported video sinks</li><li>`open` video sink</li><li>register control and event listeners</li><li>Provide video frame to opened video sink instance</li><li>`start` the video sink</li> <li>Queue the video frame using `queueVideoFrame` in the sinkController</li><li>Verify the Video flush received from `onFlushComplete`</li><li>After verification `stop` and `close` the sink</li></ul>|<ul><li>IVideoSinkManager - getVideoSinkIds, getVideoSink</li><li>IVideoSink - open, registerEventListener, close </li><li>IVideoSinkController - start, queueVideoFrame, stop<li>IVideoSinkControllerListener - onVideoUnderflow<li>IAVBuffer - createVideoPool, alloc, free, destroyPool</li></ul>|