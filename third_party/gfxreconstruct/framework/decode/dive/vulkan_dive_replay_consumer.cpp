/*
** Copyright (c) 2018-2020 Valve Corporation
** Copyright (c) 2018-2023 LunarG, Inc.
** Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
**
** Permission is hereby granted, free of charge, to any person obtaining a
** copy of this software and associated documentation files (the "Software"),
** to deal in the Software without restriction, including without limitation
** the rights to use, copy, modify, merge, publish, distribute, sublicense,
** and/or sell copies of the Software, and to permit persons to whom the
** Software is furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
** DEALINGS IN THE SOFTWARE.
*/

#include "decode/dive/vulkan_dive_replay_consumer.h"
#include "vulkan/vulkan_core.h"

#include <iostream>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <limits> 

static constexpr uint32_t kQueryCount = 64;

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

// FrameMetrics
void FrameMetrics::NewFrame()
{
    // TODO(wangra): we should be able to get the total frame count
    frame_data_.push_back({});
}

void FrameMetrics::AddRenderPassTime(int pass_index, double time)
{
    assert(!frame_data_.empty());
    auto& last_frame = frame_data_.back();
    if (frame_data_.size() == 1)
    {
        // we update the render pass count only for the 1st frame
        if (last_frame.size() <= pass_index)
        {
            last_frame.resize(pass_index + 1);
        }
    }
    else
    {
        if (last_frame.empty())
        {
            last_frame.resize(frame_data_[0].size());
        }
        assert(pass_index < last_frame.size());
    }
    last_frame[pass_index] = time;
}

uint32_t FrameMetrics::GetPassCount() const
{
    if (frame_data_.empty())
        return 0;
    return static_cast<uint32_t>(frame_data_[0].size());
}

FrameMetrics::PassStats FrameMetrics::GetPassStatistics(int pass_index) const
{
    PassStats stats;
    stats.min = std::numeric_limits<double>::max();    // Initialize min to max value
    stats.max = std::numeric_limits<double>::lowest(); // Initialize max to lowest value

    for (const auto& frame : frame_data_)
    {
        double time = frame[pass_index];
        stats.min   = std::min(stats.min, time); // Update min
        stats.max   = std::max(stats.max, time); // Update max
    }

    stats.average = CalculateAverage(pass_index); // Call CalculateAverage directly
    stats.stddev  = CalculateStdDev(pass_index, stats.average);

    return stats;
}

void FrameMetrics::PrintStats(const FrameMetrics::PassStats& stats) 
{
    std::cout << "FrameMetrics: " << std::endl;
    std::cout << "  Min:  " << stats.min << " ms" << std::endl;
    std::cout << "  Max:  " << stats.max << " ms" << std::endl;
    std::cout << "  Mean: " << stats.average << " ms" << std::endl;
    std::cout << "  Std:  " << stats.stddev << " ms" << std::endl;
}

double FrameMetrics::CalculateAverage(int pass_index) const
{
    if (frame_data_.empty())
    {
        return 0.0;
    }

    double sum = 0.0;
    for (const auto& frame : frame_data_)
    {
        sum += frame[pass_index];
    }
    return sum / frame_data_.size();
}

double FrameMetrics::CalculateStdDev(int pass_index, double average) const
{
    if (frame_data_.size() < 2)
    {
        return 0.0;
    }
    double variance = 0.0;
    for (const auto& frame : frame_data_)
    {
        double time = frame[pass_index];
        variance += (time - average) * (time - average);
    }
    variance /= (frame_data_.size() - 1);
    return std::sqrt(variance);
}


VulkanDiveReplayConsumer::VulkanDiveReplayConsumer(std::shared_ptr<application::Application> application,
                                                   const VulkanReplayOptions&                options) :
    VulkanReplayConsumer(application, options), query_pool_(VK_NULL_HANDLE), device_(VK_NULL_HANDLE), timestamp_period_(0.0f), timestamp_counter_(0),
    first_frame_done(false), allocator_(nullptr)
{
}

VulkanDiveReplayConsumer::~VulkanDiveReplayConsumer() 
{
    DestroyQueryPool();
}

void VulkanDiveReplayConsumer::Process_vkCreateDevice(const ApiCallInfo&                                call_info,
                                                      VkResult                                          returnValue,
                                                      format::HandleId                                  physicalDevice,
                                                      StructPointerDecoder<Decoded_VkDeviceCreateInfo>* pCreateInfo,
                                                      StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
                                                      HandlePointerDecoder<VkDevice>*                      pDevice)
{
    VulkanReplayConsumer::Process_vkCreateDevice(
        call_info, returnValue, physicalDevice, pCreateInfo, pAllocator, pDevice);

    if (returnValue != VK_SUCCESS)
    {
        return;
    }

    // Create a query pool for timestamps
    VkQueryPoolCreateInfo queryPoolInfo{};
    queryPoolInfo.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolInfo.queryType  = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolInfo.queryCount = kQueryCount;

    device_ = MapHandle<VulkanDeviceInfo>(*(pDevice->GetPointer()), &CommonObjectInfoTable::GetVkDeviceInfo);
    allocator_      = pAllocator->GetPointer();
    VkResult result = GetDeviceTable(device_)->CreateQueryPool(device_, &queryPoolInfo, allocator_, &query_pool_);
    auto                       in_physicalDevice = GetObjectInfoTable().GetVkPhysicalDeviceInfo(physicalDevice);
    VkPhysicalDeviceProperties deviceProperties;
    GetInstanceTable(in_physicalDevice->handle)
        ->GetPhysicalDeviceProperties(in_physicalDevice->handle, &deviceProperties);
    timestamp_period_ = deviceProperties.limits.timestampPeriod;
    std::cout << "Device Name: " << deviceProperties.deviceName << std::endl;
    std::cout << "Device Type: "
              << (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU     ? "Discrete GPU"
                  : deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "Integrated GPU"
                  : deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU    ? "Virtual GPU"
                  : deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU            ? "CPU"
                                                                                          : "Other")
              << std::endl;
    std::cout << "API Version: " << VK_VERSION_MAJOR(deviceProperties.apiVersion) << "."
              << VK_VERSION_MINOR(deviceProperties.apiVersion) << "." << VK_VERSION_PATCH(deviceProperties.apiVersion)
              << std::endl;
}

void VulkanDiveReplayConsumer::Process_vkDestroyDevice(const ApiCallInfo&                                   call_info,
                                                       format::HandleId                                     device,
                                                       StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator)
{
    VkDevice in_device = MapHandle<VulkanDeviceInfo>(device, &CommonObjectInfoTable::GetVkDeviceInfo);
    assert(in_device == device_);

    DestroyQueryPool();

    device_     = VK_NULL_HANDLE;
    VulkanReplayConsumer::Process_vkDestroyDevice(call_info, device, pAllocator);
}

void VulkanDiveReplayConsumer::Process_vkCmdBeginRenderPass(
    const ApiCallInfo&                                   call_info,
    format::HandleId                                     commandBuffer,
    StructPointerDecoder<Decoded_VkRenderPassBeginInfo>* pRenderPassBegin,
    VkSubpassContents                                    contents)
{
    VkCommandBuffer in_commandBuffer =
        MapHandle<VulkanCommandBufferInfo>(commandBuffer, &CommonObjectInfoTable::GetVkCommandBufferInfo);
    
    VulkanReplayConsumer::Process_vkCmdBeginRenderPass(call_info, commandBuffer, pRenderPassBegin, contents);

    GetDeviceTable(in_commandBuffer)
        ->CmdWriteTimestamp(in_commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool_, timestamp_counter_);
    ++timestamp_counter_;
}

void VulkanDiveReplayConsumer::Process_vkCmdEndRenderPass(const ApiCallInfo& call_info, format::HandleId commandBuffer)
{
    VkCommandBuffer in_commandBuffer =
        MapHandle<VulkanCommandBufferInfo>(commandBuffer, &CommonObjectInfoTable::GetVkCommandBufferInfo);
   GetDeviceTable(in_commandBuffer)
        ->CmdWriteTimestamp(in_commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool_, timestamp_counter_);
    ++timestamp_counter_;
    VulkanReplayConsumer::Process_vkCmdEndRenderPass(call_info, commandBuffer);
}

void VulkanDiveReplayConsumer::Process_vkAcquireNextImageKHR(const ApiCallInfo&        call_info,
                                                             VkResult                  returnValue,
                                                             format::HandleId          device,
                                                             format::HandleId          swapchain,
                                                             uint64_t                  timeout,
                                                             format::HandleId          semaphore,
                                                             format::HandleId          fence,
                                                             PointerDecoder<uint32_t>* pImageIndex)
{
    VulkanReplayConsumer::Process_vkAcquireNextImageKHR(
        call_info, returnValue, device, swapchain, timeout, semaphore, fence, pImageIndex);

    // Get the timestamp results
    uint64_t timestamps[kQueryCount];
    VkDevice in_device = MapHandle<VulkanDeviceInfo>(device, &CommonObjectInfoTable::GetVkDeviceInfo);

    if (first_frame_done)
    {
        GetDeviceTable(in_device)->GetQueryPoolResults(in_device,
                                                       query_pool_,
                                                       0,
                                                       timestamp_counter_,
                                                       sizeof(uint64_t) * timestamp_counter_,
                                                       timestamps,
                                                       sizeof(uint64_t),
                                                       VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        uint32_t timestamp_pair = timestamp_counter_ / 2;

        std::cout << "GPU Time Duration Summary Start!!!" << std::endl;
        metrics_.NewFrame();
        for (uint32_t i = 0; i < timestamp_pair; ++i)
        {
            // Calculate the elapsed time in nanoseconds
            uint64_t elapsedTime              = timestamps[2 * i + 1] - timestamps[2 * i];
            double    elapsedTimeInMS = elapsedTime * timestamp_period_ * 0.000001;
            std::cout << "Elapsed time: " << elapsedTimeInMS << " ms" << std::endl;
            metrics_.AddRenderPassTime(i, elapsedTimeInMS);
        }
        std::cout << "GPU Time Duration Summary End Frame!!!" << std::endl << std::endl;
    }
    
    timestamp_counter_ = 0;
}

void VulkanDiveReplayConsumer::Process_vkQueuePresentKHR(const ApiCallInfo&                              call_info,
                                                         VkResult                                        returnValue,
                                                         format::HandleId                                queue,
                                                         StructPointerDecoder<Decoded_VkPresentInfoKHR>* pPresentInfo)
{
    VulkanReplayConsumer::Process_vkQueuePresentKHR(call_info, returnValue, queue, pPresentInfo);
    first_frame_done = true;
}

void VulkanDiveReplayConsumer::Process_vkQueueSubmit(const ApiCallInfo&                          call_info,
    VkResult                                    returnValue,
    format::HandleId                            queue,
    uint32_t                                    submitCount,
    StructPointerDecoder<Decoded_VkSubmitInfo>* pSubmits,
    format::HandleId                            fence)
{
    VulkanReplayConsumer::Process_vkQueueSubmit(call_info, returnValue, queue, submitCount, pSubmits, fence);
}

void VulkanDiveReplayConsumer::Process_vkBeginCommandBuffer(
    const ApiCallInfo& call_info,
    VkResult                                                returnValue,
    format::HandleId                                        commandBuffer,
    StructPointerDecoder<Decoded_VkCommandBufferBeginInfo>* pBeginInfo)
{
    VulkanReplayConsumer::Process_vkBeginCommandBuffer(call_info, returnValue, commandBuffer, pBeginInfo);
    VkCommandBuffer in_commandBuffer =
        MapHandle<VulkanCommandBufferInfo>(commandBuffer, &CommonObjectInfoTable::GetVkCommandBufferInfo);

    if (timestamp_counter_ == 0)
    {
        GetDeviceTable(in_commandBuffer)->CmdResetQueryPool(in_commandBuffer, query_pool_, 0, kQueryCount);
    }
}

void VulkanDiveReplayConsumer::Process_vkEndCommandBuffer(const ApiCallInfo& call_info,
    VkResult           returnValue,
    format::HandleId   commandBuffer)
{
    VkCommandBuffer in_commandBuffer =
        MapHandle<VulkanCommandBufferInfo>(commandBuffer, &CommonObjectInfoTable::GetVkCommandBufferInfo);
    VulkanReplayConsumer::Process_vkEndCommandBuffer(call_info, returnValue, commandBuffer);
}

void VulkanDiveReplayConsumer::Process_vkGetDeviceQueue2(const ApiCallInfo& call_info,
    format::HandleId                                  device,
    StructPointerDecoder<Decoded_VkDeviceQueueInfo2>* pQueueInfo,
    HandlePointerDecoder<VkQueue>* pQueue)
{
    VulkanReplayConsumer::Process_vkGetDeviceQueue2(call_info, device, pQueueInfo, pQueue);
    VkQueue queue = *(pQueue->GetHandlePointer());
    queues_.insert(queue);
}

void VulkanDiveReplayConsumer::Process_vkGetDeviceQueue(const ApiCallInfo& call_info,
    format::HandleId               device,
    uint32_t                       queueFamilyIndex,
    uint32_t                       queueIndex,
    HandlePointerDecoder<VkQueue>* pQueue)
{
    VulkanReplayConsumer::Process_vkGetDeviceQueue(call_info, device, queueFamilyIndex, queueIndex, pQueue);
    VkQueue queue = *(pQueue->GetHandlePointer());
    queues_.insert(queue);
}

void VulkanDiveReplayConsumer::DestroyQueryPool() 
{
    if ((device_ != VK_NULL_HANDLE) && (query_pool_ != VK_NULL_HANDLE))
    {
        const uint32_t pass_count = metrics_.GetPassCount();
        for (uint32_t i = 0; i < pass_count; ++i)
        {
            std::cout << "Render Pass: " << i << std::endl;
            metrics_.PrintStats(metrics_.GetPassStatistics(i));
        }

        assert(!queues_.empty());
        for (auto& q : queues_)
        {
            GetDeviceTable(device_)->QueueWaitIdle(q);
        }
        queues_.clear();

        for (auto& q : queues_)
        {
            GetDeviceTable(device_)->QueueWaitIdle(q);
        }
        GetDeviceTable(device_)->DestroyQueryPool(device_, query_pool_, allocator_);
        query_pool_ = VK_NULL_HANDLE;
        allocator_  = nullptr;
    }
}

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)
