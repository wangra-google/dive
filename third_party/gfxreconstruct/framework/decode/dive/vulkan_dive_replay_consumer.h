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

#ifndef GFXRECON_DECODE_VULKAN_DIVE_CONSUMER_H
#define GFXRECON_DECODE_VULKAN_DIVE_CONSUMER_H

#include "generated/generated_vulkan_replay_consumer.h"
#include <set>

GFXRECON_BEGIN_NAMESPACE(gfxrecon)
GFXRECON_BEGIN_NAMESPACE(decode)

#include <vector>

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
    void AddRenderPassTime(int pass_index, double time);

    uint32_t  GetPassCount() const;
    PassStats GetPassStatistics(int pass_index) const;

    void PrintStats(const PassStats& stats);

  private:
    // Helper function to calculate the average for a given pass index
    double CalculateAverage(int pass_index) const;
    // Helper function to calculate the standard deviation for a given pass index
    double CalculateStdDev(int pass_index, double average) const;

    // Store frame data as a vector of vectors of doubles
    std::vector<std::vector<double>> frame_data_;
};


class VulkanDiveReplayConsumer : public VulkanReplayConsumer
{
  public:
    VulkanDiveReplayConsumer(std::shared_ptr<application::Application> application, const VulkanReplayOptions& options);

    ~VulkanDiveReplayConsumer() override;

    virtual void Process_vkCreateDevice(const ApiCallInfo&                                   call_info,
                                        VkResult                                             returnValue,
                                        format::HandleId                                     physicalDevice,
                                        StructPointerDecoder<Decoded_VkDeviceCreateInfo>*    pCreateInfo,
                                        StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator,
                                        HandlePointerDecoder<VkDevice>*                      pDevice) override;

    virtual void Process_vkDestroyDevice(const ApiCallInfo&                                   call_info,
                                         format::HandleId                                     device,
                                         StructPointerDecoder<Decoded_VkAllocationCallbacks>* pAllocator) override;

    virtual void Process_vkCmdBeginRenderPass(const ApiCallInfo&                                   call_info,
                                              format::HandleId                                     commandBuffer,
                                              StructPointerDecoder<Decoded_VkRenderPassBeginInfo>* pRenderPassBegin,
                                              VkSubpassContents                                    contents) override;
    virtual void Process_vkCmdEndRenderPass(const ApiCallInfo& call_info, format::HandleId commandBuffer) override;

    virtual void Process_vkAcquireNextImageKHR(const ApiCallInfo&        call_info,
                                               VkResult                  returnValue,
                                               format::HandleId          device,
                                               format::HandleId          swapchain,
                                               uint64_t                  timeout,
                                               format::HandleId          semaphore,
                                               format::HandleId          fence,
                                               PointerDecoder<uint32_t>* pImageIndex) override;
    virtual void Process_vkQueuePresentKHR(const ApiCallInfo&                              call_info,
                                           VkResult                                        returnValue,
                                           format::HandleId                                queue,
                                           StructPointerDecoder<Decoded_VkPresentInfoKHR>* pPresentInfo) override;
    void         Process_vkQueueSubmit(const ApiCallInfo&                          call_info,
                                       VkResult                                    returnValue,
                                       format::HandleId                            queue,
                                       uint32_t                                    submitCount,
                                       StructPointerDecoder<Decoded_VkSubmitInfo>* pSubmits,
                                       format::HandleId                            fence) override;

    virtual void
    Process_vkBeginCommandBuffer(const ApiCallInfo&                                      call_info,
                                 VkResult                                                returnValue,
                                 format::HandleId                                        commandBuffer,
                                 StructPointerDecoder<Decoded_VkCommandBufferBeginInfo>* pBeginInfo) override;

    virtual void Process_vkEndCommandBuffer(const ApiCallInfo& call_info,
                                            VkResult           returnValue,
                                            format::HandleId   commandBuffer) override;

    virtual void Process_vkGetDeviceQueue2(const ApiCallInfo&                                call_info,
                                           format::HandleId                                  device,
                                           StructPointerDecoder<Decoded_VkDeviceQueueInfo2>* pQueueInfo,
                                           HandlePointerDecoder<VkQueue>*                    pQueue) override;

    virtual void Process_vkGetDeviceQueue(const ApiCallInfo&             call_info,
                                          format::HandleId               device,
                                          uint32_t                       queueFamilyIndex,
                                          uint32_t                       queueIndex,
                                          HandlePointerDecoder<VkQueue>* pQueue) override;

  private:
    // Helper function to destroy the query pool
    void DestroyQueryPool();

    VkQueryPool            query_pool_;
    VkDevice               device_;
    std::set<VkQueue>      queues_;
    float                  timestamp_period_;
    uint32_t               timestamp_counter_;
    bool                   first_frame_done;
    VkAllocationCallbacks* allocator_;
    FrameMetrics           metrics_;
};

GFXRECON_END_NAMESPACE(decode)
GFXRECON_END_NAMESPACE(gfxrecon)

#endif // GFXRECON_DECODE_VULKAN_REPLAY_CONSUMER_BASE_H
