/**
 * Copyright (C) 2019 Xilinx, Inc
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
#include "xrtexec.hpp"
#include "xrt/device/device.h"
#include "xrt/scheduler/command.h"

namespace xrtcpp {

void
acquire_cu_context(xrt_device* device, value_type cuidx)
{
  auto xdevice = static_cast<xrt::device*>(device);
  xdevice->acquire_cu_context(cuidx,true);
}

void
release_cu_context(xrt_device* device, value_type cuidx)
{
  auto xdevice = static_cast<xrt::device*>(device);
  xdevice->release_cu_context(cuidx);
}

xrt::device::device_handle
get_device_handle(const xrt_device* device)
{
  auto xdevice = static_cast<const xrt::device*>(device);
  return xdevice->get_handle();
}

namespace exec {

struct command::impl : xrt::command
{
  impl(xrt::device* device, ert_cmd_opcode opcode)
    : xrt::command(device,opcode)
      //    , ecmd(get_ert_cmd<ert_packet*>())
  {
    ecmd = get_ert_cmd<ert_packet*>();
  }

  ert_packet* ecmd = nullptr;
};

command::
command(xrt_device* device, ert_cmd_opcode opcode)
  : m_impl(std::make_shared<impl>(static_cast<xrt::device*>(device),opcode))
{}

void
command::
execute()
{
  m_impl->execute();
}

void
command::
wait()
{
  m_impl->wait();
}

bool
command::
completed() const
{
  return m_impl->completed();
}

ert_cmd_state
command::
state() const
{
  return static_cast<ert_cmd_state>(m_impl->ecmd->state);
}

exec_cu_command::
exec_cu_command(xrt_device* device)
  : command(device,ERT_START_CU)
{
  m_impl->ecmd->type = ERT_CU;
  clear();
}

void
exec_cu_command::
clear()
{
  // clear cu
  auto skcmd = reinterpret_cast<ert_start_kernel_cmd*>(m_impl->ecmd);
  skcmd->cu_mask = 0;

  // clear payload since this command type is random write
  std::memset(m_impl->ecmd->data,0,m_impl->ecmd->count);

  // clear payload count
  m_impl->ecmd->count = 1 + 4; // cumask + 4 ctrl
}

void
exec_cu_command::
add_cu(value_type cuidx)
{
  if (cuidx>=32)
    throw std::runtime_error("write_exec supports at most 32 CUs");
  auto skcmd = reinterpret_cast<ert_start_kernel_cmd*>(m_impl->ecmd);
  skcmd->cu_mask |= 1<<cuidx;
}

void
exec_cu_command::
add(index_type idx, value_type value)
{
  const auto skip = 2; // header and cumask
  (*m_impl)[skip+idx] = value;
  m_impl->ecmd->count = std::max(m_impl->ecmd->count,skip+idx);
}

exec_write_command::
exec_write_command(xrt_device* device)
  : command(device,ERT_EXEC_WRITE)
{
  m_impl->ecmd->type = ERT_CU;
  clear();
}

void
exec_write_command::
add_cu(value_type cuidx)
{
  if (cuidx>=32)
    throw std::runtime_error("write_exec supports at most 32 CUs");
  auto skcmd = reinterpret_cast<ert_start_kernel_cmd*>(m_impl->ecmd);
  skcmd->cu_mask |= 1<<cuidx;
}

void
exec_write_command::
add_ctx(uint32_t ctx)
{
  if (ctx >= 32)
    throw std::runtime_error("write_exec supports at most 32 contexts numbered 0 through 31");
  auto skcmd = reinterpret_cast<ert_start_kernel_cmd*>(m_impl->ecmd);
  skcmd->data[0x10 >> 2] = ctx;
}

void
exec_write_command::
add(addr_type addr, value_type value)
{
  (*m_impl)[++m_impl->ecmd->count] = addr;
  (*m_impl)[++m_impl->ecmd->count] = value;
}

void
exec_write_command::
clear()
{
  // clear cu
  auto skcmd = reinterpret_cast<ert_start_kernel_cmd*>(m_impl->ecmd);
  skcmd->cu_mask = 0;

  // clear payload
  m_impl->ecmd->count = 1 + 4 + 2; // cumask + 4 ctrl + 2 ctx
}

}} // exec,xrt
