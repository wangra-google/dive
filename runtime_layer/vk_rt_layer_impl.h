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

#pragma once

#include <vulkan/vk_layer.h>
#include <vulkan/vulkan.h>
#include <set>
#include <vector>

namespace DiveLayer
{

class FrameMetrics
{
public:
    // Calculate and return the statistics for a specific pass index
    struct PassStats
    {
        double average;
        double min;
        double max;
        double stddev;
    };

    FrameMetrics() = default;
    // This needs to be called before adding any data to a new frame
    void NewFrame();
    // Add a render pass time for a specific frame and pass index
    void AddCmdTime(size_t cmd_index, double time);

    uint32_t  GetCmdCount() const;
    PassStats GetCmdStatistics(int cmd_index) const;

    void PrintStats(const PassStats& stats);

private:
    // Helper function to calculate the average for a given pass index
    double CalculateAverage(int cmd_index) const;
    // Helper function to calculate the standard deviation for a given pass index
    double CalculateStdDev(int cmd_index, double average) const;

    // Store frame data as a vector of vectors of doubles
    std::vector<std::vector<double>> m_frame_data;
};

class DiveRuntimeLayer
{
public:
    DiveRuntimeLayer();
    ~DiveRuntimeLayer();
    VkResult QueuePresentKHR(PFN_vkQueuePresentKHR   pfn,
                             VkQueue                 queue,
                             const VkPresentInfoKHR* pPresentInfo);

    VkResult CreateImage(PFN_vkCreateImage            pfn,
                         VkDevice                     device,
                         const VkImageCreateInfo*     pCreateInfo,
                         const VkAllocationCallbacks* pAllocator,
                         VkImage*                     pImage);

    void CmdDrawIndexed(PFN_vkCmdDrawIndexed pfn,
                        VkCommandBuffer      commandBuffer,
                        uint32_t             indexCount,
                        uint32_t             instanceCount,
                        uint32_t             firstIndex,
                        int32_t              vertexOffset,
                        uint32_t             firstInstance);

    VkResult BeginCommandBuffer(PFN_vkBeginCommandBuffer        pfn,
                                VkCommandBuffer                 commandBuffer,
                                const VkCommandBufferBeginInfo* pBeginInfo);

    VkResult EndCommandBuffer(PFN_vkEndCommandBuffer pfn, VkCommandBuffer commandBuffer);

    VkResult CreateDevice(PFN_vkGetDeviceProcAddr      pa,
                          PFN_vkCreateDevice           pfn,
                          float                        timestampPeriod,
                          VkPhysicalDevice             physicalDevice,
                          const VkDeviceCreateInfo*    pCreateInfo,
                          const VkAllocationCallbacks* pAllocator,
                          VkDevice*                    pDevice);

    void DestroyDevice(PFN_vkDestroyDevice          pfn,
                       VkDevice                     device,
                       const VkAllocationCallbacks* pAllocator);

    VkResult AcquireNextImageKHR(PFN_vkAcquireNextImageKHR pfn,
                                 VkDevice                  device,
                                 VkSwapchainKHR            swapchain,
                                 uint64_t                  timeout,
                                 VkSemaphore               semaphore,
                                 VkFence                   fence,
                                 uint32_t*                 pImageIndex);

    void GetDeviceQueue2(PFN_vkGetDeviceQueue2     pfn,
                         VkDevice                  device,
                         const VkDeviceQueueInfo2* pQueueInfo,
                         VkQueue*                  pQueue);

    void GetDeviceQueue(PFN_vkGetDeviceQueue pfn,
                        VkDevice             device,
                        uint32_t             queueFamilyIndex,
                        uint32_t             queueIndex,
                        VkQueue*             pQueue);

    void CmdInsertDebugUtilsLabel(PFN_vkCmdInsertDebugUtilsLabelEXT pfn,
                                  VkCommandBuffer                   commandBuffer,
                                  const VkDebugUtilsLabelEXT*       pLabelInfo);

private:
    PFN_vkGetDeviceProcAddr m_device_proc_addr;

    VkQueryPool                  m_query_pool;
    VkDevice                     m_device;
    std::set<VkQueue>            m_queues;
    float                        m_timestamp_period;
    uint32_t                     m_timestamp_counter;
    const VkAllocationCallbacks* m_allocator;
    FrameMetrics                 m_metrics;
    bool                         m_first_frame_done;
};

}  // namespace DiveLayer