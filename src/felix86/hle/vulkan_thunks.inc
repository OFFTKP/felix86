// Read abi.cpp -> signature format
// X(lib name, func name, signature)

#define KHR(lib, func, sig) X(lib, func, sig) X(lib, func##KHR, sig)
#define EXT(lib, func, sig) X(lib, func, sig) X(lib, func##EXT, sig)
#define KHR_EXT(lib, func, sig) X(lib, func, sig) X(lib, func##KHR, sig) X(lib, func##EXT, sig)
#define KHR_AMD(lib, func, sig) X(lib, func, sig) X(lib, func##KHR, sig) X(lib, func##AMD, sig)

// Vulkan 1.0
X("libvulkan.so", vkGetInstanceProcAddr, q_qq)
X("libvulkan.so", vkGetDeviceProcAddr, q_qq)
X("libvulkan.so", vkCreateInstance, d_qxq)
X("libvulkan.so", vkDestroyInstance, v_qx)
X("libvulkan.so", vkEnumeratePhysicalDevices, d_qqq)
X("libvulkan.so", vkGetPhysicalDeviceProperties, v_qq)
X("libvulkan.so", vkGetPhysicalDeviceQueueFamilyProperties, v_qqq)
X("libvulkan.so", vkCreateDevice, d_qqxq)
X("libvulkan.so", vkDeviceWaitIdle, d_q)
X("libvulkan.so", vkDestroyDevice, v_qx)
X("libvulkan.so", vkGetDeviceQueue, v_qddq)
X("libvulkan.so", vkQueueWaitIdle, d_q)
X("libvulkan.so", vkCreateCommandPool, d_qqxq)
X("libvulkan.so", vkResetCommandPool, d_qqd)
X("libvulkan.so", vkDestroyCommandPool, v_qqx)
X("libvulkan.so", vkAllocateCommandBuffers, d_qqq)
X("libvulkan.so", vkResetCommandBuffer, d_qq)
X("libvulkan.so", vkFreeCommandBuffers, v_qqdq)
X("libvulkan.so", vkBeginCommandBuffer, d_qq)
X("libvulkan.so", vkEndCommandBuffer, d_q)
X("libvulkan.so", vkQueueSubmit, d_qdqq)
X("libvulkan.so", vkCmdExecuteCommands, v_qdq)
X("libvulkan.so", vkCreateFence, d_qqxq)
X("libvulkan.so", vkDestroyFence, v_qqx)
X("libvulkan.so", vkGetFenceStatus, d_qq)
X("libvulkan.so", vkResetFences, d_qdq)
X("libvulkan.so", vkWaitForFences, d_qdqdq)
X("libvulkan.so", vkCreateSemaphore, d_qqxq)
X("libvulkan.so", vkDestroySemaphore, v_qqx)
X("libvulkan.so", vkCreateEvent, d_qqxq)
X("libvulkan.so", vkDestroyEvent, v_qqx)
X("libvulkan.so", vkGetEventStatus, d_qq)
X("libvulkan.so", vkSetEvent, d_qq)
X("libvulkan.so", vkResetEvent, d_qq)
X("libvulkan.so", vkCmdSetEvent, d_qqd)
X("libvulkan.so", vkCmdResetEvent, d_qqd)
X("libvulkan.so", vkCmdWaitEvents, v_qdqdddqdqdq)
X("libvulkan.so", vkCmdPipelineBarrier, v_qddddqdqdq)
X("libvulkan.so", vkCreateRenderPass, d_qqxq)
X("libvulkan.so", vkDestroyRenderPass, v_qqx)
X("libvulkan.so", vkCreateFramebuffer, d_qqxq)
X("libvulkan.so", vkDestroyFramebuffer, v_qqx)
X("libvulkan.so", vkCmdBeginRenderPass, v_qqd)
X("libvulkan.so", vkGetRenderAreaGranularity, v_qqq)
X("libvulkan.so", vkCmdNextSubpass, v_qd)
X("libvulkan.so", vkCmdEndRenderPass, v_q)
X("libvulkan.so", vkCreateShaderModule, d_qqxq)
X("libvulkan.so", vkDestroyShaderModule, v_qqx)
X("libvulkan.so", vkCreateComputePipelines, d_qqdqxq)
X("libvulkan.so", vkCreateGraphicsPipelines, d_qqdqxq)
X("libvulkan.so", vkDestroyPipeline, v_qqx)
X("libvulkan.so", vkCreatePipelineCache, d_qqxq)
X("libvulkan.so", vkMergePipelineCaches, d_qqdq)
X("libvulkan.so", vkGetPipelineCacheData, d_qqqq)
X("libvulkan.so", vkDestroyPipelineCache, v_qqx)
X("libvulkan.so", vkCmdBindPipeline, v_qdq)
X("libvulkan.so", vkGetPhysicalDeviceMemoryProperties, v_qq)
X("libvulkan.so", vkAllocateMemory, d_qqxq)
X("libvulkan.so", vkFreeMemory, v_qqx)
X("libvulkan.so", vkMapMemory, d_qqqqdq)
X("libvulkan.so", vkFlushMappedMemoryRanges, d_qdq)
X("libvulkan.so", vkInvalidateMappedMemoryRanges, d_qdq)
X("libvulkan.so", vkUnmapMemory, v_qq)
X("libvulkan.so", vkGetDeviceMemoryCommitment, v_qqq)
X("libvulkan.so", vkCreateBuffer, d_qqxq)
X("libvulkan.so", vkDestroyBuffer, v_qqx)
X("libvulkan.so", vkCreateBufferView, d_qqxq)
X("libvulkan.so", vkDestroyBufferView, v_qqx)
X("libvulkan.so", vkCreateImage, d_qqxq)
X("libvulkan.so", vkGetImageSubresourceLayout, v_qqqq)
X("libvulkan.so", vkDestroyImage, v_qqx)
X("libvulkan.so", vkCreateImageView, d_qqxq)
X("libvulkan.so", vkDestroyImageView, v_qqx)
X("libvulkan.so", vkGetBufferMemoryRequirements, v_qqq)
X("libvulkan.so", vkGetImageMemoryRequirements, v_qqq)
X("libvulkan.so", vkBindBufferMemory, d_qqqq)
X("libvulkan.so", vkBindImageMemory, d_qqqq)
X("libvulkan.so", vkCreateSampler, d_qqxq)
X("libvulkan.so", vkDestroySampler, v_qqx)
X("libvulkan.so", vkCreateDescriptorSetLayout, d_qqxq)
X("libvulkan.so", vkDestroyDescriptorSetLayout, v_qqx)
X("libvulkan.so", vkCreatePipelineLayout, d_qqxq)
X("libvulkan.so", vkDestroyPipelineLayout, v_qqx)
X("libvulkan.so", vkCreateDescriptorPool, d_qqxq)
X("libvulkan.so", vkDestroyDescriptorPool, v_qqx)
X("libvulkan.so", vkAllocateDescriptorSets, d_qqq)
X("libvulkan.so", vkFreeDescriptorSets, d_qqdq)
X("libvulkan.so", vkResetDescriptorPool, d_qqd)
X("libvulkan.so", vkUpdateDescriptorSets, v_qdqdq)
X("libvulkan.so", vkCmdBindDescriptorSets, v_qqqddqdq)
X("libvulkan.so", vkCmdPushConstants, v_qqdddq)
X("libvulkan.so", vkCreateQueryPool, d_qqxq)
X("libvulkan.so", vkDestroyQueryPool, v_qqx)
X("libvulkan.so", vkCmdResetQueryPool, v_qqdd)
X("libvulkan.so", vkCmdBeginQuery, v_qqdd)
X("libvulkan.so", vkCmdEndQuery, v_qqd)
X("libvulkan.so", vkGetQueryPoolResults, d_qqddqqqd)
X("libvulkan.so", vkCmdCopyQueryPoolResults, v_qqddqqqd)
X("libvulkan.so", vkCmdWriteTimestamp, v_qdqd)
X("libvulkan.so", vkCmdClearColorImage, v_qqqqdq)
X("libvulkan.so", vkCmdClearDepthStencilImage, v_qqqqdq)
X("libvulkan.so", vkCmdClearAttachments, v_qdqdq)
X("libvulkan.so", vkCmdFillBuffer, v_qqqqd)
X("libvulkan.so", vkCmdUpdateBuffer, v_qqqqq)
X("libvulkan.so", vkCmdCopyBuffer, v_qqqdq)
X("libvulkan.so", vkCmdCopyImage, v_qqqqqdq)
X("libvulkan.so", vkCmdCopyBufferToImage, v_qqqqdq)
X("libvulkan.so", vkCmdCopyImageToBuffer, v_qqqqdq)
X("libvulkan.so", vkCmdBlitImage, v_qqqqqdqq)
X("libvulkan.so", vkCmdResolveImage, v_qqqqqdq)
X("libvulkan.so", vkCmdBindIndexBuffer, v_qqqd)
X("libvulkan.so", vkCmdDraw, v_qdddd)
X("libvulkan.so", vkCmdDrawIndexed, v_qddddd)
X("libvulkan.so", vkCmdDrawIndirect, v_qqqdd)
X("libvulkan.so", vkCmdDrawIndexedIndirect, v_qqqdd)
X("libvulkan.so", vkCmdSetScissor, v_qddq)
X("libvulkan.so", vkCmdSetDepthBounds, v_qFF)
X("libvulkan.so", vkCmdSetStencilCompareMask, v_qdd)
X("libvulkan.so", vkCmdSetStencilWriteMask, v_qdd)
X("libvulkan.so", vkCmdSetStencilReference, v_qdd)
X("libvulkan.so", vkCmdBindVertexBuffers, v_qddqq)
X("libvulkan.so", vkCmdSetViewport, v_qddq)
X("libvulkan.so", vkCmdSetLineWidth, v_qF)
X("libvulkan.so", vkCmdSetDepthBias, v_qFFF)
X("libvulkan.so", vkCmdSetBlendConstants, v_qq)
X("libvulkan.so", vkGetPhysicalDeviceSparseImageFormatProperties, v_qdddddqq)
X("libvulkan.so", vkGetImageSparseMemoryRequirements, v_qqqq)
X("libvulkan.so", vkQueueBindSparse, d_qdqq)
X("libvulkan.so", vkCmdDispatch, v_qddd)
X("libvulkan.so", vkCmdDispatchIndirect, v_qqq)

X("libvulkan.so", vkEnumerateInstanceLayerProperties, d_qq)
X("libvulkan.so", vkEnumerateDeviceLayerProperties, d_qqq)
X("libvulkan.so", vkEnumerateInstanceExtensionProperties, d_qqq)
X("libvulkan.so", vkEnumerateDeviceExtensionProperties, d_qqqq)
X("libvulkan.so", vkGetPhysicalDeviceFeatures, v_qq)
X("libvulkan.so", vkGetPhysicalDeviceFormatProperties, v_qdq)
X("libvulkan.so", vkGetPhysicalDeviceImageFormatProperties, d_qdddddq)

// Vulkan 1.1
KHR("libvulkan.so", vkBindBufferMemory2, d_qdq)
KHR("libvulkan.so", vkBindImageMemory2, d_qdq)
KHR("libvulkan.so", vkCmdDispatchBase, v_qdddddd)
KHR("libvulkan.so", vkCmdSetDeviceMask, v_qd)
KHR("libvulkan.so", vkCreateDescriptorUpdateTemplate, d_qqxq)
KHR("libvulkan.so", vkCreateSamplerYcbcrConversion, d_qqxq)
KHR("libvulkan.so", vkDestroyDescriptorUpdateTemplate, v_qqx)
KHR("libvulkan.so", vkDestroySamplerYcbcrConversion, v_qqx)
KHR("libvulkan.so", vkEnumerateInstanceVersion, d_q)
KHR("libvulkan.so", vkEnumeratePhysicalDeviceGroups, d_qqq)
KHR("libvulkan.so", vkGetBufferMemoryRequirements2, v_qqq)
KHR("libvulkan.so", vkGetDescriptorSetLayoutSupport, v_qqq)
KHR("libvulkan.so", vkGetDeviceGroupPeerMemoryFeatures, v_qdddq)
KHR("libvulkan.so", vkGetDeviceQueue2, v_qqq)
KHR("libvulkan.so", vkGetImageMemoryRequirements2, v_qqq)
KHR("libvulkan.so", vkGetImageSparseMemoryRequirements2, v_qqqq)
KHR("libvulkan.so", vkGetPhysicalDeviceExternalBufferProperties, v_qqq)
KHR("libvulkan.so", vkGetPhysicalDeviceExternalFenceProperties, v_qqq)
KHR("libvulkan.so", vkGetPhysicalDeviceExternalSemaphoreProperties, v_qqq)
KHR("libvulkan.so", vkGetPhysicalDeviceFeatures2, v_qq)
KHR("libvulkan.so", vkGetPhysicalDeviceFormatProperties2, v_qqq)
KHR("libvulkan.so", vkGetPhysicalDeviceImageFormatProperties2, d_qqq)
KHR("libvulkan.so", vkGetPhysicalDeviceMemoryProperties2, v_qq)
KHR("libvulkan.so", vkGetPhysicalDeviceProperties2, v_qq)
KHR("libvulkan.so", vkGetPhysicalDeviceQueueFamilyProperties2, v_qqq)
KHR("libvulkan.so", vkGetPhysicalDeviceSparseImageFormatProperties2, v_qqqq)
KHR("libvulkan.so", vkTrimCommandPool, v_qqq)
KHR("libvulkan.so", vkUpdateDescriptorSetWithTemplate, v_qqqq)

// Vulkan 1.2
KHR("libvulkan.so", vkCmdBeginRenderPass2, v_qqq)
KHR("libvulkan.so", vkCmdEndRenderPass2, v_qq)
KHR("libvulkan.so", vkCmdNextSubpass2, v_qqq)
KHR("libvulkan.so", vkCreateRenderPass2, d_qqxq)
KHR("libvulkan.so", vkGetBufferOpaqueCaptureAddress, q_qq)
KHR("libvulkan.so", vkGetDeviceMemoryOpaqueCaptureAddress, q_qq)
KHR("libvulkan.so", vkGetSemaphoreCounterValue, q_qqq)
KHR("libvulkan.so", vkWaitSemaphores, d_qqq)
KHR("libvulkan.so", vkSignalSemaphore, d_qq)
EXT("libvulkan.so", vkResetQueryPool, v_qqdd)
KHR_AMD("libvulkan.so", vkCmdDrawIndexedIndirectCount, v_qqqqqdd)
KHR_AMD("libvulkan.so", vkCmdDrawIndirectCount, v_qqqqqdd)
KHR_EXT("libvulkan.so", vkGetBufferDeviceAddress, q_qq)

// Vulkan 1.3
KHR("libvulkan.so", vkCmdBeginRendering, v_qq)
EXT("libvulkan.so", vkCmdBindVertexBuffers2, v_qddqqqq)
KHR("libvulkan.so", vkCmdBlitImage2, v_qq)
KHR("libvulkan.so", vkCmdCopyBuffer2, v_qq)
KHR("libvulkan.so", vkCmdCopyBufferToImage2, v_qq)
KHR("libvulkan.so", vkCmdCopyImage2, v_qq)
KHR("libvulkan.so", vkCmdCopyImageToBuffer2, v_qq)
KHR("libvulkan.so", vkCmdEndRendering, v_q)
KHR("libvulkan.so", vkCmdPipelineBarrier2, v_qq)
KHR("libvulkan.so", vkCmdResetEvent2, v_qqq)
KHR("libvulkan.so", vkCmdResolveImage2, v_qq)
EXT("libvulkan.so", vkCmdSetCullMode, v_qd)
EXT("libvulkan.so", vkCmdSetDepthBiasEnable, v_qd)
EXT("libvulkan.so", vkCmdSetDepthBoundsTestEnable, v_qd)
EXT("libvulkan.so", vkCmdSetDepthCompareOp, v_qd)
EXT("libvulkan.so", vkCmdSetDepthTestEnable, v_qd)
EXT("libvulkan.so", vkCmdSetDepthWriteEnable, v_qd)
KHR("libvulkan.so", vkCmdSetEvent2, v_qqq)
EXT("libvulkan.so", vkCmdSetFrontFace, v_qd)
EXT("libvulkan.so", vkCmdSetPrimitiveTopology, v_qd)
EXT("libvulkan.so", vkCmdSetPrimitiveRestartEnable, v_qd)
EXT("libvulkan.so", vkCmdSetRasterizerDiscardEnable, v_qd)
EXT("libvulkan.so", vkCmdSetScissorWithCount, v_qdq)
EXT("libvulkan.so", vkCmdSetStencilOp, v_qddddd)
EXT("libvulkan.so", vkCmdSetStencilTestEnable, v_qd)
EXT("libvulkan.so", vkCmdSetViewportWithCount, v_qdq)
KHR("libvulkan.so", vkCmdWaitEvents2, v_qdqq)
KHR("libvulkan.so", vkCmdWriteTimestamp2, v_qqqd)
EXT("libvulkan.so", vkCreatePrivateDataSlot, d_qqxq)
EXT("libvulkan.so", vkDestroyPrivateDataSlot, v_qqx)
KHR("libvulkan.so", vkGetDeviceBufferMemoryRequirements, v_qqq)
KHR("libvulkan.so", vkGetDeviceImageMemoryRequirements, v_qqq)
KHR("libvulkan.so", vkGetDeviceImageSparseMemoryRequirements, v_qqqq)
EXT("libvulkan.so", vkGetPhysicalDeviceToolProperties, d_qqq)
EXT("libvulkan.so", vkGetPrivateData, v_qdqqq)
KHR("libvulkan.so", vkQueueSubmit2, d_qdqq)
EXT("libvulkan.so", vkSetPrivateData, d_qdqqq)

// Vulkan 1.4
KHR("libvulkan.so", vkMapMemory2, d_qqq)
KHR("libvulkan.so", vkUnmapMemory2, d_qq)
EXT("libvulkan.so", vkCopyMemoryToImage, d_qq)
EXT("libvulkan.so", vkCopyImageToMemory, d_qq)
EXT("libvulkan.so", vkCopyImageToImage, d_qq)
EXT("libvulkan.so", vkTransitionImageLayout, d_qdq)
KHR("libvulkan.so", vkCmdPushDescriptorSetWithTemplate, v_qqqdq)
KHR("libvulkan.so", vkCmdSetRenderingAttachmentLocations, v_qq)
KHR("libvulkan.so", vkCmdSetRenderingInputAttachmentIndices, v_qq)

// EXT
X("libvulkan.so", vkCreateDebugReportCallbackEXT, d_qqxq)
X("libvulkan.so", vkDebugReportMessageEXT, v_qddqqdqq)
X("libvulkan.so", vkDestroyDebugReportCallbackEXT, v_qqx)

// VK_KHR_surface
X("libvulkan.so", vkDestroySurfaceKHR, v_qqx)
X("libvulkan.so", vkGetPhysicalDeviceSurfaceCapabilitiesKHR, d_qqq)
X("libvulkan.so", vkGetPhysicalDeviceSurfaceFormatsKHR, d_qqqq)
X("libvulkan.so", vkGetPhysicalDeviceSurfacePresentModesKHR, d_qqqq)
X("libvulkan.so", vkGetPhysicalDeviceSurfaceSupportKHR, d_qdqq)

// VK_KHR_swapchain
X("libvulkan.so", vkAcquireNextImageKHR, d_qqqqqq)
X("libvulkan.so", vkCreateSwapchainKHR, d_qqxq)
X("libvulkan.so", vkDestroySwapchainKHR, v_qqx)
X("libvulkan.so", vkGetSwapchainImagesKHR, d_qqqq)
X("libvulkan.so", vkQueuePresentKHR, d_qq)

// VK_KHR_swapchain 1.1
X("libvulkan.so", vkAcquireNextImage2KHR, d_qqq)
X("libvulkan.so", vkGetDeviceGroupPresentCapabilitiesKHR, d_qq)
X("libvulkan.so", vkGetDeviceGroupSurfacePresentModesKHR, d_qqq)
X("libvulkan.so", vkGetPhysicalDevicePresentRectanglesKHR, d_qqqq)

// VK_KHR_display_swapchain
X("libvulkan.so", vkCreateSharedSwapchainsKHR, d_qdqqq)

// VK_KHR_get_surface_capabilities2
X("libvulkan.so", vkGetPhysicalDeviceSurfaceFormats2KHR, d_qqqq)
X("libvulkan.so", vkGetPhysicalDeviceSurfaceCapabilities2KHR, d_qqq)

// KHR Wayland
X("libvulkan.so", vkCreateWaylandSurfaceKHR, d_qqxq)
X("libvulkan.so", vkGetPhysicalDeviceWaylandPresentationSupportKHR, d_qdq)

// KHR Xcb
X("libvulkan.so", vkCreateXcbSurfaceKHR, d_qqxq)
X("libvulkan.so", vkGetPhysicalDeviceXcbPresentationSupportKHR, d_qdqq)

// KHR Xlib
X("libvulkan.so", vkCreateXlibSurfaceKHR, d_qqxq)
X("libvulkan.so", vkGetPhysicalDeviceXlibPresentationSupportKHR, d_qdqq)

// VK_EXT_descriptor_buffer
X("libvulkan.so", vkCmdBindDescriptorBufferEmbeddedSamplersEXT, v_qdqd)
X("libvulkan.so", vkCmdBindDescriptorBuffersEXT, v_qdq)
X("libvulkan.so", vkCmdSetDescriptorBufferOffsetsEXT, v_qdqddqq)
X("libvulkan.so", vkGetBufferOpaqueCaptureDescriptorDataEXT, d_qqq)
X("libvulkan.so", vkGetDescriptorEXT, v_qqqq)
X("libvulkan.so", vkGetDescriptorSetLayoutBindingOffsetEXT, v_qqdq)
X("libvulkan.so", vkGetDescriptorSetLayoutSizeEXT, v_qqq)
X("libvulkan.so", vkGetImageOpaqueCaptureDescriptorDataEXT, d_qqq)
X("libvulkan.so", vkGetImageViewOpaqueCaptureDescriptorDataEXT, d_qqq)
X("libvulkan.so", vkGetSamplerOpaqueCaptureDescriptorDataEXT, d_qqq)
X("libvulkan.so", vkGetAccelerationStructureOpaqueCaptureDescriptorDataEXT, d_qqq)

// VK_EXT_shader_module_identifier
X("libvulkan.so", vkGetShaderModuleCreateInfoIdentifierEXT, v_qqq)
X("libvulkan.so", vkGetShaderModuleIdentifierEXT, v_qqq)

// VK_EXT_extended_dynamic_state2
EXT("libvulkan.so", vkCmdSetLogicOp, v_qd)
EXT("libvulkan.so", vkCmdSetPatchControlPoints, v_qd)

// VK_EXT_extended_dynamic_state3
EXT("libvulkan.so", vkCmdSetAlphaToCoverageEnable, v_qd)
EXT("libvulkan.so", vkCmdSetAlphaToOneEnable, v_qd)
EXT("libvulkan.so", vkCmdSetColorBlendEnable, v_qddq)
EXT("libvulkan.so", vkCmdSetColorBlendEquation, v_qddq)
EXT("libvulkan.so", vkCmdSetColorWriteMask, v_qddq)
EXT("libvulkan.so", vkCmdSetDepthClampEnable, v_qddq)
EXT("libvulkan.so", vkCmdSetLogicOpEnable, v_qd)
EXT("libvulkan.so", vkCmdSetPolygonMode, v_qd)
EXT("libvulkan.so", vkCmdSetRasterizationSamples, v_qd)
EXT("libvulkan.so", vkCmdSetSampleMask, v_qdq)
EXT("libvulkan.so", vkCmdSetColorBlendAdvanced, v_qddq)
EXT("libvulkan.so", vkCmdSetConservativeRasterizationMode, v_qd)
EXT("libvulkan.so", vkCmdSetExtraPrimitiveOverestimationSize, v_qF)
EXT("libvulkan.so", vkCmdSetDepthClipNegativeOneToOne, v_qd)
EXT("libvulkan.so", vkCmdSetDepthClipEnable, v_qd)
EXT("libvulkan.so", vkCmdSetLineRasterizationMode, v_qd)
EXT("libvulkan.so", vkCmdSetLineStippleEnable, v_qd)
EXT("libvulkan.so", vkCmdSetProvokingVertexMode, v_qd)
EXT("libvulkan.so", vkCmdSetSampleLocationsEnable, v_qd)
EXT("libvulkan.so", vkCmdSetRasterizationStream, v_qd)
EXT("libvulkan.so", vkCmdSetTessellationDomainOrigin, v_qd)

// VK_EXT_depth_bias_control
EXT("libvulkan.so", vkCmdSetDepthBias2, v_qq)

// VK_EXT_sample_locations
EXT("libvulkan.so", vkCmdSetSampleLocations, v_qq)
EXT("libvulkan.so", vkGetPhysicalDeviceMultisampleProperties, v_qdq)

// VK_EXT_transform_feedback
EXT("libvulkan.so", vkCmdBeginQueryIndexed, v_qqddd)
EXT("libvulkan.so", vkCmdBeginTransformFeedback, v_qddqq)
EXT("libvulkan.so", vkCmdBindTransformFeedbackBuffers, v_qddqqq)
EXT("libvulkan.so", vkCmdDrawIndirectByteCount, v_qddqqdd)
EXT("libvulkan.so", vkCmdEndQueryIndexed, v_qqdd)
EXT("libvulkan.so", vkCmdEndTransformFeedback, v_qddqq)

// VK_EXT_multi_draw
EXT("libvulkan.so", vkCmdDrawMulti, v_qdqddd)
EXT("libvulkan.so", vkCmdDrawMultiIndexed, v_qdqdddq)

// VK_EXT_hdr_metadata
EXT("libvulkan.so", vkSetHdrMetadata, v_qdqq)

// VK_EXT_pageable_device_local_memory
EXT("libvulkan.so", vkSetDeviceMemoryPriority, v_qqF)

// VK_KHR_swapchain_maintenance1
KHR_EXT("libvulkan.so", vkReleaseSwapchainImages, d_qq)

// VK_KHR_maintenance5
KHR_EXT("libvulkan.so", vkCmdBindIndexBuffer2, v_qqqqd)
KHR_EXT("libvulkan.so", vkGetDeviceImageSubresourceLayout, v_qqq)
KHR_EXT("libvulkan.so", vkGetImageSubresourceLayout2, v_qqqq)
KHR_EXT("libvulkan.so", vkGetRenderingAreaGranularity, v_qqq)

// VK_KHR_maintenance6
KHR("libvulkan.so", vkCmdBindDescriptorSets2, v_qq)
KHR("libvulkan.so", vkCmdPushConstants2, v_qq)
EXT("libvulkan.so", vkCmdBindDescriptorBufferEmbeddedSamplers2, v_qq)
EXT("libvulkan.so", vkCmdSetDescriptorBufferOffsets2, v_qq)
KHR("libvulkan.so", vkCmdPushDescriptorSet2, v_qq)
KHR("libvulkan.so", vkCmdPushDescriptorSetWithTemplate2, v_qq)

// VK_KHR_present_wait
KHR("libvulkan.so", vkWaitForPresent, d_qqqq)

// VK_KHR_present_wait2
KHR("libvulkan.so", vkWaitForPresent2, d_qqq)

// VK_KHR_line_rasterization
KHR_EXT("libvulkan.so", vkCmdSetLineStipple, v_qdw)

// VK_KHR_calibrated_timestamps
KHR_EXT("libvulkan.so", vkGetCalibratedTimestamps, d_qdqqq)
KHR_EXT("libvulkan.so", vkGetPhysicalDeviceCalibrateableTimeDomains, d_qqq)

// VK_EXT_debug_utils
EXT("libvulkan.so", vkCmdBeginDebugUtilsLabel, v_qq)
EXT("libvulkan.so", vkCmdEndDebugUtilsLabel, v_q)
EXT("libvulkan.so", vkCmdInsertDebugUtilsLabel, v_qq)
EXT("libvulkan.so", vkCreateDebugUtilsMessenger, d_qqxq)
EXT("libvulkan.so", vkDestroyDebugUtilsMessenger, v_qqx)
EXT("libvulkan.so", vkQueueBeginDebugUtilsLabel, v_qq)
EXT("libvulkan.so", vkQueueEndDebugUtilsLabel, v_q)
EXT("libvulkan.so", vkQueueInsertDebugUtilsLabel, v_qq)
EXT("libvulkan.so", vkSetDebugUtilsObjectName, d_qq)
EXT("libvulkan.so", vkSetDebugUtilsObjectTag, d_qq)
EXT("libvulkan.so", vkSubmitDebugUtilsMessage, v_qddq)

// VK_EXT_attachment_feedback_loop_dynamic_state
EXT("libvulkan.so", vkCmdSetAttachmentFeedbackLoopEnable, v_qd)

// VK_EXT_conditional_rendering
EXT("libvulkan.so", vkCmdBeginConditionalRendering, v_qq)
EXT("libvulkan.so", vkCmdEndConditionalRendering, v_q)

// VK_KHR_external_memory_fd
KHR("libvulkan.so", vkGetMemoryFd, d_qqq)
KHR("libvulkan.so", vkGetMemoryFdProperties, d_qddq)

// VK_KHR_push_descriptor
KHR("libvulkan.so", vkCmdPushDescriptorSet, v_qdqddq)

// VK_EXT_color_write_enable
EXT("libvulkan.so", vkCmdSetColorWriteEnable, v_qdq)

// VK_EXT_shader_object
EXT("libvulkan.so", vkCmdBindShaders, v_qdqq)
EXT("libvulkan.so", vkCreateShaders, d_qdqxq)
EXT("libvulkan.so", vkDestroyShader, v_qqx)
EXT("libvulkan.so", vkGetShaderBinaryData, d_qqqq)
EXT("libvulkan.so", vkCmdSetVertexInput, v_qdqdq)

// VK_EXT_external_memory_host
EXT("libvulkan.so", vkGetMemoryHostPointerProperties, d_qdqq)

// VK_KHR_fragment_shading_rate
KHR("libvulkan.so", vkCmdSetFragmentShadingRate, v_qqq)
KHR("libvulkan.so", vkGetPhysicalDeviceFragmentShadingRates, d_qqq)

// VK_AMD_buffer_marker
X("libvulkan.so", vkCmdWriteBufferMarkerAMD, v_qdqqd)
X("libvulkan.so", vkCmdWriteBufferMarker2AMD, v_qqqqd)

// VK_EXT_descriptor_heap
X("libvulkan.so", vkCmdBindResourceHeapEXT, v_qq)
X("libvulkan.so", vkCmdBindSamplerHeapEXT, v_qq)
X("libvulkan.so", vkCmdPushDataEXT, v_qq)
X("libvulkan.so", vkGetImageOpaqueCaptureDataEXT, d_qdqq)
X("libvulkan.so", vkGetPhysicalDeviceDescriptorSizeEXT, q_qd)
X("libvulkan.so", vkWriteResourceDescriptorsEXT, d_qdqq)
X("libvulkan.so", vkWriteSamplerDescriptorsEXT, d_qdqq)
X("libvulkan.so", vkRegisterCustomBorderColorEXT, d_qqdq)
X("libvulkan.so", vkUnregisterCustomBorderColorEXT, v_qd)

// VK_KHR_external_semaphore_fd
X("libvulkan.so", vkGetSemaphoreFdKHR, d_qqq)
X("libvulkan.so", vkImportSemaphoreFdKHR, d_qq)

// VK_KHR_external_fence_fd
X("libvulkan.so", vkImportFenceFdKHR, d_qq)
X("libvulkan.so", vkGetFenceFdKHR, d_qqq)

// VK_KHR_device_address_commands
X("libvulkan.so", vkCmdBindIndexBuffer3KHR, v_qq)
X("libvulkan.so", vkCmdBindVertexBuffers3KHR, v_qddq)
X("libvulkan.so", vkCmdDrawIndirect2KHR, v_qq)
X("libvulkan.so", vkCmdDrawIndexedIndirect2KHR, v_qq)
X("libvulkan.so", vkCmdDispatchIndirect2KHR, v_qq)
X("libvulkan.so", vkCmdCopyMemoryKHR, v_qq)
X("libvulkan.so", vkCmdCopyMemoryToImageKHR, v_qq)
X("libvulkan.so", vkCmdCopyImageToMemoryKHR, v_qq)
X("libvulkan.so", vkCmdUpdateMemoryKHR, v_qqdqq)
X("libvulkan.so", vkCmdFillMemoryKHR, v_qqdd)
X("libvulkan.so", vkCmdCopyQueryPoolResultsToMemoryKHR, v_qqddqdd)
X("libvulkan.so", vkCmdDrawIndirectCount2KHR, v_qq)
X("libvulkan.so", vkCmdDrawIndexedIndirectCount2KHR, v_qq)
X("libvulkan.so", vkCmdBeginConditionalRendering2EXT, v_qq)
X("libvulkan.so", vkCmdBindTransformFeedbackBuffers2EXT, v_qddq)
X("libvulkan.so", vkCmdBeginTransformFeedback2EXT, v_qddq)
X("libvulkan.so", vkCmdEndTransformFeedback2EXT, v_qddq)
X("libvulkan.so", vkCmdDrawIndirectByteCount2EXT, v_qddqdd)
X("libvulkan.so", vkCmdDrawMeshTasksIndirect2EXT, v_qq)
X("libvulkan.so", vkCmdDrawMeshTasksIndirectCount2EXT, v_qq)
X("libvulkan.so", vkCmdWriteMarkerToMemoryAMD, v_qq)
X("libvulkan.so", vkCreateAccelerationStructure2KHR, d_qqxq)

// VK_EXT_present_timing
X("libvulkan.so", vkSetSwapchainPresentTimingQueueSizeEXT, d_qqd)
X("libvulkan.so", vkGetSwapchainTimingPropertiesEXT, d_qqqq)
X("libvulkan.so", vkGetSwapchainTimeDomainPropertiesEXT, d_qqqq)
X("libvulkan.so", vkGetPastPresentationTimingEXT, d_qqq)

// VK_EXT_image_drm_format_modifier
X("libvulkan.so", vkGetImageDrmFormatModifierPropertiesEXT, d_qqq)

// VK_KHR_maintenance10
KHR_EXT("libvulkan.so", vkCmdEndRendering2, v_qq)

// VK_EXT_custom_resolve
X("libvulkan.so", vkCmdBeginCustomResolveEXT, v_qq)

// VK_EXT_primitive_restart_index
X("libvulkan.so", vkCmdSetPrimitiveRestartIndexEXT, v_qd)

// VK_KHR_copy_memory_indirect
X("libvulkan.so", vkCmdCopyMemoryIndirectKHR, v_qq)
X("libvulkan.so", vkCmdCopyMemoryToImageIndirectKHR, v_qq)

// VK_KHR_deferred_host_operations
X("libvulkan.so", vkCreateDeferredOperationKHR, d_qxq)
X("libvulkan.so", vkDestroyDeferredOperationKHR, v_qqx)
X("libvulkan.so", vkGetDeferredOperationMaxConcurrencyKHR, d_qq)
X("libvulkan.so", vkGetDeferredOperationResultKHR, d_qq)
X("libvulkan.so", vkDeferredOperationJoinKHR, d_qq)

// VK_KHR_acceleration_structure
X("libvulkan.so", vkCreateAccelerationStructureKHR, d_qqxq)
X("libvulkan.so", vkDestroyAccelerationStructureKHR, v_qqx)
X("libvulkan.so", vkCmdBuildAccelerationStructuresKHR, v_qdqq)
X("libvulkan.so", vkCmdBuildAccelerationStructuresIndirectKHR, v_qdqqqq)
X("libvulkan.so", vkBuildAccelerationStructuresKHR, d_qqdqq)
X("libvulkan.so", vkCopyAccelerationStructureKHR, d_qqq)
X("libvulkan.so", vkCopyAccelerationStructureToMemoryKHR, d_qqq)
X("libvulkan.so", vkCopyMemoryToAccelerationStructureKHR, d_qqq)
X("libvulkan.so", vkWriteAccelerationStructuresPropertiesKHR, d_qdqdqqq)
X("libvulkan.so", vkCmdCopyAccelerationStructureKHR, v_qq)
X("libvulkan.so", vkCmdCopyAccelerationStructureToMemoryKHR, v_qq)
X("libvulkan.so", vkCmdCopyMemoryToAccelerationStructureKHR, v_qq)
X("libvulkan.so", vkGetAccelerationStructureDeviceAddressKHR, q_qq)
X("libvulkan.so", vkCmdWriteAccelerationStructuresPropertiesKHR, v_qdqdqd)
X("libvulkan.so", vkGetDeviceAccelerationStructureCompatibilityKHR, v_qqq)
X("libvulkan.so", vkGetAccelerationStructureBuildSizesKHR, v_qdqqq)

// VK_KHR_ray_tracing_pipeline
X("libvulkan.so", vkCmdTraceRaysKHR, v_qqqqqddd)
X("libvulkan.so", vkCreateRayTracingPipelinesKHR, d_qqqdqxq)
X("libvulkan.so", vkGetRayTracingCaptureReplayShaderGroupHandlesKHR, d_qqddqq)
X("libvulkan.so", vkCmdTraceRaysIndirectKHR, v_qqqqqq)
X("libvulkan.so", vkGetRayTracingShaderGroupStackSizeKHR, q_qqdd)
X("libvulkan.so", vkCmdSetRayTracingPipelineStackSizeKHR, v_qd)

// VK_KHR_ray_tracing_maintenance1
X("libvulkan.so", vkCmdTraceRaysIndirect2KHR, v_qq)

// VK_EXT_mesh_shader
X("libvulkan.so", vkCmdDrawMeshTasksEXT, v_qddd)
X("libvulkan.so", vkCmdDrawMeshTasksIndirectEXT, v_qqqdd)
X("libvulkan.so", vkCmdDrawMeshTasksIndirectCountEXT, v_qqqqqdd)

// VK_EXT_device_generated_commands
X("libvulkan.so", vkGetGeneratedCommandsMemoryRequirementsEXT, v_qqq)
X("libvulkan.so", vkCmdPreprocessGeneratedCommandsEXT, v_qqq)
X("libvulkan.so", vkCmdExecuteGeneratedCommandsEXT, v_qdq)
X("libvulkan.so", vkCreateIndirectCommandsLayoutEXT, d_qqxq)
X("libvulkan.so", vkDestroyIndirectCommandsLayoutEXT, v_qqx)
X("libvulkan.so", vkCreateIndirectExecutionSetEXT, d_qqxq)
X("libvulkan.so", vkDestroyIndirectExecutionSetEXT, v_qqx)
X("libvulkan.so", vkUpdateIndirectExecutionSetPipelineEXT, v_qqdq)
X("libvulkan.so", vkUpdateIndirectExecutionSetShaderEXT, v_qqdq)

// VK_EXT_device_fault
X("libvulkan.so", vkGetDeviceFaultInfoEXT, d_qqq)

// VK_KHR_cooperative_matrix
X("libvulkan.so", vkGetPhysicalDeviceCooperativeMatrixPropertiesKHR, d_qqq)

// VK_KHR_pipeline_executable_properties
X("libvulkan.so", vkGetPipelineExecutablePropertiesKHR, d_qqqq)
X("libvulkan.so", vkGetPipelineExecutableStatisticsKHR, d_qqqq)
X("libvulkan.so", vkGetPipelineExecutableInternalRepresentationsKHR, d_qqqq)

// VK_KHR_pipeline_binary
X("libvulkan.so", vkCreatePipelineBinariesKHR, d_qqxq)
X("libvulkan.so", vkDestroyPipelineBinaryKHR, v_qqx)
X("libvulkan.so", vkGetPipelineKeyKHR, d_qqq)
X("libvulkan.so", vkGetPipelineBinaryDataKHR, d_qqqqq)
X("libvulkan.so", vkReleaseCapturedPipelineDataKHR, d_qqx)

// VK_KHR_video_queue
X("libvulkan.so", vkGetPhysicalDeviceVideoCapabilitiesKHR, d_qqq)
X("libvulkan.so", vkGetPhysicalDeviceVideoFormatPropertiesKHR, d_qqqq)
X("libvulkan.so", vkCreateVideoSessionKHR, d_qqxq)
X("libvulkan.so", vkDestroyVideoSessionKHR, v_qqx)
X("libvulkan.so", vkGetVideoSessionMemoryRequirementsKHR, d_qqqq)
X("libvulkan.so", vkBindVideoSessionMemoryKHR, d_qqdq)
X("libvulkan.so", vkCreateVideoSessionParametersKHR, d_qqxq)
X("libvulkan.so", vkUpdateVideoSessionParametersKHR, d_qqq)
X("libvulkan.so", vkDestroyVideoSessionParametersKHR, v_qqx)
X("libvulkan.so", vkCmdBeginVideoCodingKHR, v_qq)
X("libvulkan.so", vkCmdEndVideoCodingKHR, v_qq)
X("libvulkan.so", vkCmdControlVideoCodingKHR, v_qq)

// VK_KHR_video_decode_queue
X("libvulkan.so", vkCmdDecodeVideoKHR, v_qq)

// VK_KHR_video_encode_queue
X("libvulkan.so", vkGetPhysicalDeviceVideoEncodeQualityLevelPropertiesKHR, d_qqq)
X("libvulkan.so", vkGetEncodedVideoSessionParametersKHR, d_qqqqq)
X("libvulkan.so", vkCmdEncodeVideoKHR, v_qq)

#undef KHR
#undef EXT
#undef KHR_EXT
#undef KHR_AMD