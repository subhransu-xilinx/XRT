/**
 * Copyright (C) 2016-2017 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */

#ifndef __XDP_DEVICE_TRACE_PARSER_H
#define __XDP_DEVICE_TRACE_PARSER_H

#include <limits>
#include <cstdint>
#include <set>
#include <vector>
#include <queue>
#include <string>
#include <fstream>
#include <CL/opencl.h>
#include "xclperf.h"
#include "../collection/results.h"
#include "../config.h"
#include "xdp/profile/plugin/base_plugin.h"
#include "xdp/profile/core/rt_util.h"

namespace xdp {
  class DeviceTrace;

  class TraceParser {
    public:
      TraceParser(XDPPluginI* Plugin);
      ~TraceParser();

      typedef std::vector<DeviceTrace> TraceResultVector;

    public:
      // get functions
      uint32_t getTraceSamplesThreshold() {return mTraceSamplesThreshold;}
      uint32_t getSampleIntervalMsec() {return mSampleIntervalMsec;}
      double getDeviceClockFreqMHz() {return mDeviceClockRateMHz;}
      double getGlobalMemoryClockFreqMHz() {return mGlobalMemoryClockRateMHz;}
      uint32_t getGlobalMemoryBitWidth() {return mGlobalMemoryBitWidth;}
      double getTraceClockFreqMHz() {
        // for most platforms, this is a 300 MHz system clock
        return mTraceClockRateMHz;
      }

      // set functions
      void setStartTimeMsec(double startTimeMsec) {
        mStartTimeNsec = (uint64_t)((startTimeMsec + PCIE_DELAY_OFFSET_MSEC) * 1.0e6);
      }

      void setKernelClockFreqMHz(const std::string& deviceName, unsigned int clockRateMHz) {
    	  // In 2017.4, trace events are captured at the kernel clock
    	  setTraceClockFreqMHz(clockRateMHz);
      }
      void setDeviceClockFreqMHz(double clockRateMHz) {
    	  mDeviceClockRateMHz = clockRateMHz;
      }
      void setTraceClockFreqMHz(double clockRateMHz) {
        mTraceClockRateMHz = clockRateMHz;

        // Update slope for conversion between device and host
        for (int i=0; i < XCL_PERF_MON_TOTAL_PROFILE; i++)
          mTrainSlope[i] = 1000.0 / clockRateMHz;
      }
      void setGlobalMemoryClockFreqMHz(double clockRateMHz) {
        mGlobalMemoryClockRateMHz = clockRateMHz;
      }
      void setGlobalMemoryBitWidth(uint32_t bitWidth) {
        XDP_LOG("[rt_device_profile] Setting global memory bit width to %d\n", bitWidth);
        mGlobalMemoryBitWidth = bitWidth;
      }

      // log trace results
      void logTrace(const std::string& deviceName, xclPerfMonType type,
          xclTraceResultsVector& traceVector, TraceResultVector& resultVector);
      void endLogTrace(const std::string& deviceName, xclPerfMonType type,
          TraceResultVector& resultVector);
      void logTraceHWEmu(const std::string& deviceName,
          xclTraceResultsVector& traceVector, TraceResultVector& resultVector);

    private:
      // Device/host timestamps: training and conversion
      void trainDeviceHostTimestamps(const std::string& deviceName, xclPerfMonType type);
      double convertDeviceToHostTimestamp(uint64_t deviceTimestamp, xclPerfMonType type,
          const std::string& deviceName);

      // Get timestamp in nsec
      // NOTE: this is only used for HW emulation
      uint64_t getTimestampNsec(uint64_t timeNsec) {
        static uint64_t firstTimeNsec = timeNsec;
        return (timeNsec - firstTimeNsec + mStartTimeNsec);
      }
      void ResetState();

    private:
      const double PCIE_DELAY_OFFSET_MSEC;
      uint32_t mCuEventID;
      uint32_t mGlobalMemoryBitWidth;
      uint32_t mTraceSamplesThreshold;
      uint32_t mSampleIntervalMsec;
      uint64_t mStartTimeNsec;
      uint64_t mNumTraceEvents;
      uint64_t mMaxTraceEventsHwEm;
      double mTraceClockRateMHz;
      double mDeviceClockRateMHz;
      double mGlobalMemoryClockRateMHz;
      double mEmuTraceMsecOneCycle;
      double mTrainSlope[XCL_PERF_MON_TOTAL_PROFILE];
      double mTrainOffset[XCL_PERF_MON_TOTAL_PROFILE];
      double mTrainProgramStart[XCL_PERF_MON_TOTAL_PROFILE];
      uint64_t mAccelMonCuTime[XAM_MAX_NUMBER_SLOTS]       = { 0 };
      uint64_t mAccelMonCuHostTime[XAM_MAX_NUMBER_SLOTS]   = { 0 };
      uint64_t mAccelMonStallIntTime[XAM_MAX_NUMBER_SLOTS] = { 0 };
      uint64_t mAccelMonStallStrTime[XAM_MAX_NUMBER_SLOTS] = { 0 };
      uint64_t mAccelMonStallExtTime[XAM_MAX_NUMBER_SLOTS] = { 0 };
      uint8_t mAccelMonStartedEvents[XAM_MAX_NUMBER_SLOTS] = { 0 };
      uint64_t mPerfMonLastTranx[XAIM_MAX_NUMBER_SLOTS]     = { 0 };
      uint64_t mAccelMonLastTranx[XAM_MAX_NUMBER_SLOTS]    = { 0 };
      uint64_t mStreamMonLastTranx[XASM_MAX_NUMBER_SLOTS]  = { 0 };
      std::list<uint64_t> mWriteStarts[XAIM_MAX_NUMBER_SLOTS];
      std::list<uint64_t> mHostWriteStarts[XAIM_MAX_NUMBER_SLOTS];
      std::list<uint64_t> mReadStarts[XAIM_MAX_NUMBER_SLOTS];
      std::list<uint64_t> mHostReadStarts[XAIM_MAX_NUMBER_SLOTS];
      std::list<uint64_t> mStreamTxStarts[XASM_MAX_NUMBER_SLOTS];
      std::list<uint64_t> mStreamStallStarts[XASM_MAX_NUMBER_SLOTS];
      std::list<uint64_t> mStreamStarveStarts[XASM_MAX_NUMBER_SLOTS];
      std::list<uint64_t> mStreamTxStartsHostTime[XASM_MAX_NUMBER_SLOTS];
      std::list<uint64_t> mStreamStallStartsHostTime[XASM_MAX_NUMBER_SLOTS];
      std::list<uint64_t> mStreamStarveStartsHostTime[XASM_MAX_NUMBER_SLOTS];
      std::list<uint64_t> mAccelMonCuStarts[XAM_MAX_NUMBER_SLOTS];

    private:
      bool mclockTrainingdone = false;
      uint64_t packets_parsed = 0;

    private:
      XDPPluginI* mPluginHandle;
  };

} // xdp

#endif
