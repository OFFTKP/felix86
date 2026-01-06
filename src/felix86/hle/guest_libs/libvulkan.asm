bits 64

; These are guest function pointers that we wanna call from host code at will
section .data

align 16
libname:
db "libvulkan.so", 0

guest_func_names:
XGetVisualInfo_name:
db "XGetVisualInfo", 0

XSync_name:
db "XSync", 0

section .text

extern XGetVisualInfo
extern XSync

global __felix86_constructor:function
align 16
__felix86_constructor:
invlpg [rbx]
ret
dd 0x12345678 ; invlpg + ret are 4 bytes, four more here to align to pointer
dq libname
; Here follows the null terminated list of {const char*, void*} (names, functions)
dq XGetVisualInfo_name
dq XGetVisualInfo
dq XSync_name
dq XSync
dq 0
dq 0


global vkGetInstanceProcAddr:function
align 16
vkGetInstanceProcAddr:
invlpg [rax]
db "vkGetInstanceProcAddr", 0
ret

global vkGetDeviceProcAddr:function
align 16
vkGetDeviceProcAddr:
invlpg [rax]
db "vkGetDeviceProcAddr", 0
ret

global vkCreateInstance:function
align 16
vkCreateInstance:
invlpg [rax]
db "vkCreateInstance", 0
ret

global vkDestroyInstance:function
align 16
vkDestroyInstance:
invlpg [rax]
db "vkDestroyInstance", 0
ret

global vkEnumeratePhysicalDevices:function
align 16
vkEnumeratePhysicalDevices:
invlpg [rax]
db "vkEnumeratePhysicalDevices", 0
ret

global vkGetPhysicalDeviceProperties:function
align 16
vkGetPhysicalDeviceProperties:
invlpg [rax]
db "vkGetPhysicalDeviceProperties", 0
ret

global vkGetPhysicalDeviceQueueFamilyProperties:function
align 16
vkGetPhysicalDeviceQueueFamilyProperties:
invlpg [rax]
db "vkGetPhysicalDeviceQueueFamilyProperties", 0
ret

global vkCreateDevice:function
align 16
vkCreateDevice:
invlpg [rax]
db "vkCreateDevice", 0
ret

global vkDeviceWaitIdle:function
align 16
vkDeviceWaitIdle:
invlpg [rax]
db "vkDeviceWaitIdle", 0
ret

global vkDestroyDevice:function
align 16
vkDestroyDevice:
invlpg [rax]
db "vkDestroyDevice", 0
ret

global vkGetDeviceQueue:function
align 16
vkGetDeviceQueue:
invlpg [rax]
db "vkGetDeviceQueue", 0
ret

global vkQueueWaitIdle:function
align 16
vkQueueWaitIdle:
invlpg [rax]
db "vkQueueWaitIdle", 0
ret

global vkCreateCommandPool:function
align 16
vkCreateCommandPool:
invlpg [rax]
db "vkCreateCommandPool", 0
ret

global vkResetCommandPool:function
align 16
vkResetCommandPool:
invlpg [rax]
db "vkResetCommandPool", 0
ret

global vkDestroyCommandPool:function
align 16
vkDestroyCommandPool:
invlpg [rax]
db "vkDestroyCommandPool", 0
ret

global vkAllocateCommandBuffers:function
align 16
vkAllocateCommandBuffers:
invlpg [rax]
db "vkAllocateCommandBuffers", 0
ret

global vkResetCommandBuffer:function
align 16
vkResetCommandBuffer:
invlpg [rax]
db "vkResetCommandBuffer", 0
ret

global vkFreeCommandBuffers:function
align 16
vkFreeCommandBuffers:
invlpg [rax]
db "vkFreeCommandBuffers", 0
ret

global vkBeginCommandBuffer:function
align 16
vkBeginCommandBuffer:
invlpg [rax]
db "vkBeginCommandBuffer", 0
ret

global vkEndCommandBuffer:function
align 16
vkEndCommandBuffer:
invlpg [rax]
db "vkEndCommandBuffer", 0
ret

global vkQueueSubmit:function
align 16
vkQueueSubmit:
invlpg [rax]
db "vkQueueSubmit", 0
ret

global vkCmdExecuteCommands:function
align 16
vkCmdExecuteCommands:
invlpg [rax]
db "vkCmdExecuteCommands", 0
ret

global vkCreateFence:function
align 16
vkCreateFence:
invlpg [rax]
db "vkCreateFence", 0
ret

global vkDestroyFence:function
align 16
vkDestroyFence:
invlpg [rax]
db "vkDestroyFence", 0
ret

global vkGetFenceStatus:function
align 16
vkGetFenceStatus:
invlpg [rax]
db "vkGetFenceStatus", 0
ret

global vkResetFences:function
align 16
vkResetFences:
invlpg [rax]
db "vkResetFences", 0
ret

global vkWaitForFences:function
align 16
vkWaitForFences:
invlpg [rax]
db "vkWaitForFences", 0
ret

global vkCreateSemaphore:function
align 16
vkCreateSemaphore:
invlpg [rax]
db "vkCreateSemaphore", 0
ret

global vkDestroySemaphore:function
align 16
vkDestroySemaphore:
invlpg [rax]
db "vkDestroySemaphore", 0
ret

global vkCreateEvent:function
align 16
vkCreateEvent:
invlpg [rax]
db "vkCreateEvent", 0
ret

global vkDestroyEvent:function
align 16
vkDestroyEvent:
invlpg [rax]
db "vkDestroyEvent", 0
ret

global vkGetEventStatus:function
align 16
vkGetEventStatus:
invlpg [rax]
db "vkGetEventStatus", 0
ret

global vkSetEvent:function
align 16
vkSetEvent:
invlpg [rax]
db "vkSetEvent", 0
ret

global vkResetEvent:function
align 16
vkResetEvent:
invlpg [rax]
db "vkResetEvent", 0
ret

global vkCmdSetEvent:function
align 16
vkCmdSetEvent:
invlpg [rax]
db "vkCmdSetEvent", 0
ret

global vkCmdResetEvent:function
align 16
vkCmdResetEvent:
invlpg [rax]
db "vkCmdResetEvent", 0
ret

global vkCmdWaitEvents:function
align 16
vkCmdWaitEvents:
invlpg [rax]
db "vkCmdWaitEvents", 0
ret

global vkCmdPipelineBarrier:function
align 16
vkCmdPipelineBarrier:
invlpg [rax]
db "vkCmdPipelineBarrier", 0
ret

global vkCreateRenderPass:function
align 16
vkCreateRenderPass:
invlpg [rax]
db "vkCreateRenderPass", 0
ret

global vkDestroyRenderPass:function
align 16
vkDestroyRenderPass:
invlpg [rax]
db "vkDestroyRenderPass", 0
ret

global vkCreateFramebuffer:function
align 16
vkCreateFramebuffer:
invlpg [rax]
db "vkCreateFramebuffer", 0
ret

global vkDestroyFramebuffer:function
align 16
vkDestroyFramebuffer:
invlpg [rax]
db "vkDestroyFramebuffer", 0
ret

global vkCmdBeginRenderPass:function
align 16
vkCmdBeginRenderPass:
invlpg [rax]
db "vkCmdBeginRenderPass", 0
ret

global vkGetRenderAreaGranularity:function
align 16
vkGetRenderAreaGranularity:
invlpg [rax]
db "vkGetRenderAreaGranularity", 0
ret

global vkCmdNextSubpass:function
align 16
vkCmdNextSubpass:
invlpg [rax]
db "vkCmdNextSubpass", 0
ret

global vkCmdEndRenderPass:function
align 16
vkCmdEndRenderPass:
invlpg [rax]
db "vkCmdEndRenderPass", 0
ret

global vkCreateShaderModule:function
align 16
vkCreateShaderModule:
invlpg [rax]
db "vkCreateShaderModule", 0
ret

global vkDestroyShaderModule:function
align 16
vkDestroyShaderModule:
invlpg [rax]
db "vkDestroyShaderModule", 0
ret

global vkCreateComputePipelines:function
align 16
vkCreateComputePipelines:
invlpg [rax]
db "vkCreateComputePipelines", 0
ret

global vkCreateGraphicsPipelines:function
align 16
vkCreateGraphicsPipelines:
invlpg [rax]
db "vkCreateGraphicsPipelines", 0
ret

global vkDestroyPipeline:function
align 16
vkDestroyPipeline:
invlpg [rax]
db "vkDestroyPipeline", 0
ret

global vkCreatePipelineCache:function
align 16
vkCreatePipelineCache:
invlpg [rax]
db "vkCreatePipelineCache", 0
ret

global vkMergePipelineCaches:function
align 16
vkMergePipelineCaches:
invlpg [rax]
db "vkMergePipelineCaches", 0
ret

global vkGetPipelineCacheData:function
align 16
vkGetPipelineCacheData:
invlpg [rax]
db "vkGetPipelineCacheData", 0
ret

global vkDestroyPipelineCache:function
align 16
vkDestroyPipelineCache:
invlpg [rax]
db "vkDestroyPipelineCache", 0
ret

global vkCmdBindPipeline:function
align 16
vkCmdBindPipeline:
invlpg [rax]
db "vkCmdBindPipeline", 0
ret

global vkGetPhysicalDeviceMemoryProperties:function
align 16
vkGetPhysicalDeviceMemoryProperties:
invlpg [rax]
db "vkGetPhysicalDeviceMemoryProperties", 0
ret

global vkAllocateMemory:function
align 16
vkAllocateMemory:
invlpg [rax]
db "vkAllocateMemory", 0
ret

global vkFreeMemory:function
align 16
vkFreeMemory:
invlpg [rax]
db "vkFreeMemory", 0
ret

global vkMapMemory:function
align 16
vkMapMemory:
invlpg [rax]
db "vkMapMemory", 0
ret

global vkFlushMappedMemoryRanges:function
align 16
vkFlushMappedMemoryRanges:
invlpg [rax]
db "vkFlushMappedMemoryRanges", 0
ret

global vkInvalidateMappedMemoryRanges:function
align 16
vkInvalidateMappedMemoryRanges:
invlpg [rax]
db "vkInvalidateMappedMemoryRanges", 0
ret

global vkUnmapMemory:function
align 16
vkUnmapMemory:
invlpg [rax]
db "vkUnmapMemory", 0
ret

global vkGetDeviceMemoryCommitment:function
align 16
vkGetDeviceMemoryCommitment:
invlpg [rax]
db "vkGetDeviceMemoryCommitment", 0
ret

global vkCreateBuffer:function
align 16
vkCreateBuffer:
invlpg [rax]
db "vkCreateBuffer", 0
ret

global vkDestroyBuffer:function
align 16
vkDestroyBuffer:
invlpg [rax]
db "vkDestroyBuffer", 0
ret

global vkCreateBufferView:function
align 16
vkCreateBufferView:
invlpg [rax]
db "vkCreateBufferView", 0
ret

global vkDestroyBufferView:function
align 16
vkDestroyBufferView:
invlpg [rax]
db "vkDestroyBufferView", 0
ret

global vkCreateImage:function
align 16
vkCreateImage:
invlpg [rax]
db "vkCreateImage", 0
ret

global vkGetImageSubresourceLayout:function
align 16
vkGetImageSubresourceLayout:
invlpg [rax]
db "vkGetImageSubresourceLayout", 0
ret

global vkDestroyImage:function
align 16
vkDestroyImage:
invlpg [rax]
db "vkDestroyImage", 0
ret

global vkCreateImageView:function
align 16
vkCreateImageView:
invlpg [rax]
db "vkCreateImageView", 0
ret

global vkDestroyImageView:function
align 16
vkDestroyImageView:
invlpg [rax]
db "vkDestroyImageView", 0
ret

global vkGetBufferMemoryRequirements:function
align 16
vkGetBufferMemoryRequirements:
invlpg [rax]
db "vkGetBufferMemoryRequirements", 0
ret

global vkGetImageMemoryRequirements:function
align 16
vkGetImageMemoryRequirements:
invlpg [rax]
db "vkGetImageMemoryRequirements", 0
ret

global vkBindBufferMemory:function
align 16
vkBindBufferMemory:
invlpg [rax]
db "vkBindBufferMemory", 0
ret

global vkBindImageMemory:function
align 16
vkBindImageMemory:
invlpg [rax]
db "vkBindImageMemory", 0
ret

global vkCreateSampler:function
align 16
vkCreateSampler:
invlpg [rax]
db "vkCreateSampler", 0
ret

global vkDestroySampler:function
align 16
vkDestroySampler:
invlpg [rax]
db "vkDestroySampler", 0
ret

global vkCreateDescriptorSetLayout:function
align 16
vkCreateDescriptorSetLayout:
invlpg [rax]
db "vkCreateDescriptorSetLayout", 0
ret

global vkDestroyDescriptorSetLayout:function
align 16
vkDestroyDescriptorSetLayout:
invlpg [rax]
db "vkDestroyDescriptorSetLayout", 0
ret

global vkCreatePipelineLayout:function
align 16
vkCreatePipelineLayout:
invlpg [rax]
db "vkCreatePipelineLayout", 0
ret

global vkDestroyPipelineLayout:function
align 16
vkDestroyPipelineLayout:
invlpg [rax]
db "vkDestroyPipelineLayout", 0
ret

global vkCreateDescriptorPool:function
align 16
vkCreateDescriptorPool:
invlpg [rax]
db "vkCreateDescriptorPool", 0
ret

global vkDestroyDescriptorPool:function
align 16
vkDestroyDescriptorPool:
invlpg [rax]
db "vkDestroyDescriptorPool", 0
ret

global vkAllocateDescriptorSets:function
align 16
vkAllocateDescriptorSets:
invlpg [rax]
db "vkAllocateDescriptorSets", 0
ret

global vkFreeDescriptorSets:function
align 16
vkFreeDescriptorSets:
invlpg [rax]
db "vkFreeDescriptorSets", 0
ret

global vkResetDescriptorPool:function
align 16
vkResetDescriptorPool:
invlpg [rax]
db "vkResetDescriptorPool", 0
ret

global vkUpdateDescriptorSets:function
align 16
vkUpdateDescriptorSets:
invlpg [rax]
db "vkUpdateDescriptorSets", 0
ret

global vkCmdBindDescriptorSets:function
align 16
vkCmdBindDescriptorSets:
invlpg [rax]
db "vkCmdBindDescriptorSets", 0
ret

global vkCmdPushConstants:function
align 16
vkCmdPushConstants:
invlpg [rax]
db "vkCmdPushConstants", 0
ret

global vkCreateQueryPool:function
align 16
vkCreateQueryPool:
invlpg [rax]
db "vkCreateQueryPool", 0
ret

global vkDestroyQueryPool:function
align 16
vkDestroyQueryPool:
invlpg [rax]
db "vkDestroyQueryPool", 0
ret

global vkCmdResetQueryPool:function
align 16
vkCmdResetQueryPool:
invlpg [rax]
db "vkCmdResetQueryPool", 0
ret

global vkCmdBeginQuery:function
align 16
vkCmdBeginQuery:
invlpg [rax]
db "vkCmdBeginQuery", 0
ret

global vkCmdEndQuery:function
align 16
vkCmdEndQuery:
invlpg [rax]
db "vkCmdEndQuery", 0
ret

global vkGetQueryPoolResults:function
align 16
vkGetQueryPoolResults:
invlpg [rax]
db "vkGetQueryPoolResults", 0
ret

global vkCmdCopyQueryPoolResults:function
align 16
vkCmdCopyQueryPoolResults:
invlpg [rax]
db "vkCmdCopyQueryPoolResults", 0
ret

global vkCmdWriteTimestamp:function
align 16
vkCmdWriteTimestamp:
invlpg [rax]
db "vkCmdWriteTimestamp", 0
ret

global vkCmdClearColorImage:function
align 16
vkCmdClearColorImage:
invlpg [rax]
db "vkCmdClearColorImage", 0
ret

global vkCmdClearDepthStencilImage:function
align 16
vkCmdClearDepthStencilImage:
invlpg [rax]
db "vkCmdClearDepthStencilImage", 0
ret

global vkCmdClearAttachments:function
align 16
vkCmdClearAttachments:
invlpg [rax]
db "vkCmdClearAttachments", 0
ret

global vkCmdFillBuffer:function
align 16
vkCmdFillBuffer:
invlpg [rax]
db "vkCmdFillBuffer", 0
ret

global vkCmdUpdateBuffer:function
align 16
vkCmdUpdateBuffer:
invlpg [rax]
db "vkCmdUpdateBuffer", 0
ret

global vkCmdCopyBuffer:function
align 16
vkCmdCopyBuffer:
invlpg [rax]
db "vkCmdCopyBuffer", 0
ret

global vkCmdCopyImage:function
align 16
vkCmdCopyImage:
invlpg [rax]
db "vkCmdCopyImage", 0
ret

global vkCmdCopyBufferToImage:function
align 16
vkCmdCopyBufferToImage:
invlpg [rax]
db "vkCmdCopyBufferToImage", 0
ret

global vkCmdCopyImageToBuffer:function
align 16
vkCmdCopyImageToBuffer:
invlpg [rax]
db "vkCmdCopyImageToBuffer", 0
ret

global vkCmdBlitImage:function
align 16
vkCmdBlitImage:
invlpg [rax]
db "vkCmdBlitImage", 0
ret

global vkCmdResolveImage:function
align 16
vkCmdResolveImage:
invlpg [rax]
db "vkCmdResolveImage", 0
ret

global vkCmdBindIndexBuffer:function
align 16
vkCmdBindIndexBuffer:
invlpg [rax]
db "vkCmdBindIndexBuffer", 0
ret

global vkCmdDraw:function
align 16
vkCmdDraw:
invlpg [rax]
db "vkCmdDraw", 0
ret

global vkCmdDrawIndexed:function
align 16
vkCmdDrawIndexed:
invlpg [rax]
db "vkCmdDrawIndexed", 0
ret

global vkCmdDrawIndirect:function
align 16
vkCmdDrawIndirect:
invlpg [rax]
db "vkCmdDrawIndirect", 0
ret

global vkCmdDrawIndexedIndirect:function
align 16
vkCmdDrawIndexedIndirect:
invlpg [rax]
db "vkCmdDrawIndexedIndirect", 0
ret

global vkCmdSetScissor:function
align 16
vkCmdSetScissor:
invlpg [rax]
db "vkCmdSetScissor", 0
ret

global vkCmdSetDepthBounds:function
align 16
vkCmdSetDepthBounds:
invlpg [rax]
db "vkCmdSetDepthBounds", 0
ret

global vkCmdSetStencilCompareMask:function
align 16
vkCmdSetStencilCompareMask:
invlpg [rax]
db "vkCmdSetStencilCompareMask", 0
ret

global vkCmdSetStencilWriteMask:function
align 16
vkCmdSetStencilWriteMask:
invlpg [rax]
db "vkCmdSetStencilWriteMask", 0
ret

global vkCmdSetStencilReference:function
align 16
vkCmdSetStencilReference:
invlpg [rax]
db "vkCmdSetStencilReference", 0
ret

global vkCmdBindVertexBuffers:function
align 16
vkCmdBindVertexBuffers:
invlpg [rax]
db "vkCmdBindVertexBuffers", 0
ret

global vkCmdSetViewport:function
align 16
vkCmdSetViewport:
invlpg [rax]
db "vkCmdSetViewport", 0
ret

global vkCmdSetLineWidth:function
align 16
vkCmdSetLineWidth:
invlpg [rax]
db "vkCmdSetLineWidth", 0
ret

global vkCmdSetDepthBias:function
align 16
vkCmdSetDepthBias:
invlpg [rax]
db "vkCmdSetDepthBias", 0
ret

global vkCmdSetBlendConstants:function
align 16
vkCmdSetBlendConstants:
invlpg [rax]
db "vkCmdSetBlendConstants", 0
ret

global vkGetPhysicalDeviceSparseImageFormatProperties:function
align 16
vkGetPhysicalDeviceSparseImageFormatProperties:
invlpg [rax]
db "vkGetPhysicalDeviceSparseImageFormatProperties", 0
ret

global vkGetImageSparseMemoryRequirements:function
align 16
vkGetImageSparseMemoryRequirements:
invlpg [rax]
db "vkGetImageSparseMemoryRequirements", 0
ret

global vkQueueBindSparse:function
align 16
vkQueueBindSparse:
invlpg [rax]
db "vkQueueBindSparse", 0
ret

global vkCmdDispatch:function
align 16
vkCmdDispatch:
invlpg [rax]
db "vkCmdDispatch", 0
ret

global vkCmdDispatchIndirect:function
align 16
vkCmdDispatchIndirect:
invlpg [rax]
db "vkCmdDispatchIndirect", 0
ret

global vkEnumerateInstanceLayerProperties:function
align 16
vkEnumerateInstanceLayerProperties:
invlpg [rax]
db "vkEnumerateInstanceLayerProperties", 0
ret

global vkEnumerateDeviceLayerProperties:function
align 16
vkEnumerateDeviceLayerProperties:
invlpg [rax]
db "vkEnumerateDeviceLayerProperties", 0
ret

global vkEnumerateInstanceExtensionProperties:function
align 16
vkEnumerateInstanceExtensionProperties:
invlpg [rax]
db "vkEnumerateInstanceExtensionProperties", 0
ret

global vkEnumerateDeviceExtensionProperties:function
align 16
vkEnumerateDeviceExtensionProperties:
invlpg [rax]
db "vkEnumerateDeviceExtensionProperties", 0
ret

global vkGetPhysicalDeviceFeatures:function
align 16
vkGetPhysicalDeviceFeatures:
invlpg [rax]
db "vkGetPhysicalDeviceFeatures", 0
ret

global vkGetPhysicalDeviceFormatProperties:function
align 16
vkGetPhysicalDeviceFormatProperties:
invlpg [rax]
db "vkGetPhysicalDeviceFormatProperties", 0
ret

global vkGetPhysicalDeviceImageFormatProperties:function
align 16
vkGetPhysicalDeviceImageFormatProperties:
invlpg [rax]
db "vkGetPhysicalDeviceImageFormatProperties", 0
ret

global vkBindBufferMemory2:function
align 16
vkBindBufferMemory2:
invlpg [rax]
db "vkBindBufferMemory2", 0
ret

global vkBindBufferMemory2KHR:function
align 16
vkBindBufferMemory2KHR:
invlpg [rax]
db "vkBindBufferMemory2KHR", 0
ret

global vkBindImageMemory2:function
align 16
vkBindImageMemory2:
invlpg [rax]
db "vkBindImageMemory2", 0
ret

global vkBindImageMemory2KHR:function
align 16
vkBindImageMemory2KHR:
invlpg [rax]
db "vkBindImageMemory2KHR", 0
ret

global vkCmdDispatchBase:function
align 16
vkCmdDispatchBase:
invlpg [rax]
db "vkCmdDispatchBase", 0
ret

global vkCmdDispatchBaseKHR:function
align 16
vkCmdDispatchBaseKHR:
invlpg [rax]
db "vkCmdDispatchBaseKHR", 0
ret

global vkCmdSetDeviceMask:function
align 16
vkCmdSetDeviceMask:
invlpg [rax]
db "vkCmdSetDeviceMask", 0
ret

global vkCmdSetDeviceMaskKHR:function
align 16
vkCmdSetDeviceMaskKHR:
invlpg [rax]
db "vkCmdSetDeviceMaskKHR", 0
ret

global vkCreateDescriptorUpdateTemplate:function
align 16
vkCreateDescriptorUpdateTemplate:
invlpg [rax]
db "vkCreateDescriptorUpdateTemplate", 0
ret

global vkCreateDescriptorUpdateTemplateKHR:function
align 16
vkCreateDescriptorUpdateTemplateKHR:
invlpg [rax]
db "vkCreateDescriptorUpdateTemplateKHR", 0
ret

global vkCreateSamplerYcbcrConversion:function
align 16
vkCreateSamplerYcbcrConversion:
invlpg [rax]
db "vkCreateSamplerYcbcrConversion", 0
ret

global vkCreateSamplerYcbcrConversionKHR:function
align 16
vkCreateSamplerYcbcrConversionKHR:
invlpg [rax]
db "vkCreateSamplerYcbcrConversionKHR", 0
ret

global vkDestroyDescriptorUpdateTemplate:function
align 16
vkDestroyDescriptorUpdateTemplate:
invlpg [rax]
db "vkDestroyDescriptorUpdateTemplate", 0
ret

global vkDestroyDescriptorUpdateTemplateKHR:function
align 16
vkDestroyDescriptorUpdateTemplateKHR:
invlpg [rax]
db "vkDestroyDescriptorUpdateTemplateKHR", 0
ret

global vkDestroySamplerYcbcrConversion:function
align 16
vkDestroySamplerYcbcrConversion:
invlpg [rax]
db "vkDestroySamplerYcbcrConversion", 0
ret

global vkDestroySamplerYcbcrConversionKHR:function
align 16
vkDestroySamplerYcbcrConversionKHR:
invlpg [rax]
db "vkDestroySamplerYcbcrConversionKHR", 0
ret

global vkEnumerateInstanceVersion:function
align 16
vkEnumerateInstanceVersion:
invlpg [rax]
db "vkEnumerateInstanceVersion", 0
ret

global vkEnumerateInstanceVersionKHR:function
align 16
vkEnumerateInstanceVersionKHR:
invlpg [rax]
db "vkEnumerateInstanceVersionKHR", 0
ret

global vkEnumeratePhysicalDeviceGroups:function
align 16
vkEnumeratePhysicalDeviceGroups:
invlpg [rax]
db "vkEnumeratePhysicalDeviceGroups", 0
ret

global vkEnumeratePhysicalDeviceGroupsKHR:function
align 16
vkEnumeratePhysicalDeviceGroupsKHR:
invlpg [rax]
db "vkEnumeratePhysicalDeviceGroupsKHR", 0
ret

global vkGetBufferMemoryRequirements2:function
align 16
vkGetBufferMemoryRequirements2:
invlpg [rax]
db "vkGetBufferMemoryRequirements2", 0
ret

global vkGetBufferMemoryRequirements2KHR:function
align 16
vkGetBufferMemoryRequirements2KHR:
invlpg [rax]
db "vkGetBufferMemoryRequirements2KHR", 0
ret

global vkGetDescriptorSetLayoutSupport:function
align 16
vkGetDescriptorSetLayoutSupport:
invlpg [rax]
db "vkGetDescriptorSetLayoutSupport", 0
ret

global vkGetDescriptorSetLayoutSupportKHR:function
align 16
vkGetDescriptorSetLayoutSupportKHR:
invlpg [rax]
db "vkGetDescriptorSetLayoutSupportKHR", 0
ret

global vkGetDeviceGroupPeerMemoryFeatures:function
align 16
vkGetDeviceGroupPeerMemoryFeatures:
invlpg [rax]
db "vkGetDeviceGroupPeerMemoryFeatures", 0
ret

global vkGetDeviceGroupPeerMemoryFeaturesKHR:function
align 16
vkGetDeviceGroupPeerMemoryFeaturesKHR:
invlpg [rax]
db "vkGetDeviceGroupPeerMemoryFeaturesKHR", 0
ret

global vkGetDeviceQueue2:function
align 16
vkGetDeviceQueue2:
invlpg [rax]
db "vkGetDeviceQueue2", 0
ret

global vkGetDeviceQueue2KHR:function
align 16
vkGetDeviceQueue2KHR:
invlpg [rax]
db "vkGetDeviceQueue2KHR", 0
ret

global vkGetImageMemoryRequirements2:function
align 16
vkGetImageMemoryRequirements2:
invlpg [rax]
db "vkGetImageMemoryRequirements2", 0
ret

global vkGetImageMemoryRequirements2KHR:function
align 16
vkGetImageMemoryRequirements2KHR:
invlpg [rax]
db "vkGetImageMemoryRequirements2KHR", 0
ret

global vkGetImageSparseMemoryRequirements2:function
align 16
vkGetImageSparseMemoryRequirements2:
invlpg [rax]
db "vkGetImageSparseMemoryRequirements2", 0
ret

global vkGetImageSparseMemoryRequirements2KHR:function
align 16
vkGetImageSparseMemoryRequirements2KHR:
invlpg [rax]
db "vkGetImageSparseMemoryRequirements2KHR", 0
ret

global vkGetPhysicalDeviceExternalBufferProperties:function
align 16
vkGetPhysicalDeviceExternalBufferProperties:
invlpg [rax]
db "vkGetPhysicalDeviceExternalBufferProperties", 0
ret

global vkGetPhysicalDeviceExternalBufferPropertiesKHR:function
align 16
vkGetPhysicalDeviceExternalBufferPropertiesKHR:
invlpg [rax]
db "vkGetPhysicalDeviceExternalBufferPropertiesKHR", 0
ret

global vkGetPhysicalDeviceExternalFenceProperties:function
align 16
vkGetPhysicalDeviceExternalFenceProperties:
invlpg [rax]
db "vkGetPhysicalDeviceExternalFenceProperties", 0
ret

global vkGetPhysicalDeviceExternalFencePropertiesKHR:function
align 16
vkGetPhysicalDeviceExternalFencePropertiesKHR:
invlpg [rax]
db "vkGetPhysicalDeviceExternalFencePropertiesKHR", 0
ret

global vkGetPhysicalDeviceExternalSemaphoreProperties:function
align 16
vkGetPhysicalDeviceExternalSemaphoreProperties:
invlpg [rax]
db "vkGetPhysicalDeviceExternalSemaphoreProperties", 0
ret

global vkGetPhysicalDeviceExternalSemaphorePropertiesKHR:function
align 16
vkGetPhysicalDeviceExternalSemaphorePropertiesKHR:
invlpg [rax]
db "vkGetPhysicalDeviceExternalSemaphorePropertiesKHR", 0
ret

global vkGetPhysicalDeviceFeatures2:function
align 16
vkGetPhysicalDeviceFeatures2:
invlpg [rax]
db "vkGetPhysicalDeviceFeatures2", 0
ret

global vkGetPhysicalDeviceFeatures2KHR:function
align 16
vkGetPhysicalDeviceFeatures2KHR:
invlpg [rax]
db "vkGetPhysicalDeviceFeatures2KHR", 0
ret

global vkGetPhysicalDeviceFormatProperties2:function
align 16
vkGetPhysicalDeviceFormatProperties2:
invlpg [rax]
db "vkGetPhysicalDeviceFormatProperties2", 0
ret

global vkGetPhysicalDeviceFormatProperties2KHR:function
align 16
vkGetPhysicalDeviceFormatProperties2KHR:
invlpg [rax]
db "vkGetPhysicalDeviceFormatProperties2KHR", 0
ret

global vkGetPhysicalDeviceImageFormatProperties2:function
align 16
vkGetPhysicalDeviceImageFormatProperties2:
invlpg [rax]
db "vkGetPhysicalDeviceImageFormatProperties2", 0
ret

global vkGetPhysicalDeviceImageFormatProperties2KHR:function
align 16
vkGetPhysicalDeviceImageFormatProperties2KHR:
invlpg [rax]
db "vkGetPhysicalDeviceImageFormatProperties2KHR", 0
ret

global vkGetPhysicalDeviceMemoryProperties2:function
align 16
vkGetPhysicalDeviceMemoryProperties2:
invlpg [rax]
db "vkGetPhysicalDeviceMemoryProperties2", 0
ret

global vkGetPhysicalDeviceMemoryProperties2KHR:function
align 16
vkGetPhysicalDeviceMemoryProperties2KHR:
invlpg [rax]
db "vkGetPhysicalDeviceMemoryProperties2KHR", 0
ret

global vkGetPhysicalDeviceProperties2:function
align 16
vkGetPhysicalDeviceProperties2:
invlpg [rax]
db "vkGetPhysicalDeviceProperties2", 0
ret

global vkGetPhysicalDeviceProperties2KHR:function
align 16
vkGetPhysicalDeviceProperties2KHR:
invlpg [rax]
db "vkGetPhysicalDeviceProperties2KHR", 0
ret

global vkGetPhysicalDeviceQueueFamilyProperties2:function
align 16
vkGetPhysicalDeviceQueueFamilyProperties2:
invlpg [rax]
db "vkGetPhysicalDeviceQueueFamilyProperties2", 0
ret

global vkGetPhysicalDeviceQueueFamilyProperties2KHR:function
align 16
vkGetPhysicalDeviceQueueFamilyProperties2KHR:
invlpg [rax]
db "vkGetPhysicalDeviceQueueFamilyProperties2KHR", 0
ret

global vkGetPhysicalDeviceSparseImageFormatProperties2:function
align 16
vkGetPhysicalDeviceSparseImageFormatProperties2:
invlpg [rax]
db "vkGetPhysicalDeviceSparseImageFormatProperties2", 0
ret

global vkGetPhysicalDeviceSparseImageFormatProperties2KHR:function
align 16
vkGetPhysicalDeviceSparseImageFormatProperties2KHR:
invlpg [rax]
db "vkGetPhysicalDeviceSparseImageFormatProperties2KHR", 0
ret

global vkTrimCommandPool:function
align 16
vkTrimCommandPool:
invlpg [rax]
db "vkTrimCommandPool", 0
ret

global vkTrimCommandPoolKHR:function
align 16
vkTrimCommandPoolKHR:
invlpg [rax]
db "vkTrimCommandPoolKHR", 0
ret

global vkUpdateDescriptorSetWithTemplate:function
align 16
vkUpdateDescriptorSetWithTemplate:
invlpg [rax]
db "vkUpdateDescriptorSetWithTemplate", 0
ret

global vkUpdateDescriptorSetWithTemplateKHR:function
align 16
vkUpdateDescriptorSetWithTemplateKHR:
invlpg [rax]
db "vkUpdateDescriptorSetWithTemplateKHR", 0
ret

global vkCmdBeginRenderPass2:function
align 16
vkCmdBeginRenderPass2:
invlpg [rax]
db "vkCmdBeginRenderPass2", 0
ret

global vkCmdBeginRenderPass2KHR:function
align 16
vkCmdBeginRenderPass2KHR:
invlpg [rax]
db "vkCmdBeginRenderPass2KHR", 0
ret

global vkCmdEndRenderPass2:function
align 16
vkCmdEndRenderPass2:
invlpg [rax]
db "vkCmdEndRenderPass2", 0
ret

global vkCmdEndRenderPass2KHR:function
align 16
vkCmdEndRenderPass2KHR:
invlpg [rax]
db "vkCmdEndRenderPass2KHR", 0
ret

global vkCmdNextSubpass2:function
align 16
vkCmdNextSubpass2:
invlpg [rax]
db "vkCmdNextSubpass2", 0
ret

global vkCmdNextSubpass2KHR:function
align 16
vkCmdNextSubpass2KHR:
invlpg [rax]
db "vkCmdNextSubpass2KHR", 0
ret

global vkCreateRenderPass2:function
align 16
vkCreateRenderPass2:
invlpg [rax]
db "vkCreateRenderPass2", 0
ret

global vkCreateRenderPass2KHR:function
align 16
vkCreateRenderPass2KHR:
invlpg [rax]
db "vkCreateRenderPass2KHR", 0
ret

global vkGetBufferOpaqueCaptureAddress:function
align 16
vkGetBufferOpaqueCaptureAddress:
invlpg [rax]
db "vkGetBufferOpaqueCaptureAddress", 0
ret

global vkGetBufferOpaqueCaptureAddressKHR:function
align 16
vkGetBufferOpaqueCaptureAddressKHR:
invlpg [rax]
db "vkGetBufferOpaqueCaptureAddressKHR", 0
ret

global vkGetDeviceMemoryOpaqueCaptureAddress:function
align 16
vkGetDeviceMemoryOpaqueCaptureAddress:
invlpg [rax]
db "vkGetDeviceMemoryOpaqueCaptureAddress", 0
ret

global vkGetDeviceMemoryOpaqueCaptureAddressKHR:function
align 16
vkGetDeviceMemoryOpaqueCaptureAddressKHR:
invlpg [rax]
db "vkGetDeviceMemoryOpaqueCaptureAddressKHR", 0
ret

global vkGetSemaphoreCounterValue:function
align 16
vkGetSemaphoreCounterValue:
invlpg [rax]
db "vkGetSemaphoreCounterValue", 0
ret

global vkGetSemaphoreCounterValueKHR:function
align 16
vkGetSemaphoreCounterValueKHR:
invlpg [rax]
db "vkGetSemaphoreCounterValueKHR", 0
ret

global vkWaitSemaphores:function
align 16
vkWaitSemaphores:
invlpg [rax]
db "vkWaitSemaphores", 0
ret

global vkWaitSemaphoresKHR:function
align 16
vkWaitSemaphoresKHR:
invlpg [rax]
db "vkWaitSemaphoresKHR", 0
ret

global vkSignalSemaphore:function
align 16
vkSignalSemaphore:
invlpg [rax]
db "vkSignalSemaphore", 0
ret

global vkSignalSemaphoreKHR:function
align 16
vkSignalSemaphoreKHR:
invlpg [rax]
db "vkSignalSemaphoreKHR", 0
ret

global vkResetQueryPool:function
align 16
vkResetQueryPool:
invlpg [rax]
db "vkResetQueryPool", 0
ret

global vkResetQueryPoolEXT:function
align 16
vkResetQueryPoolEXT:
invlpg [rax]
db "vkResetQueryPoolEXT", 0
ret

global vkCmdDrawIndexedIndirectCount:function
align 16
vkCmdDrawIndexedIndirectCount:
invlpg [rax]
db "vkCmdDrawIndexedIndirectCount", 0
ret

global vkCmdDrawIndexedIndirectCountKHR:function
align 16
vkCmdDrawIndexedIndirectCountKHR:
invlpg [rax]
db "vkCmdDrawIndexedIndirectCountKHR", 0
ret

global vkCmdDrawIndexedIndirectCountAMD:function
align 16
vkCmdDrawIndexedIndirectCountAMD:
invlpg [rax]
db "vkCmdDrawIndexedIndirectCountAMD", 0
ret

global vkCmdDrawIndirectCount:function
align 16
vkCmdDrawIndirectCount:
invlpg [rax]
db "vkCmdDrawIndirectCount", 0
ret

global vkCmdDrawIndirectCountKHR:function
align 16
vkCmdDrawIndirectCountKHR:
invlpg [rax]
db "vkCmdDrawIndirectCountKHR", 0
ret

global vkCmdDrawIndirectCountAMD:function
align 16
vkCmdDrawIndirectCountAMD:
invlpg [rax]
db "vkCmdDrawIndirectCountAMD", 0
ret

global vkGetBufferDeviceAddress:function
align 16
vkGetBufferDeviceAddress:
invlpg [rax]
db "vkGetBufferDeviceAddress", 0
ret

global vkGetBufferDeviceAddressKHR:function
align 16
vkGetBufferDeviceAddressKHR:
invlpg [rax]
db "vkGetBufferDeviceAddressKHR", 0
ret

global vkGetBufferDeviceAddressEXT:function
align 16
vkGetBufferDeviceAddressEXT:
invlpg [rax]
db "vkGetBufferDeviceAddressEXT", 0
ret

global vkCmdBeginRendering:function
align 16
vkCmdBeginRendering:
invlpg [rax]
db "vkCmdBeginRendering", 0
ret

global vkCmdBeginRenderingKHR:function
align 16
vkCmdBeginRenderingKHR:
invlpg [rax]
db "vkCmdBeginRenderingKHR", 0
ret

global vkCmdBindVertexBuffers2:function
align 16
vkCmdBindVertexBuffers2:
invlpg [rax]
db "vkCmdBindVertexBuffers2", 0
ret

global vkCmdBindVertexBuffers2EXT:function
align 16
vkCmdBindVertexBuffers2EXT:
invlpg [rax]
db "vkCmdBindVertexBuffers2EXT", 0
ret

global vkCmdBlitImage2:function
align 16
vkCmdBlitImage2:
invlpg [rax]
db "vkCmdBlitImage2", 0
ret

global vkCmdBlitImage2KHR:function
align 16
vkCmdBlitImage2KHR:
invlpg [rax]
db "vkCmdBlitImage2KHR", 0
ret

global vkCmdCopyBuffer2:function
align 16
vkCmdCopyBuffer2:
invlpg [rax]
db "vkCmdCopyBuffer2", 0
ret

global vkCmdCopyBuffer2KHR:function
align 16
vkCmdCopyBuffer2KHR:
invlpg [rax]
db "vkCmdCopyBuffer2KHR", 0
ret

global vkCmdCopyBufferToImage2:function
align 16
vkCmdCopyBufferToImage2:
invlpg [rax]
db "vkCmdCopyBufferToImage2", 0
ret

global vkCmdCopyBufferToImage2KHR:function
align 16
vkCmdCopyBufferToImage2KHR:
invlpg [rax]
db "vkCmdCopyBufferToImage2KHR", 0
ret

global vkCmdCopyImage2:function
align 16
vkCmdCopyImage2:
invlpg [rax]
db "vkCmdCopyImage2", 0
ret

global vkCmdCopyImage2KHR:function
align 16
vkCmdCopyImage2KHR:
invlpg [rax]
db "vkCmdCopyImage2KHR", 0
ret

global vkCmdCopyImageToBuffer2:function
align 16
vkCmdCopyImageToBuffer2:
invlpg [rax]
db "vkCmdCopyImageToBuffer2", 0
ret

global vkCmdCopyImageToBuffer2KHR:function
align 16
vkCmdCopyImageToBuffer2KHR:
invlpg [rax]
db "vkCmdCopyImageToBuffer2KHR", 0
ret

global vkCmdEndRendering:function
align 16
vkCmdEndRendering:
invlpg [rax]
db "vkCmdEndRendering", 0
ret

global vkCmdEndRenderingKHR:function
align 16
vkCmdEndRenderingKHR:
invlpg [rax]
db "vkCmdEndRenderingKHR", 0
ret

global vkCmdPipelineBarrier2:function
align 16
vkCmdPipelineBarrier2:
invlpg [rax]
db "vkCmdPipelineBarrier2", 0
ret

global vkCmdPipelineBarrier2KHR:function
align 16
vkCmdPipelineBarrier2KHR:
invlpg [rax]
db "vkCmdPipelineBarrier2KHR", 0
ret

global vkCmdResetEvent2:function
align 16
vkCmdResetEvent2:
invlpg [rax]
db "vkCmdResetEvent2", 0
ret

global vkCmdResetEvent2KHR:function
align 16
vkCmdResetEvent2KHR:
invlpg [rax]
db "vkCmdResetEvent2KHR", 0
ret

global vkCmdResolveImage2:function
align 16
vkCmdResolveImage2:
invlpg [rax]
db "vkCmdResolveImage2", 0
ret

global vkCmdResolveImage2KHR:function
align 16
vkCmdResolveImage2KHR:
invlpg [rax]
db "vkCmdResolveImage2KHR", 0
ret

global vkCmdSetCullMode:function
align 16
vkCmdSetCullMode:
invlpg [rax]
db "vkCmdSetCullMode", 0
ret

global vkCmdSetCullModeEXT:function
align 16
vkCmdSetCullModeEXT:
invlpg [rax]
db "vkCmdSetCullModeEXT", 0
ret

global vkCmdSetDepthBiasEnable:function
align 16
vkCmdSetDepthBiasEnable:
invlpg [rax]
db "vkCmdSetDepthBiasEnable", 0
ret

global vkCmdSetDepthBiasEnableEXT:function
align 16
vkCmdSetDepthBiasEnableEXT:
invlpg [rax]
db "vkCmdSetDepthBiasEnableEXT", 0
ret

global vkCmdSetDepthBoundsTestEnable:function
align 16
vkCmdSetDepthBoundsTestEnable:
invlpg [rax]
db "vkCmdSetDepthBoundsTestEnable", 0
ret

global vkCmdSetDepthBoundsTestEnableEXT:function
align 16
vkCmdSetDepthBoundsTestEnableEXT:
invlpg [rax]
db "vkCmdSetDepthBoundsTestEnableEXT", 0
ret

global vkCmdSetDepthCompareOp:function
align 16
vkCmdSetDepthCompareOp:
invlpg [rax]
db "vkCmdSetDepthCompareOp", 0
ret

global vkCmdSetDepthCompareOpEXT:function
align 16
vkCmdSetDepthCompareOpEXT:
invlpg [rax]
db "vkCmdSetDepthCompareOpEXT", 0
ret

global vkCmdSetDepthTestEnable:function
align 16
vkCmdSetDepthTestEnable:
invlpg [rax]
db "vkCmdSetDepthTestEnable", 0
ret

global vkCmdSetDepthTestEnableEXT:function
align 16
vkCmdSetDepthTestEnableEXT:
invlpg [rax]
db "vkCmdSetDepthTestEnableEXT", 0
ret

global vkCmdSetDepthWriteEnable:function
align 16
vkCmdSetDepthWriteEnable:
invlpg [rax]
db "vkCmdSetDepthWriteEnable", 0
ret

global vkCmdSetDepthWriteEnableEXT:function
align 16
vkCmdSetDepthWriteEnableEXT:
invlpg [rax]
db "vkCmdSetDepthWriteEnableEXT", 0
ret

global vkCmdSetEvent2:function
align 16
vkCmdSetEvent2:
invlpg [rax]
db "vkCmdSetEvent2", 0
ret

global vkCmdSetEvent2KHR:function
align 16
vkCmdSetEvent2KHR:
invlpg [rax]
db "vkCmdSetEvent2KHR", 0
ret

global vkCmdSetFrontFace:function
align 16
vkCmdSetFrontFace:
invlpg [rax]
db "vkCmdSetFrontFace", 0
ret

global vkCmdSetFrontFaceEXT:function
align 16
vkCmdSetFrontFaceEXT:
invlpg [rax]
db "vkCmdSetFrontFaceEXT", 0
ret

global vkCmdSetPrimitiveTopology:function
align 16
vkCmdSetPrimitiveTopology:
invlpg [rax]
db "vkCmdSetPrimitiveTopology", 0
ret

global vkCmdSetPrimitiveTopologyEXT:function
align 16
vkCmdSetPrimitiveTopologyEXT:
invlpg [rax]
db "vkCmdSetPrimitiveTopologyEXT", 0
ret

global vkCmdSetRasterizerDiscardEnable:function
align 16
vkCmdSetRasterizerDiscardEnable:
invlpg [rax]
db "vkCmdSetRasterizerDiscardEnable", 0
ret

global vkCmdSetRasterizerDiscardEnableEXT:function
align 16
vkCmdSetRasterizerDiscardEnableEXT:
invlpg [rax]
db "vkCmdSetRasterizerDiscardEnableEXT", 0
ret

global vkCmdSetScissorWithCount:function
align 16
vkCmdSetScissorWithCount:
invlpg [rax]
db "vkCmdSetScissorWithCount", 0
ret

global vkCmdSetScissorWithCountEXT:function
align 16
vkCmdSetScissorWithCountEXT:
invlpg [rax]
db "vkCmdSetScissorWithCountEXT", 0
ret

global vkCmdSetStencilOp:function
align 16
vkCmdSetStencilOp:
invlpg [rax]
db "vkCmdSetStencilOp", 0
ret

global vkCmdSetStencilOpEXT:function
align 16
vkCmdSetStencilOpEXT:
invlpg [rax]
db "vkCmdSetStencilOpEXT", 0
ret

global vkCmdSetStencilTestEnable:function
align 16
vkCmdSetStencilTestEnable:
invlpg [rax]
db "vkCmdSetStencilTestEnable", 0
ret

global vkCmdSetStencilTestEnableEXT:function
align 16
vkCmdSetStencilTestEnableEXT:
invlpg [rax]
db "vkCmdSetStencilTestEnableEXT", 0
ret

global vkCmdSetViewportWithCount:function
align 16
vkCmdSetViewportWithCount:
invlpg [rax]
db "vkCmdSetViewportWithCount", 0
ret

global vkCmdSetViewportWithCountEXT:function
align 16
vkCmdSetViewportWithCountEXT:
invlpg [rax]
db "vkCmdSetViewportWithCountEXT", 0
ret

global vkCmdWaitEvents2:function
align 16
vkCmdWaitEvents2:
invlpg [rax]
db "vkCmdWaitEvents2", 0
ret

global vkCmdWaitEvents2KHR:function
align 16
vkCmdWaitEvents2KHR:
invlpg [rax]
db "vkCmdWaitEvents2KHR", 0
ret

global vkCmdWriteTimestamp2:function
align 16
vkCmdWriteTimestamp2:
invlpg [rax]
db "vkCmdWriteTimestamp2", 0
ret

global vkCmdWriteTimestamp2KHR:function
align 16
vkCmdWriteTimestamp2KHR:
invlpg [rax]
db "vkCmdWriteTimestamp2KHR", 0
ret

global vkCreatePrivateDataSlot:function
align 16
vkCreatePrivateDataSlot:
invlpg [rax]
db "vkCreatePrivateDataSlot", 0
ret

global vkCreatePrivateDataSlotEXT:function
align 16
vkCreatePrivateDataSlotEXT:
invlpg [rax]
db "vkCreatePrivateDataSlotEXT", 0
ret

global vkDestroyPrivateDataSlot:function
align 16
vkDestroyPrivateDataSlot:
invlpg [rax]
db "vkDestroyPrivateDataSlot", 0
ret

global vkDestroyPrivateDataSlotEXT:function
align 16
vkDestroyPrivateDataSlotEXT:
invlpg [rax]
db "vkDestroyPrivateDataSlotEXT", 0
ret

global vkGetDeviceBufferMemoryRequirements:function
align 16
vkGetDeviceBufferMemoryRequirements:
invlpg [rax]
db "vkGetDeviceBufferMemoryRequirements", 0
ret

global vkGetDeviceBufferMemoryRequirementsKHR:function
align 16
vkGetDeviceBufferMemoryRequirementsKHR:
invlpg [rax]
db "vkGetDeviceBufferMemoryRequirementsKHR", 0
ret

global vkGetDeviceImageMemoryRequirements:function
align 16
vkGetDeviceImageMemoryRequirements:
invlpg [rax]
db "vkGetDeviceImageMemoryRequirements", 0
ret

global vkGetDeviceImageMemoryRequirementsKHR:function
align 16
vkGetDeviceImageMemoryRequirementsKHR:
invlpg [rax]
db "vkGetDeviceImageMemoryRequirementsKHR", 0
ret

global vkGetDeviceImageSparseMemoryRequirements:function
align 16
vkGetDeviceImageSparseMemoryRequirements:
invlpg [rax]
db "vkGetDeviceImageSparseMemoryRequirements", 0
ret

global vkGetDeviceImageSparseMemoryRequirementsKHR:function
align 16
vkGetDeviceImageSparseMemoryRequirementsKHR:
invlpg [rax]
db "vkGetDeviceImageSparseMemoryRequirementsKHR", 0
ret

global vkGetPhysicalDeviceToolProperties:function
align 16
vkGetPhysicalDeviceToolProperties:
invlpg [rax]
db "vkGetPhysicalDeviceToolProperties", 0
ret

global vkGetPhysicalDeviceToolPropertiesEXT:function
align 16
vkGetPhysicalDeviceToolPropertiesEXT:
invlpg [rax]
db "vkGetPhysicalDeviceToolPropertiesEXT", 0
ret

global vkGetPrivateData:function
align 16
vkGetPrivateData:
invlpg [rax]
db "vkGetPrivateData", 0
ret

global vkGetPrivateDataEXT:function
align 16
vkGetPrivateDataEXT:
invlpg [rax]
db "vkGetPrivateDataEXT", 0
ret

global vkQueueSubmit2:function
align 16
vkQueueSubmit2:
invlpg [rax]
db "vkQueueSubmit2", 0
ret

global vkQueueSubmit2KHR:function
align 16
vkQueueSubmit2KHR:
invlpg [rax]
db "vkQueueSubmit2KHR", 0
ret

global vkSetPrivateData:function
align 16
vkSetPrivateData:
invlpg [rax]
db "vkSetPrivateData", 0
ret

global vkSetPrivateDataEXT:function
align 16
vkSetPrivateDataEXT:
invlpg [rax]
db "vkSetPrivateDataEXT", 0
ret

global vkCreateDebugReportCallbackEXT:function
align 16
vkCreateDebugReportCallbackEXT:
invlpg [rax]
db "vkCreateDebugReportCallbackEXT", 0
ret

global vkDebugReportMessageEXT:function
align 16
vkDebugReportMessageEXT:
invlpg [rax]
db "vkDebugReportMessageEXT", 0
ret

global vkDestroyDebugReportCallbackEXT:function
align 16
vkDestroyDebugReportCallbackEXT:
invlpg [rax]
db "vkDestroyDebugReportCallbackEXT", 0
ret

global vkDestroySurfaceKHR:function
align 16
vkDestroySurfaceKHR:
invlpg [rax]
db "vkDestroySurfaceKHR", 0
ret

global vkGetPhysicalDeviceSurfaceCapabilitiesKHR:function
align 16
vkGetPhysicalDeviceSurfaceCapabilitiesKHR:
invlpg [rax]
db "vkGetPhysicalDeviceSurfaceCapabilitiesKHR", 0
ret

global vkGetPhysicalDeviceSurfaceFormatsKHR:function
align 16
vkGetPhysicalDeviceSurfaceFormatsKHR:
invlpg [rax]
db "vkGetPhysicalDeviceSurfaceFormatsKHR", 0
ret

global vkGetPhysicalDeviceSurfacePresentModesKHR:function
align 16
vkGetPhysicalDeviceSurfacePresentModesKHR:
invlpg [rax]
db "vkGetPhysicalDeviceSurfacePresentModesKHR", 0
ret

global vkGetPhysicalDeviceSurfaceSupportKHR:function
align 16
vkGetPhysicalDeviceSurfaceSupportKHR:
invlpg [rax]
db "vkGetPhysicalDeviceSurfaceSupportKHR", 0
ret

global vkAcquireNextImageKHR:function
align 16
vkAcquireNextImageKHR:
invlpg [rax]
db "vkAcquireNextImageKHR", 0
ret

global vkCreateSwapchainKHR:function
align 16
vkCreateSwapchainKHR:
invlpg [rax]
db "vkCreateSwapchainKHR", 0
ret

global vkDestroySwapchainKHR:function
align 16
vkDestroySwapchainKHR:
invlpg [rax]
db "vkDestroySwapchainKHR", 0
ret

global vkGetSwapchainImagesKHR:function
align 16
vkGetSwapchainImagesKHR:
invlpg [rax]
db "vkGetSwapchainImagesKHR", 0
ret

global vkQueuePresentKHR:function
align 16
vkQueuePresentKHR:
invlpg [rax]
db "vkQueuePresentKHR", 0
ret

global vkAcquireNextImage2KHR:function
align 16
vkAcquireNextImage2KHR:
invlpg [rax]
db "vkAcquireNextImage2KHR", 0
ret

global vkGetDeviceGroupPresentCapabilitiesKHR:function
align 16
vkGetDeviceGroupPresentCapabilitiesKHR:
invlpg [rax]
db "vkGetDeviceGroupPresentCapabilitiesKHR", 0
ret

global vkGetDeviceGroupSurfacePresentModesKHR:function
align 16
vkGetDeviceGroupSurfacePresentModesKHR:
invlpg [rax]
db "vkGetDeviceGroupSurfacePresentModesKHR", 0
ret

global vkGetPhysicalDevicePresentRectanglesKHR:function
align 16
vkGetPhysicalDevicePresentRectanglesKHR:
invlpg [rax]
db "vkGetPhysicalDevicePresentRectanglesKHR", 0
ret

global vkCreateSharedSwapchainsKHR:function
align 16
vkCreateSharedSwapchainsKHR:
invlpg [rax]
db "vkCreateSharedSwapchainsKHR", 0
ret

global vkGetPhysicalDeviceSurfaceFormats2KHR:function
align 16
vkGetPhysicalDeviceSurfaceFormats2KHR:
invlpg [rax]
db "vkGetPhysicalDeviceSurfaceFormats2KHR", 0
ret

global vkGetPhysicalDeviceSurfaceCapabilities2KHR:function
align 16
vkGetPhysicalDeviceSurfaceCapabilities2KHR:
invlpg [rax]
db "vkGetPhysicalDeviceSurfaceCapabilities2KHR", 0
ret

global vkCreateWaylandSurfaceKHR:function
align 16
vkCreateWaylandSurfaceKHR:
invlpg [rax]
db "vkCreateWaylandSurfaceKHR", 0
ret

global vkGetPhysicalDeviceWaylandPresentationSupportKHR:function
align 16
vkGetPhysicalDeviceWaylandPresentationSupportKHR:
invlpg [rax]
db "vkGetPhysicalDeviceWaylandPresentationSupportKHR", 0
ret

global vkCreateXcbSurfaceKHR:function
align 16
vkCreateXcbSurfaceKHR:
invlpg [rax]
db "vkCreateXcbSurfaceKHR", 0
ret

global vkGetPhysicalDeviceXcbPresentationSupportKHR:function
align 16
vkGetPhysicalDeviceXcbPresentationSupportKHR:
invlpg [rax]
db "vkGetPhysicalDeviceXcbPresentationSupportKHR", 0
ret

global vkCreateXlibSurfaceKHR:function
align 16
vkCreateXlibSurfaceKHR:
invlpg [rax]
db "vkCreateXlibSurfaceKHR", 0
ret

global vkGetPhysicalDeviceXlibPresentationSupportKHR:function
align 16
vkGetPhysicalDeviceXlibPresentationSupportKHR:
invlpg [rax]
db "vkGetPhysicalDeviceXlibPresentationSupportKHR", 0
ret

section .init_array
    dq __felix86_constructor