/*
Copyright 2025 Google Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "vk_rt_layer_impl.h"

#include <cstdio>
#include <cstdlib>
#if defined(__ANDROID__)
#    include <dlfcn.h>
#endif

#include <vulkan/vulkan_core.h>
#include "capture_service/log.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

namespace DiveLayer
{

static bool sEnableDrawcallReport = false;
static bool sEnableDrawcallLimit = false;
static bool sEnableDrawcallFilter = false;
static bool sRemoveImageFlagFDMOffset = false;
static bool sRemoveImageFlagSubSampled = false;

static uint32_t sDrawcallCounter = 0;
static size_t   sTotalIndexCounter = 0;

static constexpr uint32_t kDrawcallCountLimit = 300;
static constexpr uint32_t kVisibilityMaskIndexCount = 42;
static constexpr uint32_t kQueryCount = 64;

// FrameMetrics
void FrameMetrics::NewFrame()
{
    // TODO(wangra): we should be able to get the total frame count
    m_frame_data.push_back({});
}

void FrameMetrics::AddCmdTime(size_t cmd_index, double time)
{
    if (m_frame_data.empty())
    {
        LOGI("Frame data should not be empty! \n");
        return;
    }
    auto& last_frame = m_frame_data.back();
    if (m_frame_data.size() == 1)
    {
        // we update the render pass count only for the 1st frame
        if (last_frame.size() <= cmd_index)
        {
            last_frame.resize(cmd_index + 1);
        }
    }
    else
    {
        if (last_frame.empty())
        {
            last_frame.resize(m_frame_data[0].size());
        }

        if (cmd_index >= last_frame.size())
        {
            LOGI("Command buffer count doesn't match last frame! \n");
            return;
        }
    }
    last_frame[cmd_index] = time;
}

uint32_t FrameMetrics::GetCmdCount() const
{
    if (m_frame_data.empty())
        return 0;
    return static_cast<uint32_t>(m_frame_data[0].size());
}

FrameMetrics::PassStats FrameMetrics::GetCmdStatistics(int cmd_index) const
{
    PassStats stats;
    stats.min = std::numeric_limits<double>::max();     // Initialize min to max value
    stats.max = std::numeric_limits<double>::lowest();  // Initialize max to lowest value

    for (const auto& frame : m_frame_data)
    {
        double time = frame[cmd_index];
        stats.min = std::min(stats.min, time);  // Update min
        stats.max = std::max(stats.max, time);  // Update max
    }

    stats.average = CalculateAverage(cmd_index);  // Call CalculateAverage directly
    stats.stddev = CalculateStdDev(cmd_index, stats.average);

    return stats;
}

void FrameMetrics::PrintStats(const FrameMetrics::PassStats& stats)
{
    LOGI("FrameMetrics: \n");
    LOGI("  Min: %f ms \n", stats.min);
    LOGI("  Max: %f ms \n", stats.max);
    LOGI("  Mean: %f ms \n", stats.average);
    LOGI("  Std: %f ms \n", stats.stddev);
}

double FrameMetrics::CalculateAverage(int cmd_index) const
{
    if (m_frame_data.empty())
    {
        return 0.0;
    }

    double sum = 0.0;
    for (const auto& frame : m_frame_data)
    {
        sum += frame[cmd_index];
    }
    return sum / m_frame_data.size();
}

double FrameMetrics::CalculateStdDev(int cmd_index, double average) const
{
    if (m_frame_data.size() < 2)
    {
        return 0.0;
    }
    double variance = 0.0;
    for (const auto& frame : m_frame_data)
    {
        double time = frame[cmd_index];
        variance += (time - average) * (time - average);
    }
    variance /= (m_frame_data.size() - 1);
    return std::sqrt(variance);
}

// DiveRuntimeLayer
DiveRuntimeLayer::DiveRuntimeLayer() :
    m_device_proc_addr(nullptr),
    m_query_pool(VK_NULL_HANDLE),
    m_device(VK_NULL_HANDLE),
    m_timestamp_period(0.0f),
    m_timestamp_counter(0),
    m_allocator(nullptr),
    m_first_frame_done(false)
{
}

DiveRuntimeLayer::~DiveRuntimeLayer() {}

VkResult DiveRuntimeLayer::QueuePresentKHR(PFN_vkQueuePresentKHR   pfn,
                                           VkQueue                 queue,
                                           const VkPresentInfoKHR* pPresentInfo)
{
    return pfn(queue, pPresentInfo);
}

VkResult DiveRuntimeLayer::CreateImage(PFN_vkCreateImage            pfn,
                                       VkDevice                     device,
                                       const VkImageCreateInfo*     pCreateInfo,
                                       const VkAllocationCallbacks* pAllocator,
                                       VkImage*                     pImage)
{
    // Remove VK_IMAGE_CREATE_FRAGMENT_DENSITY_MAP_OFFSET_BIT_QCOM flag
    if (sRemoveImageFlagFDMOffset &&
        ((pCreateInfo->flags & VK_IMAGE_CREATE_FRAGMENT_DENSITY_MAP_OFFSET_BIT_QCOM) != 0))
    {
        LOGI("Image %p CreateImage has the density map offset flag! \n", pImage);
        const_cast<VkImageCreateInfo*>(pCreateInfo)
        ->flags &= ~VK_IMAGE_CREATE_FRAGMENT_DENSITY_MAP_OFFSET_BIT_QCOM;
    }

    if (sRemoveImageFlagSubSampled &&
        ((pCreateInfo->flags & VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT) != 0))
    {
        LOGI("Image %p CreateImage has the subsampled bit flag! \n", pImage);
        const_cast<VkImageCreateInfo*>(pCreateInfo)->flags &= ~VK_IMAGE_CREATE_SUBSAMPLED_BIT_EXT;
    }

    return pfn(device, pCreateInfo, pAllocator, pImage);
}

void DiveRuntimeLayer::CmdDrawIndexed(PFN_vkCmdDrawIndexed pfn,
                                      VkCommandBuffer      commandBuffer,
                                      uint32_t             indexCount,
                                      uint32_t             instanceCount,
                                      uint32_t             firstIndex,
                                      int32_t              vertexOffset,
                                      uint32_t             firstInstance)
{
    // Disable drawcalls with N index count
    // Specifically for visibility mask:
    // BiRP is using 2 drawcalls with 42 each, URP is using 1 drawcall with 84,
    if (sEnableDrawcallFilter && ((indexCount == kVisibilityMaskIndexCount) ||
                                  (indexCount == kVisibilityMaskIndexCount * 2)))
    {
        LOGI("Skip drawcalls with index count of %d & %d\n",
             kVisibilityMaskIndexCount,
             kVisibilityMaskIndexCount * 2);
        return;
    }

    ++sDrawcallCounter;
    sTotalIndexCounter += indexCount;

    if (sEnableDrawcallLimit && (sDrawcallCounter > kDrawcallCountLimit))
    {
        return;
    }

    return pfn(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

VkResult DiveRuntimeLayer::BeginCommandBuffer(PFN_vkBeginCommandBuffer        pfn,
                                              VkCommandBuffer                 commandBuffer,
                                              const VkCommandBufferBeginInfo* pBeginInfo)
{
    if (sEnableDrawcallReport)
    {
        LOGI("Drawcall count: %d\n", sDrawcallCounter);
        LOGI("Total index count: %zd\n", sTotalIndexCounter);
    }

    sDrawcallCounter = 0;
    sTotalIndexCounter = 0;

    if (m_timestamp_counter == 0)
    {
        PFN_vkCmdResetQueryPool CmdResetQueryPool = (PFN_vkCmdResetQueryPool)
        m_device_proc_addr(m_device, "vkCmdResetQueryPool");

        LOGI("CmdResetQueryPool: %p\n", CmdResetQueryPool);
        CmdResetQueryPool(commandBuffer, m_query_pool, 0, kQueryCount);
        LOGI("Reset Query Pool End\n");
    }

    PFN_vkCmdWriteTimestamp CmdWriteTimestamp = (PFN_vkCmdWriteTimestamp)
    m_device_proc_addr(m_device, "vkCmdWriteTimestamp");
    CmdWriteTimestamp(commandBuffer,
                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      m_query_pool,
                      m_timestamp_counter);
    ++m_timestamp_counter;

    return pfn(commandBuffer, pBeginInfo);
}

VkResult DiveRuntimeLayer::EndCommandBuffer(PFN_vkEndCommandBuffer pfn,
                                            VkCommandBuffer        commandBuffer)
{
    PFN_vkCmdWriteTimestamp CmdWriteTimestamp = (PFN_vkCmdWriteTimestamp)
    m_device_proc_addr(m_device, "vkCmdWriteTimestamp");
    CmdWriteTimestamp(commandBuffer,
                      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                      m_query_pool,
                      m_timestamp_counter);
    ++m_timestamp_counter;
    return pfn(commandBuffer);
}

VkResult DiveRuntimeLayer::CreateDevice(PFN_vkGetDeviceProcAddr      pa,
                                        PFN_vkCreateDevice           pfn,
                                        float                        timestampPeriod,
                                        VkPhysicalDevice             physicalDevice,
                                        const VkDeviceCreateInfo*    pCreateInfo,
                                        const VkAllocationCallbacks* pAllocator,
                                        VkDevice*                    pDevice)
{

    m_allocator = pAllocator;

    VkResult result = pfn(physicalDevice, pCreateInfo, pAllocator, pDevice);
    m_device = *pDevice;

    m_device_proc_addr = pa;

    if (result == VK_SUCCESS)
    {
        PFN_vkCreateQueryPool CreateQueryPool = (PFN_vkCreateQueryPool)pa(m_device,
                                                                          "vkCreateQueryPool");

        // Create a query pool for timestamps
        VkQueryPoolCreateInfo queryPoolInfo{};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = kQueryCount;

        result = CreateQueryPool(m_device, &queryPoolInfo, m_allocator, &m_query_pool);
        m_timestamp_period = timestampPeriod;
    }

    return result;
}

void DiveRuntimeLayer::DestroyDevice(PFN_vkDestroyDevice          pfn,
                                     VkDevice                     device,
                                     const VkAllocationCallbacks* pAllocator)
{
    if (device != m_device)
    {
        LOGI("Wrong device while destroying device!!!");
    }

    if ((m_device != VK_NULL_HANDLE) && (m_query_pool != VK_NULL_HANDLE))
    {
        if (m_device_proc_addr == nullptr)
        {
            LOGI("Device proc addr is nullptr!!!");
            return;
        }

        PFN_vkQueueWaitIdle QueueWaitIdle = (PFN_vkQueueWaitIdle)
        m_device_proc_addr(m_device, "vkQueueWaitIdle");

        if (m_queues.empty())
        {
            LOGI("vk queue is empty!");
        }

        for (auto& q : m_queues)
        {
            QueueWaitIdle(q);
        }
        m_queues.clear();

        PFN_vkDestroyQueryPool DestroyQueryPool = (PFN_vkDestroyQueryPool)
        m_device_proc_addr(m_device, "vkDestroyQueryPool");

        DestroyQueryPool(m_device, m_query_pool, m_allocator);
        m_query_pool = VK_NULL_HANDLE;
        m_allocator = nullptr;
    }

    m_device = VK_NULL_HANDLE;

    pfn(device, pAllocator);
}

VkResult DiveRuntimeLayer::AcquireNextImageKHR(PFN_vkAcquireNextImageKHR pfn,
                                               VkDevice                  device,
                                               VkSwapchainKHR            swapchain,
                                               uint64_t                  timeout,
                                               VkSemaphore               semaphore,
                                               VkFence                   fence,
                                               uint32_t*                 pImageIndex)
{
    VkResult result = pfn(device, swapchain, timeout, semaphore, fence, pImageIndex);

    // Get the timestamp results
    uint64_t timestamps[kQueryCount];

    if (m_first_frame_done)
    {
        PFN_vkGetQueryPoolResults GetQueryPoolResults = (PFN_vkGetQueryPoolResults)
        m_device_proc_addr(device, "vkGetQueryPoolResults");
        GetQueryPoolResults(device,
                            m_query_pool,
                            0,
                            m_timestamp_counter,
                            sizeof(uint64_t) * m_timestamp_counter,
                            timestamps,
                            sizeof(uint64_t),
                            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        uint32_t timestamp_pair = m_timestamp_counter / 2;

        m_metrics.NewFrame();
        double frameTime = 0.0;
        for (uint32_t i = 0; i < timestamp_pair; ++i)
        {
            // Calculate the elapsed time in nanoseconds
            uint64_t elapsedTime = timestamps[2 * i + 1] - timestamps[2 * i];
            double   elapsedTimeInMS = elapsedTime * m_timestamp_period * 0.000001;
            LOGI("Elapsed time: %f ms \n", elapsedTimeInMS);
            m_metrics.AddCmdTime(i, elapsedTimeInMS);
            frameTime += elapsedTimeInMS;
        }
        LOGI("GPU Frame time: %f \n\n", frameTime);
    }

    m_first_frame_done = true;

    m_timestamp_counter = 0;

    return result;
}

void DiveRuntimeLayer::GetDeviceQueue2(PFN_vkGetDeviceQueue2     pfn,
                                       VkDevice                  device,
                                       const VkDeviceQueueInfo2* pQueueInfo,
                                       VkQueue*                  pQueue)
{
    pfn(device, pQueueInfo, pQueue);
    if (pQueue != nullptr)
    {
        m_queues.insert(*pQueue);
    }
}

void DiveRuntimeLayer::GetDeviceQueue(PFN_vkGetDeviceQueue pfn,
                                      VkDevice             device,
                                      uint32_t             queueFamilyIndex,
                                      uint32_t             queueIndex,
                                      VkQueue*             pQueue)
{
    pfn(device, queueFamilyIndex, queueIndex, pQueue);
    if (pQueue != nullptr)
    {
        m_queues.insert(*pQueue);
    }
}

void DiveRuntimeLayer::CmdInsertDebugUtilsLabel(PFN_vkCmdInsertDebugUtilsLabelEXT pfn,
                                                VkCommandBuffer                   commandBuffer,
                                                const VkDebugUtilsLabelEXT*       pLabelInfo)
{
    LOGI("Debug Label: %s", pLabelInfo->pLabelName);
    pfn(commandBuffer, pLabelInfo);
}

}  // namespace DiveLayer