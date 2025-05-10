bits 64

section .text

global vkCreateInstance
align 16
vkCreateInstance:
invlpg [rax]
db "vkCreateInstance", 0
ret

global vkDestroyInstance
align 16
vkDestroyInstance:
invlpg [rax]
db "vkDestroyInstance", 0
ret

global vkEnumeratePhysicalDevices
align 16
vkEnumeratePhysicalDevices:
invlpg [rax]
db "vkEnumeratePhysicalDevices", 0
ret

global vkGetPhysicalDeviceProperties
align 16
vkGetPhysicalDeviceProperties:
invlpg [rax]
db "vkGetPhysicalDeviceProperties", 0
ret

global vkGetPhysicalDeviceQueueFamilyProperties
align 16
vkGetPhysicalDeviceQueueFamilyProperties:
invlpg [rax]
db "vkGetPhysicalDeviceQueueFamilyProperties", 0
ret

global vkCreateDevice
align 16
vkCreateDevice:
invlpg [rax]
db "vkCreateDevice", 0
ret

global vkDeviceWaitIdle
align 16
vkDeviceWaitIdle:
invlpg [rax]
db "vkDeviceWaitIdle", 0
ret

global vkDestroyDevice
align 16
vkDestroyDevice:
invlpg [rax]
db "vkDestroyDevice", 0
ret

global vkGetDeviceQueue
align 16
vkGetDeviceQueue:
invlpg [rax]
db "vkGetDeviceQueue", 0
ret

global vkQueueWaitIdle
align 16
vkQueueWaitIdle:
invlpg [rax]
db "vkQueueWaitIdle", 0
ret

global vkCreateCommandPool
align 16
vkCreateCommandPool:
invlpg [rax]
db "vkCreateCommandPool", 0
ret

global vkResetCommandPool
align 16
vkResetCommandPool:
invlpg [rax]
db "vkResetCommandPool", 0
ret

global vkDestroyCommandPool
align 16
vkDestroyCommandPool:
invlpg [rax]
db "vkDestroyCommandPool", 0
ret

global vkAllocateCommandBuffers
align 16
vkAllocateCommandBuffers:
invlpg [rax]
db "vkAllocateCommandBuffers", 0
ret

global vkResetCommandBuffer
align 16
vkResetCommandBuffer:
invlpg [rax]
db "vkResetCommandBuffer", 0
ret

global vkFreeCommandBuffers
align 16
vkFreeCommandBuffers:
invlpg [rax]
db "vkFreeCommandBuffers", 0
ret

global vkBeginCommandBuffer
align 16
vkBeginCommandBuffer:
invlpg [rax]
db "vkBeginCommandBuffer", 0
ret

global vkEndCommandBuffer
align 16
vkEndCommandBuffer:
invlpg [rax]
db "vkEndCommandBuffer", 0
ret

global vkQueueSubmit
align 16
vkQueueSubmit:
invlpg [rax]
db "vkQueueSubmit", 0
ret

global vkCmdExecuteCommands
align 16
vkCmdExecuteCommands:
invlpg [rax]
db "vkCmdExecuteCommands", 0
ret

global vkCreateFence
align 16
vkCreateFence:
invlpg [rax]
db "vkCreateFence", 0
ret

global vkDestroyFence
align 16
vkDestroyFence:
invlpg [rax]
db "vkDestroyFence", 0
ret

global vkGetFenceStatus
align 16
vkGetFenceStatus:
invlpg [rax]
db "vkGetFenceStatus", 0
ret

global vkResetFences
align 16
vkResetFences:
invlpg [rax]
db "vkResetFences", 0
ret

global vkWaitForFences
align 16
vkWaitForFences:
invlpg [rax]
db "vkWaitForFences", 0
ret

global vkCreateSemaphore
align 16
vkCreateSemaphore:
invlpg [rax]
db "vkCreateSemaphore", 0
ret

global vkDestroySemaphore
align 16
vkDestroySemaphore:
invlpg [rax]
db "vkDestroySemaphore", 0
ret

global vkCreateEvent
align 16
vkCreateEvent:
invlpg [rax]
db "vkCreateEvent", 0
ret

global vkDestroyEvent
align 16
vkDestroyEvent:
invlpg [rax]
db "vkDestroyEvent", 0
ret

global vkGetEventStatus
align 16
vkGetEventStatus:
invlpg [rax]
db "vkGetEventStatus", 0
ret

global vkSetEvent
align 16
vkSetEvent:
invlpg [rax]
db "vkSetEvent", 0
ret

global vkResetEvent
align 16
vkResetEvent:
invlpg [rax]
db "vkResetEvent", 0
ret

global vkCmdSetEvent
align 16
vkCmdSetEvent:
invlpg [rax]
db "vkCmdSetEvent", 0
ret

global vkCmdResetEvent
align 16
vkCmdResetEvent:
invlpg [rax]
db "vkCmdResetEvent", 0
ret

global vkCmdWaitEvents
align 16
vkCmdWaitEvents:
invlpg [rax]
db "vkCmdWaitEvents", 0
ret

global vkCmdPipelineBarrier
align 16
vkCmdPipelineBarrier:
invlpg [rax]
db "vkCmdPipelineBarrier", 0
ret

global vkCreateRenderPass
align 16
vkCreateRenderPass:
invlpg [rax]
db "vkCreateRenderPass", 0
ret

global vkDestroyRenderPass
align 16
vkDestroyRenderPass:
invlpg [rax]
db "vkDestroyRenderPass", 0
ret

global vkCreateFramebuffer
align 16
vkCreateFramebuffer:
invlpg [rax]
db "vkCreateFramebuffer", 0
ret

global vkDestroyFramebuffer
align 16
vkDestroyFramebuffer:
invlpg [rax]
db "vkDestroyFramebuffer", 0
ret

global vkCmdBeginRenderPass
align 16
vkCmdBeginRenderPass:
invlpg [rax]
db "vkCmdBeginRenderPass", 0
ret

global vkGetRenderAreaGranularity
align 16
vkGetRenderAreaGranularity:
invlpg [rax]
db "vkGetRenderAreaGranularity", 0
ret

global vkCmdNextSubpass
align 16
vkCmdNextSubpass:
invlpg [rax]
db "vkCmdNextSubpass", 0
ret

global vkCmdEndRenderPass
align 16
vkCmdEndRenderPass:
invlpg [rax]
db "vkCmdEndRenderPass", 0
ret

global vkCreateShaderModule
align 16
vkCreateShaderModule:
invlpg [rax]
db "vkCreateShaderModule", 0
ret

global vkDestroyShaderModule
align 16
vkDestroyShaderModule:
invlpg [rax]
db "vkDestroyShaderModule", 0
ret

global vkCreateComputePipelines
align 16
vkCreateComputePipelines:
invlpg [rax]
db "vkCreateComputePipelines", 0
ret

global vkCreateGraphicsPipelines
align 16
vkCreateGraphicsPipelines:
invlpg [rax]
db "vkCreateGraphicsPipelines", 0
ret

global vkDestroyPipeline
align 16
vkDestroyPipeline:
invlpg [rax]
db "vkDestroyPipeline", 0
ret

global vkCreatePipelineCache
align 16
vkCreatePipelineCache:
invlpg [rax]
db "vkCreatePipelineCache", 0
ret

global vkMergePipelineCaches
align 16
vkMergePipelineCaches:
invlpg [rax]
db "vkMergePipelineCaches", 0
ret

global vkGetPipelineCacheData
align 16
vkGetPipelineCacheData:
invlpg [rax]
db "vkGetPipelineCacheData", 0
ret

global vkDestroyPipelineCache
align 16
vkDestroyPipelineCache:
invlpg [rax]
db "vkDestroyPipelineCache", 0
ret

global vkCmdBindPipeline
align 16
vkCmdBindPipeline:
invlpg [rax]
db "vkCmdBindPipeline", 0
ret

global vkGetPhysicalDeviceMemoryProperties
align 16
vkGetPhysicalDeviceMemoryProperties:
invlpg [rax]
db "vkGetPhysicalDeviceMemoryProperties", 0
ret

global vkAllocateMemory
align 16
vkAllocateMemory:
invlpg [rax]
db "vkAllocateMemory", 0
ret

global vkFreeMemory
align 16
vkFreeMemory:
invlpg [rax]
db "vkFreeMemory", 0
ret

global vkMapMemory
align 16
vkMapMemory:
invlpg [rax]
db "vkMapMemory", 0
ret

global vkFlushMappedMemoryRanges
align 16
vkFlushMappedMemoryRanges:
invlpg [rax]
db "vkFlushMappedMemoryRanges", 0
ret

global vkInvalidateMappedMemoryRanges
align 16
vkInvalidateMappedMemoryRanges:
invlpg [rax]
db "vkInvalidateMappedMemoryRanges", 0
ret

global vkUnmapMemory
align 16
vkUnmapMemory:
invlpg [rax]
db "vkUnmapMemory", 0
ret

global vkGetDeviceMemoryCommitment
align 16
vkGetDeviceMemoryCommitment:
invlpg [rax]
db "vkGetDeviceMemoryCommitment", 0
ret

global vkCreateBuffer
align 16
vkCreateBuffer:
invlpg [rax]
db "vkCreateBuffer", 0
ret

global vkDestroyBuffer
align 16
vkDestroyBuffer:
invlpg [rax]
db "vkDestroyBuffer", 0
ret

global vkCreateBufferView
align 16
vkCreateBufferView:
invlpg [rax]
db "vkCreateBufferView", 0
ret

global vkDestroyBufferView
align 16
vkDestroyBufferView:
invlpg [rax]
db "vkDestroyBufferView", 0
ret

global vkCreateImage
align 16
vkCreateImage:
invlpg [rax]
db "vkCreateImage", 0
ret

global vkGetImageSubresourceLayout
align 16
vkGetImageSubresourceLayout:
invlpg [rax]
db "vkGetImageSubresourceLayout", 0
ret

global vkDestroyImage
align 16
vkDestroyImage:
invlpg [rax]
db "vkDestroyImage", 0
ret

global vkCreateImageView
align 16
vkCreateImageView:
invlpg [rax]
db "vkCreateImageView", 0
ret

global vkDestroyImageView
align 16
vkDestroyImageView:
invlpg [rax]
db "vkDestroyImageView", 0
ret

global vkGetBufferMemoryRequirements
align 16
vkGetBufferMemoryRequirements:
invlpg [rax]
db "vkGetBufferMemoryRequirements", 0
ret

global vkGetImageMemoryRequirements
align 16
vkGetImageMemoryRequirements:
invlpg [rax]
db "vkGetImageMemoryRequirements", 0
ret

global vkBindBufferMemory
align 16
vkBindBufferMemory:
invlpg [rax]
db "vkBindBufferMemory", 0
ret

global vkBindImageMemory
align 16
vkBindImageMemory:
invlpg [rax]
db "vkBindImageMemory", 0
ret

global vkCreateSampler
align 16
vkCreateSampler:
invlpg [rax]
db "vkCreateSampler", 0
ret

global vkDestroySampler
align 16
vkDestroySampler:
invlpg [rax]
db "vkDestroySampler", 0
ret

global vkCreateDescriptorSetLayout
align 16
vkCreateDescriptorSetLayout:
invlpg [rax]
db "vkCreateDescriptorSetLayout", 0
ret

global vkDestroyDescriptorSetLayout
align 16
vkDestroyDescriptorSetLayout:
invlpg [rax]
db "vkDestroyDescriptorSetLayout", 0
ret

global vkCreatePipelineLayout
align 16
vkCreatePipelineLayout:
invlpg [rax]
db "vkCreatePipelineLayout", 0
ret

global vkDestroyPipelineLayout
align 16
vkDestroyPipelineLayout:
invlpg [rax]
db "vkDestroyPipelineLayout", 0
ret

global vkCreateDescriptorPool
align 16
vkCreateDescriptorPool:
invlpg [rax]
db "vkCreateDescriptorPool", 0
ret

global vkDestroyDescriptorPool
align 16
vkDestroyDescriptorPool:
invlpg [rax]
db "vkDestroyDescriptorPool", 0
ret

global vkAllocateDescriptorSets
align 16
vkAllocateDescriptorSets:
invlpg [rax]
db "vkAllocateDescriptorSets", 0
ret

global vkFreeDescriptorSets
align 16
vkFreeDescriptorSets:
invlpg [rax]
db "vkFreeDescriptorSets", 0
ret

global vkResetDescriptorPool
align 16
vkResetDescriptorPool:
invlpg [rax]
db "vkResetDescriptorPool", 0
ret

global vkUpdateDescriptorSets
align 16
vkUpdateDescriptorSets:
invlpg [rax]
db "vkUpdateDescriptorSets", 0
ret

global vkCmdBindDescriptorSets
align 16
vkCmdBindDescriptorSets:
invlpg [rax]
db "vkCmdBindDescriptorSets", 0
ret

global vkCmdPushConstants
align 16
vkCmdPushConstants:
invlpg [rax]
db "vkCmdPushConstants", 0
ret

global vkCreateQueryPool
align 16
vkCreateQueryPool:
invlpg [rax]
db "vkCreateQueryPool", 0
ret

global vkDestroyQueryPool
align 16
vkDestroyQueryPool:
invlpg [rax]
db "vkDestroyQueryPool", 0
ret

global vkCmdResetQueryPool
align 16
vkCmdResetQueryPool:
invlpg [rax]
db "vkCmdResetQueryPool", 0
ret

global vkCmdBeginQuery
align 16
vkCmdBeginQuery:
invlpg [rax]
db "vkCmdBeginQuery", 0
ret

global vkCmdEndQuery
align 16
vkCmdEndQuery:
invlpg [rax]
db "vkCmdEndQuery", 0
ret

global vkGetQueryPoolResults
align 16
vkGetQueryPoolResults:
invlpg [rax]
db "vkGetQueryPoolResults", 0
ret

global vkCmdCopyQueryPoolResults
align 16
vkCmdCopyQueryPoolResults:
invlpg [rax]
db "vkCmdCopyQueryPoolResults", 0
ret

global vkCmdWriteTimestamp
align 16
vkCmdWriteTimestamp:
invlpg [rax]
db "vkCmdWriteTimestamp", 0
ret

global vkCmdClearColorImage
align 16
vkCmdClearColorImage:
invlpg [rax]
db "vkCmdClearColorImage", 0
ret

global vkCmdClearDepthStencilImage
align 16
vkCmdClearDepthStencilImage:
invlpg [rax]
db "vkCmdClearDepthStencilImage", 0
ret

global vkCmdClearAttachments
align 16
vkCmdClearAttachments:
invlpg [rax]
db "vkCmdClearAttachments", 0
ret

global vkCmdFillBuffer
align 16
vkCmdFillBuffer:
invlpg [rax]
db "vkCmdFillBuffer", 0
ret

global vkCmdUpdateBuffer
align 16
vkCmdUpdateBuffer:
invlpg [rax]
db "vkCmdUpdateBuffer", 0
ret

global vkCmdCopyBuffer
align 16
vkCmdCopyBuffer:
invlpg [rax]
db "vkCmdCopyBuffer", 0
ret

global vkCmdCopyImage
align 16
vkCmdCopyImage:
invlpg [rax]
db "vkCmdCopyImage", 0
ret

global vkCmdCopyBufferToImage
align 16
vkCmdCopyBufferToImage:
invlpg [rax]
db "vkCmdCopyBufferToImage", 0
ret

global vkCmdCopyImageToBuffer
align 16
vkCmdCopyImageToBuffer:
invlpg [rax]
db "vkCmdCopyImageToBuffer", 0
ret

global vkCmdBlitImage
align 16
vkCmdBlitImage:
invlpg [rax]
db "vkCmdBlitImage", 0
ret

global vkCmdResolveImage
align 16
vkCmdResolveImage:
invlpg [rax]
db "vkCmdResolveImage", 0
ret

global vkCmdBindIndexBuffer
align 16
vkCmdBindIndexBuffer:
invlpg [rax]
db "vkCmdBindIndexBuffer", 0
ret

global vkCmdDraw
align 16
vkCmdDraw:
invlpg [rax]
db "vkCmdDraw", 0
ret

global vkCmdDrawIndexed
align 16
vkCmdDrawIndexed:
invlpg [rax]
db "vkCmdDrawIndexed", 0
ret

global vkCmdDrawIndirect
align 16
vkCmdDrawIndirect:
invlpg [rax]
db "vkCmdDrawIndirect", 0
ret

global vkCmdDrawIndexedIndirect
align 16
vkCmdDrawIndexedIndirect:
invlpg [rax]
db "vkCmdDrawIndexedIndirect", 0
ret

global vkCmdSetScissor
align 16
vkCmdSetScissor:
invlpg [rax]
db "vkCmdSetScissor", 0
ret

global vkCmdSetDepthBounds
align 16
vkCmdSetDepthBounds:
invlpg [rax]
db "vkCmdSetDepthBounds", 0
ret

global vkCmdSetStencilCompareMask
align 16
vkCmdSetStencilCompareMask:
invlpg [rax]
db "vkCmdSetStencilCompareMask", 0
ret

global vkCmdSetStencilWriteMask
align 16
vkCmdSetStencilWriteMask:
invlpg [rax]
db "vkCmdSetStencilWriteMask", 0
ret

global vkCmdSetStencilReference
align 16
vkCmdSetStencilReference:
invlpg [rax]
db "vkCmdSetStencilReference", 0
ret

global vkCmdBindVertexBuffers
align 16
vkCmdBindVertexBuffers:
invlpg [rax]
db "vkCmdBindVertexBuffers", 0
ret

global vkCmdSetViewport
align 16
vkCmdSetViewport:
invlpg [rax]
db "vkCmdSetViewport", 0
ret

global vkCmdSetLineWidth
align 16
vkCmdSetLineWidth:
invlpg [rax]
db "vkCmdSetLineWidth", 0
ret

global vkCmdSetDepthBias
align 16
vkCmdSetDepthBias:
invlpg [rax]
db "vkCmdSetDepthBias", 0
ret

global vkCmdSetBlendConstants
align 16
vkCmdSetBlendConstants:
invlpg [rax]
db "vkCmdSetBlendConstants", 0
ret

global vkGetPhysicalDeviceSparseImageFormatProperties
align 16
vkGetPhysicalDeviceSparseImageFormatProperties:
invlpg [rax]
db "vkGetPhysicalDeviceSparseImageFormatProperties", 0
ret

global vkGetImageSparseMemoryRequirements
align 16
vkGetImageSparseMemoryRequirements:
invlpg [rax]
db "vkGetImageSparseMemoryRequirements", 0
ret

global vkQueueBindSparse
align 16
vkQueueBindSparse:
invlpg [rax]
db "vkQueueBindSparse", 0
ret

global vkCmdDispatch
align 16
vkCmdDispatch:
invlpg [rax]
db "vkCmdDispatch", 0
ret

global vkCmdDispatchIndirect
align 16
vkCmdDispatchIndirect:
invlpg [rax]
db "vkCmdDispatchIndirect", 0
ret

global vkCreateWaylandSurfaceKHR
align 16
vkCreateWaylandSurfaceKHR:
invlpg [rax]
db "vkCreateWaylandSurfaceKHR", 0
ret

global vkCreateSwapchainKHR
align 16
vkCreateSwapchainKHR:
invlpg [rax]
db "vkCreateSwapchainKHR", 0
ret

global vkDestroySwapchainKHR
align 16
vkDestroySwapchainKHR:
invlpg [rax]
db "vkDestroySwapchainKHR", 0
ret

global vkCreateSharedSwapchainsKHR
align 16
vkCreateSharedSwapchainsKHR:
invlpg [rax]
db "vkCreateSharedSwapchainsKHR", 0
ret

global vkGetSwapchainImagesKHR
align 16
vkGetSwapchainImagesKHR:
invlpg [rax]
db "vkGetSwapchainImagesKHR", 0
ret

global vkAcquireNextImageKHR
align 16
vkAcquireNextImageKHR:
invlpg [rax]
db "vkAcquireNextImageKHR", 0
ret

global vkQueuePresentKHR
align 16
vkQueuePresentKHR:
invlpg [rax]
db "vkQueuePresentKHR", 0
ret

global vkEnumerateInstanceLayerProperties
align 16
vkEnumerateInstanceLayerProperties:
invlpg [rax]
db "vkEnumerateInstanceLayerProperties", 0
ret

global vkEnumerateDeviceLayerProperties
align 16
vkEnumerateDeviceLayerProperties:
invlpg [rax]
db "vkEnumerateDeviceLayerProperties", 0
ret

global vkEnumerateInstanceExtensionProperties
align 16
vkEnumerateInstanceExtensionProperties:
invlpg [rax]
db "vkEnumerateInstanceExtensionProperties", 0
ret

global vkEnumerateDeviceExtensionProperties
align 16
vkEnumerateDeviceExtensionProperties:
invlpg [rax]
db "vkEnumerateDeviceExtensionProperties", 0
ret

global vkGetPhysicalDeviceFeatures
align 16
vkGetPhysicalDeviceFeatures:
invlpg [rax]
db "vkGetPhysicalDeviceFeatures", 0
ret

global vkGetPhysicalDeviceFormatProperties
align 16
vkGetPhysicalDeviceFormatProperties:
invlpg [rax]
db "vkGetPhysicalDeviceFormatProperties", 0
ret

global vkGetPhysicalDeviceImageFormatProperties
align 16
vkGetPhysicalDeviceImageFormatProperties:
invlpg [rax]
db "vkGetPhysicalDeviceImageFormatProperties", 0
ret
