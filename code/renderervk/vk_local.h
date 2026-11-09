/*
===========================================================================
Copyright (C) 2026 the OpenMOHAA Plus contributors

This file is part of OpenMOHAA Plus source code.

OpenMOHAA Plus source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMOHAA Plus source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMOHAA Plus source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// vk_local.h: Added in OpenMOHAA Plus
//  The Vulkan renderer (renderer_vulkan) is the OpenGL 1 renderer (code/renderergl1) built
//  against an emulation of the OpenGL 1.x calls it makes (vk_gl.c) on top of Vulkan (vk_core.c).
//  See docs/dev/architecture/renderer-vulkan.md.

#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "../qcommon/q_shared.h"

// Vulkan functions, loaded at run time (no link-time dependency on the Vulkan loader)
#define VK_GLOBAL_FUNCS \
    VKF(vkCreateInstance) \
    VKF(vkEnumerateInstanceExtensionProperties)

#define VK_INSTANCE_FUNCS \
    VKF(vkDestroyInstance) \
    VKF(vkEnumeratePhysicalDevices) \
    VKF(vkGetPhysicalDeviceProperties) \
    VKF(vkGetPhysicalDeviceFeatures) \
    VKF(vkGetPhysicalDeviceQueueFamilyProperties) \
    VKF(vkGetPhysicalDeviceMemoryProperties) \
    VKF(vkGetPhysicalDeviceFormatProperties) \
    VKF(vkEnumerateDeviceExtensionProperties) \
    VKF(vkGetPhysicalDeviceSurfaceSupportKHR) \
    VKF(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
    VKF(vkGetPhysicalDeviceSurfaceFormatsKHR) \
    VKF(vkGetPhysicalDeviceSurfacePresentModesKHR) \
    VKF(vkDestroySurfaceKHR) \
    VKF(vkCreateDevice) \
    VKF(vkGetDeviceProcAddr)

#define VK_DEVICE_FUNCS \
    VKF(vkDestroyDevice) \
    VKF(vkGetDeviceQueue) \
    VKF(vkDeviceWaitIdle) \
    VKF(vkCreateSwapchainKHR) \
    VKF(vkDestroySwapchainKHR) \
    VKF(vkGetSwapchainImagesKHR) \
    VKF(vkAcquireNextImageKHR) \
    VKF(vkQueuePresentKHR) \
    VKF(vkQueueSubmit) \
    VKF(vkAllocateMemory) \
    VKF(vkFreeMemory) \
    VKF(vkMapMemory) \
    VKF(vkUnmapMemory) \
    VKF(vkCreateBuffer) \
    VKF(vkDestroyBuffer) \
    VKF(vkGetBufferMemoryRequirements) \
    VKF(vkBindBufferMemory) \
    VKF(vkCreateImage) \
    VKF(vkDestroyImage) \
    VKF(vkGetImageMemoryRequirements) \
    VKF(vkBindImageMemory) \
    VKF(vkCreateImageView) \
    VKF(vkDestroyImageView) \
    VKF(vkCreateSampler) \
    VKF(vkDestroySampler) \
    VKF(vkCreateRenderPass) \
    VKF(vkDestroyRenderPass) \
    VKF(vkCreateFramebuffer) \
    VKF(vkDestroyFramebuffer) \
    VKF(vkCreateShaderModule) \
    VKF(vkDestroyShaderModule) \
    VKF(vkCreateDescriptorSetLayout) \
    VKF(vkDestroyDescriptorSetLayout) \
    VKF(vkCreatePipelineLayout) \
    VKF(vkDestroyPipelineLayout) \
    VKF(vkCreatePipelineCache) \
    VKF(vkDestroyPipelineCache) \
    VKF(vkCreateGraphicsPipelines) \
    VKF(vkDestroyPipeline) \
    VKF(vkCreateCommandPool) \
    VKF(vkDestroyCommandPool) \
    VKF(vkAllocateCommandBuffers) \
    VKF(vkResetCommandBuffer) \
    VKF(vkBeginCommandBuffer) \
    VKF(vkEndCommandBuffer) \
    VKF(vkCreateFence) \
    VKF(vkDestroyFence) \
    VKF(vkWaitForFences) \
    VKF(vkResetFences) \
    VKF(vkCreateSemaphore) \
    VKF(vkDestroySemaphore) \
    VKF(vkCmdBeginRenderPass) \
    VKF(vkCmdEndRenderPass) \
    VKF(vkCmdBindPipeline) \
    VKF(vkCmdBindVertexBuffers) \
    VKF(vkCmdBindIndexBuffer) \
    VKF(vkCmdDrawIndexed) \
    VKF(vkCmdSetViewport) \
    VKF(vkCmdSetScissor) \
    VKF(vkCmdSetDepthBias) \
    VKF(vkCmdSetLineWidth) \
    VKF(vkCmdSetStencilReference) \
    VKF(vkCmdSetStencilCompareMask) \
    VKF(vkCmdSetStencilWriteMask) \
    VKF(vkCmdClearAttachments) \
    VKF(vkCmdPipelineBarrier) \
    VKF(vkCmdCopyBufferToImage) \
    VKF(vkCmdCopyImageToBuffer) \
    VKF(vkCmdBlitImage) \
    VKF(vkCmdPushDescriptorSetKHR)

#define VKF(name) extern PFN_##name q##name;
VK_GLOBAL_FUNCS
VK_INSTANCE_FUNCS
VK_DEVICE_FUNCS
#undef VKF

#define VK_CHECK(call) VK_Check((call), #call)
void VK_Check(VkResult result, const char *what);

typedef struct vkTexture_s {
    VkImage        image;
    VkImageView    view;
    VkDeviceMemory memory;
    int            width, height;
    int            levels;        // mip levels the image has
    int            viewLevels;    // levels the current view covers (the uploaded ones, from 0)
    uint32_t       definedMask;   // bit per uploaded level
    VkImageLayout  layout;
} vkTexture_t;

typedef struct {
    // a GL draw reduced to the state a Vulkan pipeline needs
    uint32_t blendSrc, blendDst; // GL enums, 0 = blending off
    uint32_t depthFunc;          // GL enum, 0 = depth test off
    uint8_t  depthWrite;
    uint8_t  cullMode;           // 0 none, 1 front, 2 back (GL faces)
    uint8_t  depthBias;
    uint8_t  colorMask;          // RGBA bits
    uint8_t  topology;           // 0 triangles, 1 lines, 2 points
    uint8_t  polygonLine;
    uint8_t  stencilTest;
    uint8_t  pad;
    uint32_t stencilFunc, stencilFail, stencilZFail, stencilZPass;
} vkPipelineKey_t;

typedef struct {
    float   xyz[3];
    uint8_t color[4];
    float   st0[2];
    float   st1[2];
} vkVertex_t;

// per-draw shader state (layout matches DrawState in shaders/ffp_common.glsl)
typedef struct {
    float mvp[16];
    float modelView[16];
    float clipPlane[4];
    float fogColor[4];
    float fogParams[4];
    float alphaParams[4];
    int   misc[4];
    int   env[10][4];
} vkDrawState_t;

typedef struct {
    float x, y, w, h;   // framebuffer pixels, top-left origin
    float minDepth, maxDepth;
} vkViewport_t;

// vk_core.c
qboolean VK_Init(void *sdlWindow, int width, int height, qboolean headless);
void     VK_Shutdown(void);
void     VK_EndFrame(void); // present (or, headless, just finish the frame)
void     VK_Finish(void);   // submit everything recorded so far and wait for it
const char *VK_DeviceName(void);
int      VK_MaxTextureSize(void);
float    VK_MaxAnisotropy(void);
int      VK_FramebufferWidth(void);
int      VK_FramebufferHeight(void);

vkTexture_t *VK_CreateTexture(int width, int height, int levels);
void     VK_DestroyTexture(vkTexture_t *tex);
void     VK_UploadTexture(vkTexture_t *tex, int level, int x, int y, int width, int height, const byte *rgba);
VkSampler VK_GetSampler(VkFilter minFilter, VkFilter magFilter, VkSamplerMipmapMode mipmapMode, qboolean mipmaps,
                        VkSamplerAddressMode wrapS, VkSamplerAddressMode wrapT, float anisotropy);

void VK_Draw(
    const vkPipelineKey_t *key,
    const vkDrawState_t   *state,
    const vkViewport_t    *viewport,
    const VkRect2D        *scissor,
    float                  depthBiasConstant,
    float                  depthBiasSlope,
    float                  lineWidth,
    uint32_t               stencilRef,
    uint32_t               stencilMask,
    uint32_t               stencilWriteMask,
    vkTexture_t           *tex0,
    VkSampler              sampler0,
    vkTexture_t           *tex1,
    VkSampler              sampler1,
    const vkVertex_t      *vertices,
    int                    numVertices,
    const uint32_t        *indices,
    int                    numIndices
);
void VK_Clear(const VkRect2D *rect, qboolean color, const float rgba[4], qboolean depth, float depthValue,
              qboolean stencil, uint32_t stencilValue);
// rows top to bottom, RGBA
void VK_ReadPixels(int x, int y, int width, int height, byte *rgba);

// vk_gl.c
void VK_GL_SetProcAddresses(void);
void VK_GL_Shutdown(void);
