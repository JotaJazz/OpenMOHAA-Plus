/*
===========================================================================
Copyright (C) 2026 the OpenMOHAA Plus contributors

This file is part of OpenMOHAA Plus source code.

OpenMOHAA Plus source code is free software: you can redistribute it
and/or modify it under the terms of the GNU Affero General Public License
as published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

OpenMOHAA Plus source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with OpenMOHAA Plus source code.  If not, see <https://www.gnu.org/licenses/>.
===========================================================================
*/

// vk_core.c: Added in OpenMOHAA Plus
//  Vulkan device, frame and resources for the Vulkan renderer. Everything is drawn into an
//  offscreen image the size of the video mode; at the end of a frame it is blitted to the
//  swapchain (or, headless, kept for screenshots). The device setup follows Quake3e's vk.c
//  (https://github.com/ec-/Quake3e, GPL-2.0).
//
//  shortcut: one frame in flight (every submit waits for the GPU) and one memory allocation
//  per texture; double-buffer the per-frame rings and sub-allocate when performance matters.

#include "vk_local.h"
#include "../renderercommon/tr_common.h"
#include "vk_shaders.h"

#ifdef USE_INTERNAL_SDL_HEADERS
#    include "SDL.h"
#    include "SDL_vulkan.h"
#else
#    include <SDL.h>
#    include <SDL_vulkan.h>
#endif

#define VKF(name) PFN_##name q##name;
VK_GLOBAL_FUNCS
VK_INSTANCE_FUNCS
VK_DEVICE_FUNCS
#undef VKF

static PFN_vkGetInstanceProcAddr qvkGetInstanceProcAddr;

#define VERTEX_RING_SIZE  (16 * 1024 * 1024)
#define INDEX_RING_SIZE   (8 * 1024 * 1024)
#define UNIFORM_RING_SIZE (16 * 1024 * 1024)
#define STAGING_RING_SIZE (32 * 1024 * 1024)
#define MAX_PIPELINES     1024
#define MAX_SAMPLERS      64
#define MAX_DEFERRED      4096

typedef struct {
    VkBuffer       buffer;
    VkDeviceMemory memory;
    byte          *mapped;
    VkDeviceSize   size;
    VkDeviceSize   used;
} vkRing_t;

typedef struct {
    vkPipelineKey_t key;
    VkPipeline      pipeline;
} vkPipelineEntry_t;

typedef struct {
    VkFilter             minFilter, magFilter;
    VkSamplerMipmapMode  mipmapMode;
    qboolean             mipmaps;
    VkSamplerAddressMode wrapS, wrapT;
    float                anisotropy;
    VkSampler            sampler;
} vkSamplerEntry_t;

static struct {
    void            *sdlLib;
    SDL_Window      *window;
    qboolean         headless;
    VkInstance       instance;
    VkSurfaceKHR     surface;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties       props;
    VkPhysicalDeviceFeatures         features;
    VkPhysicalDeviceMemoryProperties memProps;
    VkDevice         device;
    uint32_t         queueFamily;
    VkQueue          queue;

    VkSwapchainKHR   swapchain;
    VkFormat         swapchainFormat;
    VkExtent2D       swapchainExtent;
    uint32_t         numSwapchainImages;
    VkImage          swapchainImages[8];
    VkSemaphore      acquireSemaphore, renderSemaphore;

    int              width, height;
    VkFormat         depthFormat;
    VkImage          colorImage, depthImage;
    VkDeviceMemory   colorMemory, depthMemory;
    VkImageView      colorView, depthView;
    VkRenderPass     renderPass;
    VkFramebuffer    framebuffer;

    VkDescriptorSetLayout setLayout;
    VkPipelineLayout pipelineLayout;
    VkPipelineCache  pipelineCache;
    VkShaderModule   vertModule, fragModule;
    vkPipelineEntry_t pipelines[MAX_PIPELINES];
    int              numPipelines;
    vkSamplerEntry_t samplers[MAX_SAMPLERS];
    int              numSamplers;

    VkCommandPool    commandPool;
    VkCommandBuffer  cmd;
    VkFence          fence;
    qboolean         cmdActive, passActive;
    VkPipeline       boundPipeline;

    vkRing_t         vertexRing, indexRing, uniformRing, stagingRing;

    vkTexture_t     *deferred[MAX_DEFERRED];
    int              numDeferred;
    VkImageView      deferredViews[MAX_DEFERRED];
    int              numDeferredViews;
    vkTexture_t     *white;
} vk;

void VK_Check(VkResult result, const char *what)
{
    if (result != VK_SUCCESS) {
        ri.Error(ERR_FATAL, "Vulkan: %s failed (%d)\n", what, (int)result);
    }
}

static uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags flags)
{
    uint32_t i;

    for (i = 0; i < vk.memProps.memoryTypeCount; i++) {
        if ((typeBits & (1u << i)) && (vk.memProps.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    ri.Error(ERR_FATAL, "Vulkan: no memory type with flags 0x%x\n", flags);
    return 0;
}

static void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer *buffer, VkDeviceMemory *memory, byte **mapped)
{
    VkBufferCreateInfo   info  = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    VkMemoryRequirements req;

    info.size        = size;
    info.usage       = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(qvkCreateBuffer(vk.device, &info, NULL, buffer));
    qvkGetBufferMemoryRequirements(vk.device, *buffer, &req);
    alloc.allocationSize  = req.size;
    alloc.memoryTypeIndex = FindMemoryType(
        req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    VK_CHECK(qvkAllocateMemory(vk.device, &alloc, NULL, memory));
    VK_CHECK(qvkBindBufferMemory(vk.device, *buffer, *memory, 0));
    VK_CHECK(qvkMapMemory(vk.device, *memory, 0, VK_WHOLE_SIZE, 0, (void **)mapped));
}

static void CreateRing(vkRing_t *ring, VkDeviceSize size, VkBufferUsageFlags usage)
{
    ring->size = size;
    ring->used = 0;
    CreateBuffer(size, usage, &ring->buffer, &ring->memory, &ring->mapped);
}

static void DestroyRing(vkRing_t *ring)
{
    if (ring->buffer) {
        qvkDestroyBuffer(vk.device, ring->buffer, NULL);
        qvkFreeMemory(vk.device, ring->memory, NULL);
    }
    Com_Memset(ring, 0, sizeof(*ring));
}

static void CreateImage(
    int width, int height, int levels, VkFormat format, VkImageUsageFlags usage, VkImage *image, VkDeviceMemory *memory
)
{
    VkImageCreateInfo    info  = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    VkMemoryRequirements req;

    info.imageType     = VK_IMAGE_TYPE_2D;
    info.format        = format;
    info.extent.width  = width;
    info.extent.height = height;
    info.extent.depth  = 1;
    info.mipLevels     = levels;
    info.arrayLayers   = 1;
    info.samples       = VK_SAMPLE_COUNT_1_BIT;
    info.tiling        = VK_IMAGE_TILING_OPTIMAL;
    info.usage         = usage;
    info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VK_CHECK(qvkCreateImage(vk.device, &info, NULL, image));
    qvkGetImageMemoryRequirements(vk.device, *image, &req);
    alloc.allocationSize  = req.size;
    alloc.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(qvkAllocateMemory(vk.device, &alloc, NULL, memory));
    VK_CHECK(qvkBindImageMemory(vk.device, *image, *memory, 0));
}

static VkImageView CreateView(VkImage image, VkFormat format, VkImageAspectFlags aspect, int levels)
{
    VkImageViewCreateInfo info = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    VkImageView           view;

    info.image                       = image;
    info.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
    info.format                      = format;
    info.subresourceRange.aspectMask = aspect;
    info.subresourceRange.levelCount = levels;
    info.subresourceRange.layerCount = 1;
    VK_CHECK(qvkCreateImageView(vk.device, &info, NULL, &view));
    return view;
}

static void Barrier(
    VkImage            image,
    VkImageAspectFlags aspect,
    VkImageLayout      oldLayout,
    VkImageLayout      newLayout,
    VkAccessFlags      srcAccess,
    VkAccessFlags      dstAccess,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage
)
{
    VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};

    b.srcAccessMask               = srcAccess;
    b.dstAccessMask               = dstAccess;
    b.oldLayout                   = oldLayout;
    b.newLayout                   = newLayout;
    b.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    b.image                       = image;
    b.subresourceRange.aspectMask = aspect;
    b.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    b.subresourceRange.layerCount = 1;
    qvkCmdPipelineBarrier(vk.cmd, srcStage, dstStage, 0, 0, NULL, 0, NULL, 1, &b);
}

/*
==============================================================================

Command recording

==============================================================================
*/

static void BeginCommands(void)
{
    VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};

    if (vk.cmdActive) {
        return;
    }
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(qvkResetCommandBuffer(vk.cmd, 0));
    VK_CHECK(qvkBeginCommandBuffer(vk.cmd, &begin));
    vk.cmdActive     = qtrue;
    vk.boundPipeline = VK_NULL_HANDLE;
}

static void EndPass(void)
{
    if (vk.passActive) {
        qvkCmdEndRenderPass(vk.cmd);
        vk.passActive = qfalse;
    }
}

static void BeginPass(void)
{
    VkRenderPassBeginInfo info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    VkDeviceSize          zero = 0;

    BeginCommands();
    if (vk.passActive) {
        return;
    }
    info.renderPass               = vk.renderPass;
    info.framebuffer              = vk.framebuffer;
    info.renderArea.extent.width  = vk.width;
    info.renderArea.extent.height = vk.height;
    qvkCmdBeginRenderPass(vk.cmd, &info, VK_SUBPASS_CONTENTS_INLINE);
    qvkCmdBindVertexBuffers(vk.cmd, 0, 1, &vk.vertexRing.buffer, &zero);
    qvkCmdBindIndexBuffer(vk.cmd, vk.indexRing.buffer, 0, VK_INDEX_TYPE_UINT32);
    vk.passActive    = qtrue;
    vk.boundPipeline = VK_NULL_HANDLE;
}

static void UpdateView(vkTexture_t *tex);

static void DestroyTextureNow(vkTexture_t *tex)
{
    if (tex->view) {
        qvkDestroyImageView(vk.device, tex->view, NULL);
    }
    qvkDestroyImage(vk.device, tex->image, NULL);
    qvkFreeMemory(vk.device, tex->memory, NULL);
    ri.Free(tex);
}

// Submit what was recorded and wait for it; the rings can be reused afterwards
static void Submit(qboolean present, uint32_t imageIndex)
{
    VkSubmitInfo         submit    = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    int                  i;

    if (!vk.cmdActive) {
        return;
    }
    EndPass();
    VK_CHECK(qvkEndCommandBuffer(vk.cmd));

    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &vk.cmd;
    if (present) {
        submit.waitSemaphoreCount   = 1;
        submit.pWaitSemaphores      = &vk.acquireSemaphore;
        submit.pWaitDstStageMask    = &waitStage;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores    = &vk.renderSemaphore;
    }
    VK_CHECK(qvkResetFences(vk.device, 1, &vk.fence));
    VK_CHECK(qvkQueueSubmit(vk.queue, 1, &submit, vk.fence));

    if (present) {
        VkPresentInfoKHR info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        VkResult         result;

        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores    = &vk.renderSemaphore;
        info.swapchainCount     = 1;
        info.pSwapchains        = &vk.swapchain;
        info.pImageIndices      = &imageIndex;
        result                  = qvkQueuePresentKHR(vk.queue, &info);
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR) {
            VK_Check(result, "vkQueuePresentKHR");
        }
    }

    VK_CHECK(qvkWaitForFences(vk.device, 1, &vk.fence, VK_TRUE, UINT64_MAX));
    vk.cmdActive          = qfalse;
    vk.vertexRing.used    = 0;
    vk.indexRing.used     = 0;
    vk.uniformRing.used   = 0;
    vk.stagingRing.used   = 0;
    for (i = 0; i < vk.numDeferredViews; i++) {
        qvkDestroyImageView(vk.device, vk.deferredViews[i], NULL);
    }
    vk.numDeferredViews = 0;
    for (i = 0; i < vk.numDeferred; i++) {
        DestroyTextureNow(vk.deferred[i]);
    }
    vk.numDeferred = 0;
}

static VkDeviceSize RingAlloc(vkRing_t *ring, VkDeviceSize size, VkDeviceSize alignment)
{
    VkDeviceSize offset = (ring->used + alignment - 1) & ~(alignment - 1);

    if (size > ring->size) {
        ri.Error(ERR_FATAL, "Vulkan: %u bytes do not fit a %u byte buffer\n", (unsigned)size, (unsigned)ring->size);
    }
    if (offset + size > ring->size) {
        // the buffer is full for this frame: finish the work so far and start over
        qboolean pass = vk.passActive;

        Submit(qfalse, 0);
        if (pass) {
            BeginPass();
        } else {
            BeginCommands();
        }
        offset = 0;
    }
    ring->used = offset + size;
    return offset;
}

/*
==============================================================================

Swapchain

==============================================================================
*/

static void DestroySwapchain(void)
{
    if (vk.swapchain) {
        qvkDestroySwapchainKHR(vk.device, vk.swapchain, NULL);
        vk.swapchain = VK_NULL_HANDLE;
    }
}

static void CreateSwapchain(void)
{
    VkSurfaceCapabilitiesKHR caps;
    VkSurfaceFormatKHR       formats[64];
    VkPresentModeKHR         modes[16];
    uint32_t                 numFormats = ARRAY_LEN(formats), numModes = ARRAY_LEN(modes), i;
    VkSwapchainCreateInfoKHR info       = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    VkPresentModeKHR         mode       = VK_PRESENT_MODE_FIFO_KHR;
    cvar_t                  *swapInterval = ri.Cvar_Get("r_swapInterval", "0", CVAR_ARCHIVE | CVAR_LATCH);

    VK_CHECK(qvkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.physicalDevice, vk.surface, &caps));
    qvkGetPhysicalDeviceSurfaceFormatsKHR(vk.physicalDevice, vk.surface, &numFormats, formats);
    qvkGetPhysicalDeviceSurfacePresentModesKHR(vk.physicalDevice, vk.surface, &numModes, modes);

    vk.swapchainFormat = formats[0].format;
    for (i = 0; i < numFormats; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM || formats[i].format == VK_FORMAT_R8G8B8A8_UNORM) {
            vk.swapchainFormat = formats[i].format;
            info.imageColorSpace = formats[i].colorSpace;
            break;
        }
    }
    if (i == numFormats) {
        info.imageColorSpace = formats[0].colorSpace;
    }
    if (!swapInterval->integer) {
        for (i = 0; i < numModes; i++) {
            if (modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR || modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                mode = modes[i];
                if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                    break;
                }
            }
        }
    }

    vk.swapchainExtent = caps.currentExtent;
    if (vk.swapchainExtent.width == 0xFFFFFFFF) {
        vk.swapchainExtent.width  = vk.width;
        vk.swapchainExtent.height = vk.height;
    }

    info.surface          = vk.surface;
    info.minImageCount    = Q_max(2, caps.minImageCount);
    if (caps.maxImageCount && info.minImageCount > caps.maxImageCount) {
        info.minImageCount = caps.maxImageCount;
    }
    info.imageFormat      = vk.swapchainFormat;
    info.imageExtent      = vk.swapchainExtent;
    info.imageArrayLayers = 1;
    info.imageUsage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.preTransform     = caps.currentTransform;
    info.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode      = mode;
    info.clipped          = VK_TRUE;
    VK_CHECK(qvkCreateSwapchainKHR(vk.device, &info, NULL, &vk.swapchain));

    vk.numSwapchainImages = ARRAY_LEN(vk.swapchainImages);
    VK_CHECK(qvkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.numSwapchainImages, vk.swapchainImages));
}

/*
==============================================================================

Initialization

==============================================================================
*/

static qboolean LoadLoader(void)
{
    static const char *names[] = {
#if defined(_WIN32)
        "vulkan-1.dll",
#elif defined(__APPLE__)
        "libvulkan.1.dylib", "libMoltenVK.dylib",
#else
        "libvulkan.so.1", "libvulkan.so",
#endif
    };
    int i;

    if (!vk.headless && SDL_Vulkan_LoadLibrary(NULL) == 0) {
        qvkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr();
        return qvkGetInstanceProcAddr != NULL;
    }
    for (i = 0; i < ARRAY_LEN(names) && !vk.sdlLib; i++) {
        vk.sdlLib = SDL_LoadObject(names[i]);
    }
    if (!vk.sdlLib) {
        ri.Printf(PRINT_ALL, "Vulkan: no Vulkan loader found\n");
        return qfalse;
    }
    qvkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)SDL_LoadFunction(vk.sdlLib, "vkGetInstanceProcAddr");
    return qvkGetInstanceProcAddr != NULL;
}

static qboolean HasExtension(const VkExtensionProperties *list, uint32_t count, const char *name)
{
    uint32_t i;

    for (i = 0; i < count; i++) {
        if (!strcmp(list[i].extensionName, name)) {
            return qtrue;
        }
    }
    return qfalse;
}

static qboolean CreateInstance(void)
{
    VkApplicationInfo     app  = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    VkInstanceCreateInfo  info = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    const char           *exts[16];
    unsigned int          numExts = 0;
    VkExtensionProperties available[256];
    uint32_t              numAvailable = ARRAY_LEN(available);

#define VKF(name) q##name = (PFN_##name)qvkGetInstanceProcAddr(NULL, #name);
    VK_GLOBAL_FUNCS
#undef VKF

    if (!vk.headless) {
        unsigned int count = ARRAY_LEN(exts) - 2;
        if (!SDL_Vulkan_GetInstanceExtensions(vk.window, &count, exts)) {
            ri.Printf(PRINT_ALL, "Vulkan: SDL_Vulkan_GetInstanceExtensions failed: %s\n", SDL_GetError());
            return qfalse;
        }
        numExts = count;
    }
    qvkEnumerateInstanceExtensionProperties(NULL, &numAvailable, available);
    if (HasExtension(available, numAvailable, "VK_KHR_portability_enumeration")) {
        // MoltenVK
        exts[numExts++] = "VK_KHR_portability_enumeration";
        info.flags |= 0x00000001; // VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR
    }

    app.pApplicationName    = "OpenMOHAA Plus";
    app.pEngineName         = "OpenMOHAA Plus";
    app.apiVersion          = VK_API_VERSION_1_1;
    info.pApplicationInfo        = &app;
    info.enabledExtensionCount   = numExts;
    info.ppEnabledExtensionNames = exts;
    if (qvkCreateInstance(&info, NULL, &vk.instance) != VK_SUCCESS) {
        ri.Printf(PRINT_ALL, "Vulkan: vkCreateInstance failed\n");
        return qfalse;
    }

#define VKF(name) q##name = (PFN_##name)qvkGetInstanceProcAddr(vk.instance, #name);
    VK_INSTANCE_FUNCS
#undef VKF
    return qtrue;
}

static qboolean PickDevice(void)
{
    VkPhysicalDevice devices[16];
    uint32_t         numDevices = ARRAY_LEN(devices), i, j;
    int              bestScore  = -1;

    if (qvkEnumeratePhysicalDevices(vk.instance, &numDevices, devices) != VK_SUCCESS || !numDevices) {
        ri.Printf(PRINT_ALL, "Vulkan: no device\n");
        return qfalse;
    }
    for (i = 0; i < numDevices; i++) {
        VkPhysicalDeviceProperties props;
        VkQueueFamilyProperties    families[16];
        VkExtensionProperties      exts[512];
        uint32_t                   numFamilies = ARRAY_LEN(families), numExts = ARRAY_LEN(exts);
        int                        score;

        qvkGetPhysicalDeviceProperties(devices[i], &props);
        qvkEnumerateDeviceExtensionProperties(devices[i], NULL, &numExts, exts);
        if (!HasExtension(exts, numExts, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)) {
            continue;
        }
        if (!vk.headless && !HasExtension(exts, numExts, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
            continue;
        }
        switch (props.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            score = 4;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            score = 3;
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            score = 2;
            break;
        default:
            score = 1;
            break;
        }
        qvkGetPhysicalDeviceQueueFamilyProperties(devices[i], &numFamilies, families);
        for (j = 0; j < numFamilies; j++) {
            VkBool32 present = VK_TRUE;
            if (!(families[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
                continue;
            }
            if (!vk.headless) {
                qvkGetPhysicalDeviceSurfaceSupportKHR(devices[i], j, vk.surface, &present);
            }
            if (present && score > bestScore) {
                bestScore         = score;
                vk.physicalDevice = devices[i];
                vk.queueFamily    = j;
            }
        }
    }
    if (bestScore < 0) {
        ri.Printf(PRINT_ALL, "Vulkan: no device with graphics, presentation and VK_KHR_push_descriptor\n");
        return qfalse;
    }
    qvkGetPhysicalDeviceProperties(vk.physicalDevice, &vk.props);
    qvkGetPhysicalDeviceFeatures(vk.physicalDevice, &vk.features);
    qvkGetPhysicalDeviceMemoryProperties(vk.physicalDevice, &vk.memProps);
    ri.Printf(PRINT_ALL, "Vulkan device: %s\n", vk.props.deviceName);
    return qtrue;
}

static qboolean CreateDevice(void)
{
    float                    priority = 1.0f;
    VkDeviceQueueCreateInfo  queue    = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    VkDeviceCreateInfo       info     = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    VkPhysicalDeviceFeatures features = {0};
    const char              *exts[4];
    uint32_t                 numExts = 0;
    VkExtensionProperties    available[512];
    uint32_t                 numAvailable = ARRAY_LEN(available);

    qvkEnumerateDeviceExtensionProperties(vk.physicalDevice, NULL, &numAvailable, available);
    exts[numExts++] = VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME;
    if (!vk.headless) {
        exts[numExts++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    }
    if (HasExtension(available, numAvailable, "VK_KHR_portability_subset")) {
        exts[numExts++] = "VK_KHR_portability_subset";
    }

    features.shaderClipDistance = vk.features.shaderClipDistance;
    features.fillModeNonSolid   = vk.features.fillModeNonSolid;
    features.wideLines          = vk.features.wideLines;
    features.samplerAnisotropy  = vk.features.samplerAnisotropy;
    vk.features                 = features;

    queue.queueFamilyIndex       = vk.queueFamily;
    queue.queueCount             = 1;
    queue.pQueuePriorities       = &priority;
    info.queueCreateInfoCount    = 1;
    info.pQueueCreateInfos       = &queue;
    info.enabledExtensionCount   = numExts;
    info.ppEnabledExtensionNames = exts;
    info.pEnabledFeatures        = &features;
    if (qvkCreateDevice(vk.physicalDevice, &info, NULL, &vk.device) != VK_SUCCESS) {
        ri.Printf(PRINT_ALL, "Vulkan: vkCreateDevice failed\n");
        return qfalse;
    }

#define VKF(name) q##name = (PFN_##name)qvkGetDeviceProcAddr(vk.device, #name);
    VK_DEVICE_FUNCS
#undef VKF
    qvkGetDeviceQueue(vk.device, vk.queueFamily, 0, &vk.queue);
    return qtrue;
}

static void CreateTargets(void)
{
    VkFormat                candidates[] = {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT};
    VkAttachmentDescription att[2]       = {{0}};
    VkAttachmentReference   colorRef     = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference   depthRef     = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription    subpass      = {0};
    VkRenderPassCreateInfo  rp           = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    VkFramebufferCreateInfo fb           = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    VkImageView             views[2];
    int                     i;

    vk.depthFormat = VK_FORMAT_UNDEFINED;
    for (i = 0; i < ARRAY_LEN(candidates); i++) {
        VkFormatProperties props;
        qvkGetPhysicalDeviceFormatProperties(vk.physicalDevice, candidates[i], &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            vk.depthFormat = candidates[i];
            break;
        }
    }
    if (vk.depthFormat == VK_FORMAT_UNDEFINED) {
        ri.Error(ERR_FATAL, "Vulkan: no depth-stencil format\n");
    }

    CreateImage(
        vk.width,
        vk.height,
        1,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        &vk.colorImage,
        &vk.colorMemory
    );
    CreateImage(
        vk.width,
        vk.height,
        1,
        vk.depthFormat,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        &vk.depthImage,
        &vk.depthMemory
    );
    vk.colorView = CreateView(vk.colorImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    vk.depthView =
        CreateView(vk.depthImage, vk.depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 1);

    // OpenGL keeps the framebuffer between frames and clears only on glClear
    att[0].format         = VK_FORMAT_R8G8B8A8_UNORM;
    att[0].samples        = VK_SAMPLE_COUNT_1_BIT;
    att[0].loadOp         = VK_ATTACHMENT_LOAD_OP_LOAD;
    att[0].storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    att[0].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att[0].initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    att[0].finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    att[1]                = att[0];
    att[1].format         = vk.depthFormat;
    att[1].stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_LOAD;
    att[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    att[1].initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    att[1].finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;
    rp.attachmentCount              = 2;
    rp.pAttachments                 = att;
    rp.subpassCount                 = 1;
    rp.pSubpasses                   = &subpass;
    VK_CHECK(qvkCreateRenderPass(vk.device, &rp, NULL, &vk.renderPass));

    views[0]           = vk.colorView;
    views[1]           = vk.depthView;
    fb.renderPass      = vk.renderPass;
    fb.attachmentCount = 2;
    fb.pAttachments    = views;
    fb.width           = vk.width;
    fb.height          = vk.height;
    fb.layers          = 1;
    VK_CHECK(qvkCreateFramebuffer(vk.device, &fb, NULL, &vk.framebuffer));

    // first layouts, and black like a new GL window
    BeginCommands();
    Barrier(
        vk.colorImage,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    );
    Barrier(
        vk.depthImage,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        0,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
    );
    {
        VkRect2D    rect  = {{0, 0}, {vk.width, vk.height}};
        const float black[4] = {0, 0, 0, 1};
        VK_Clear(&rect, qtrue, black, qtrue, 1.0f, qtrue, 0);
    }
    Submit(qfalse, 0);
}

static void CreatePipelineObjects(void)
{
    VkDescriptorSetLayoutBinding    bindings[3] = {{0}};
    VkDescriptorSetLayoutCreateInfo set         = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    VkPipelineLayoutCreateInfo      layout      = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VkShaderModuleCreateInfo        module      = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    VkPipelineCacheCreateInfo       cache       = {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    int                             i;

    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    for (i = 1; i < 3; i++) {
        bindings[i].binding         = i;
        bindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    set.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    set.bindingCount = 3;
    set.pBindings    = bindings;
    VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &set, NULL, &vk.setLayout));

    layout.setLayoutCount = 1;
    layout.pSetLayouts    = &vk.setLayout;
    VK_CHECK(qvkCreatePipelineLayout(vk.device, &layout, NULL, &vk.pipelineLayout));
    VK_CHECK(qvkCreatePipelineCache(vk.device, &cache, NULL, &vk.pipelineCache));

    module.codeSize = sizeof(vk_ffp_vert);
    module.pCode    = vk_ffp_vert;
    VK_CHECK(qvkCreateShaderModule(vk.device, &module, NULL, &vk.vertModule));
    module.codeSize = sizeof(vk_ffp_frag);
    module.pCode    = vk_ffp_frag;
    VK_CHECK(qvkCreateShaderModule(vk.device, &module, NULL, &vk.fragModule));
}

qboolean VK_Init(void *sdlWindow, int width, int height, qboolean headless)
{
    VkCommandPoolCreateInfo     pool  = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    VkCommandBufferAllocateInfo alloc = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    VkFenceCreateInfo           fence = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkSemaphoreCreateInfo       sem   = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    static const byte           white[4] = {255, 255, 255, 255};

    Com_Memset(&vk, 0, sizeof(vk));
    vk.window   = sdlWindow;
    vk.headless = headless;
    vk.width    = width;
    vk.height   = height;

    if (!LoadLoader() || !CreateInstance()) {
        return qfalse;
    }
    if (!vk.headless && !SDL_Vulkan_CreateSurface(vk.window, vk.instance, &vk.surface)) {
        ri.Printf(PRINT_ALL, "Vulkan: SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
        return qfalse;
    }
    if (!PickDevice() || !CreateDevice()) {
        return qfalse;
    }

    pool.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = vk.queueFamily;
    VK_CHECK(qvkCreateCommandPool(vk.device, &pool, NULL, &vk.commandPool));
    alloc.commandPool        = vk.commandPool;
    alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = 1;
    VK_CHECK(qvkAllocateCommandBuffers(vk.device, &alloc, &vk.cmd));
    VK_CHECK(qvkCreateFence(vk.device, &fence, NULL, &vk.fence));
    VK_CHECK(qvkCreateSemaphore(vk.device, &sem, NULL, &vk.acquireSemaphore));
    VK_CHECK(qvkCreateSemaphore(vk.device, &sem, NULL, &vk.renderSemaphore));

    CreateRing(&vk.vertexRing, VERTEX_RING_SIZE, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    CreateRing(&vk.indexRing, INDEX_RING_SIZE, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    CreateRing(&vk.uniformRing, UNIFORM_RING_SIZE, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    CreateRing(&vk.stagingRing, STAGING_RING_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    CreatePipelineObjects();
    CreateTargets();
    if (!vk.headless) {
        CreateSwapchain();
    }

    vk.white = VK_CreateTexture(1, 1, 1);
    VK_UploadTexture(vk.white, 0, 0, 0, 1, 1, white);
    UpdateView(vk.white);
    return qtrue;
}

void VK_Shutdown(void)
{
    int i;

    if (!vk.device) {
        if (vk.instance) {
            qvkDestroyInstance(vk.instance, NULL);
        }
        Com_Memset(&vk, 0, sizeof(vk));
        return;
    }
    Submit(qfalse, 0);
    qvkDeviceWaitIdle(vk.device);

    VK_GL_Shutdown(); // destroys the GL textures
    if (vk.white) {
        DestroyTextureNow(vk.white);
    }
    for (i = 0; i < vk.numDeferredViews; i++) {
        qvkDestroyImageView(vk.device, vk.deferredViews[i], NULL);
    }
    for (i = 0; i < vk.numPipelines; i++) {
        qvkDestroyPipeline(vk.device, vk.pipelines[i].pipeline, NULL);
    }
    for (i = 0; i < vk.numSamplers; i++) {
        qvkDestroySampler(vk.device, vk.samplers[i].sampler, NULL);
    }
    qvkDestroyShaderModule(vk.device, vk.vertModule, NULL);
    qvkDestroyShaderModule(vk.device, vk.fragModule, NULL);
    qvkDestroyPipelineCache(vk.device, vk.pipelineCache, NULL);
    qvkDestroyPipelineLayout(vk.device, vk.pipelineLayout, NULL);
    qvkDestroyDescriptorSetLayout(vk.device, vk.setLayout, NULL);
    qvkDestroyFramebuffer(vk.device, vk.framebuffer, NULL);
    qvkDestroyRenderPass(vk.device, vk.renderPass, NULL);
    qvkDestroyImageView(vk.device, vk.colorView, NULL);
    qvkDestroyImageView(vk.device, vk.depthView, NULL);
    qvkDestroyImage(vk.device, vk.colorImage, NULL);
    qvkDestroyImage(vk.device, vk.depthImage, NULL);
    qvkFreeMemory(vk.device, vk.colorMemory, NULL);
    qvkFreeMemory(vk.device, vk.depthMemory, NULL);
    DestroyRing(&vk.vertexRing);
    DestroyRing(&vk.indexRing);
    DestroyRing(&vk.uniformRing);
    DestroyRing(&vk.stagingRing);
    DestroySwapchain();
    qvkDestroySemaphore(vk.device, vk.acquireSemaphore, NULL);
    qvkDestroySemaphore(vk.device, vk.renderSemaphore, NULL);
    qvkDestroyFence(vk.device, vk.fence, NULL);
    qvkDestroyCommandPool(vk.device, vk.commandPool, NULL);
    qvkDestroyDevice(vk.device, NULL);
    if (vk.surface) {
        qvkDestroySurfaceKHR(vk.instance, vk.surface, NULL);
    }
    qvkDestroyInstance(vk.instance, NULL);
    if (vk.sdlLib) {
        SDL_UnloadObject(vk.sdlLib);
    } else {
        SDL_Vulkan_UnloadLibrary();
    }
    Com_Memset(&vk, 0, sizeof(vk));
}

const char *VK_DeviceName(void)
{
    return vk.props.deviceName;
}

int VK_MaxTextureSize(void)
{
    return Q_min(vk.props.limits.maxImageDimension2D, 8192);
}

float VK_MaxAnisotropy(void)
{
    return vk.features.samplerAnisotropy ? vk.props.limits.maxSamplerAnisotropy : 0;
}

int VK_FramebufferWidth(void)
{
    return vk.width;
}

int VK_FramebufferHeight(void)
{
    return vk.height;
}

/*
==============================================================================

Frames

==============================================================================
*/

void VK_Finish(void)
{
    Submit(qfalse, 0);
}

void VK_EndFrame(void)
{
    uint32_t    imageIndex;
    VkResult    result;
    VkImageBlit blit = {{0}};

    if (vk.headless) {
        Submit(qfalse, 0);
        return;
    }

    result = qvkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX, vk.acquireSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // window resized: try again next frame
        Submit(qfalse, 0);
        qvkDeviceWaitIdle(vk.device);
        DestroySwapchain();
        CreateSwapchain();
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        VK_Check(result, "vkAcquireNextImageKHR");
    }

    BeginCommands();
    EndPass();
    Barrier(
        vk.colorImage,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT
    );
    Barrier(
        vk.swapchainImages[imageIndex],
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT
    );
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[1].x           = vk.width;
    blit.srcOffsets[1].y           = vk.height;
    blit.srcOffsets[1].z           = 1;
    blit.dstSubresource            = blit.srcSubresource;
    blit.dstOffsets[1].x           = vk.swapchainExtent.width;
    blit.dstOffsets[1].y           = vk.swapchainExtent.height;
    blit.dstOffsets[1].z           = 1;
    qvkCmdBlitImage(
        vk.cmd,
        vk.colorImage,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        vk.swapchainImages[imageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &blit,
        VK_FILTER_LINEAR
    );
    Barrier(
        vk.swapchainImages[imageIndex],
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        0,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
    );
    Barrier(
        vk.colorImage,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    );
    Submit(qtrue, imageIndex);
}

/*
==============================================================================

Textures

==============================================================================
*/

vkTexture_t *VK_CreateTexture(int width, int height, int levels)
{
    vkTexture_t *tex = ri.Malloc(sizeof(*tex));

    Com_Memset(tex, 0, sizeof(*tex));
    tex->width  = width;
    tex->height = height;
    tex->levels = levels;
    tex->layout = VK_IMAGE_LAYOUT_UNDEFINED;
    CreateImage(
        width,
        height,
        levels,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        &tex->image,
        &tex->memory
    );
    return tex;
}

void VK_DestroyTexture(vkTexture_t *tex)
{
    if (!tex) {
        return;
    }
    if (vk.cmdActive) {
        // may still be used by recorded commands
        if (vk.numDeferred == MAX_DEFERRED) {
            Submit(qfalse, 0);
        } else {
            vk.deferred[vk.numDeferred++] = tex;
            return;
        }
    }
    DestroyTextureNow(tex);
}

void VK_UploadTexture(vkTexture_t *tex, int level, int x, int y, int width, int height, const byte *rgba)
{
    VkDeviceSize      size = (VkDeviceSize)width * height * 4;
    VkDeviceSize      offset;
    VkBufferImageCopy copy = {0};
    qboolean          pass;
    if (level >= tex->levels || width <= 0 || height <= 0) {
        return;
    }

    pass   = vk.passActive;
    offset = RingAlloc(&vk.stagingRing, size, 16);
    Com_Memcpy(vk.stagingRing.mapped + offset, rgba, size);

    // copies happen outside the render pass, in order with the draws around them, like in GL
    BeginCommands();
    EndPass();
    Barrier(
        tex->image,
        VK_IMAGE_ASPECT_COLOR_BIT,
        tex->layout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        tex->layout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_SHADER_READ_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        tex->layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT
    );
    copy.bufferOffset                = offset;
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.mipLevel   = level;
    copy.imageSubresource.layerCount = 1;
    copy.imageOffset.x               = x;
    copy.imageOffset.y               = y;
    copy.imageExtent.width           = width;
    copy.imageExtent.height          = height;
    copy.imageExtent.depth           = 1;
    qvkCmdCopyBufferToImage(vk.cmd, vk.stagingRing.buffer, tex->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    Barrier(
        tex->image,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    );
    tex->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (pass) {
        BeginPass();
    }

    tex->definedMask |= 1u << level;
}

// The view covers the levels uploaded so far (from 0); refreshed when the texture is drawn
static void UpdateView(vkTexture_t *tex)
{
    int levels;

    for (levels = 0; levels < tex->levels && (tex->definedMask & (1u << levels)); levels++) {}
    if (!levels || levels == tex->viewLevels) {
        return;
    }
    if (tex->view) {
        // the old view may be in use by recorded draws
        if (vk.numDeferredViews == MAX_DEFERRED) {
            qboolean pass = vk.passActive;
            Submit(qfalse, 0);
            if (pass) {
                BeginPass();
            }
        }
        vk.deferredViews[vk.numDeferredViews++] = tex->view;
    }
    tex->view       = CreateView(tex->image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, levels);
    tex->viewLevels = levels;
}

VkSampler VK_GetSampler(
    VkFilter             minFilter,
    VkFilter             magFilter,
    VkSamplerMipmapMode  mipmapMode,
    qboolean             mipmaps,
    VkSamplerAddressMode wrapS,
    VkSamplerAddressMode wrapT,
    float                anisotropy
)
{
    VkSamplerCreateInfo info = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    vkSamplerEntry_t   *e;
    int                 i;

    if (!vk.features.samplerAnisotropy) {
        anisotropy = 0;
    }
    for (i = 0; i < vk.numSamplers; i++) {
        e = &vk.samplers[i];
        if (e->minFilter == minFilter && e->magFilter == magFilter && e->mipmapMode == mipmapMode
            && e->mipmaps == mipmaps && e->wrapS == wrapS && e->wrapT == wrapT && e->anisotropy == anisotropy) {
            return e->sampler;
        }
    }
    if (vk.numSamplers == MAX_SAMPLERS) {
        return vk.samplers[0].sampler;
    }

    info.magFilter        = magFilter;
    info.minFilter        = minFilter;
    info.mipmapMode       = mipmapMode;
    info.addressModeU     = wrapS;
    info.addressModeV     = wrapT;
    info.addressModeW     = wrapS;
    info.anisotropyEnable = anisotropy > 1.0f;
    info.maxAnisotropy    = anisotropy > 1.0f ? anisotropy : 1.0f;
    info.maxLod           = mipmaps ? VK_LOD_CLAMP_NONE : 0.0f;

    e             = &vk.samplers[vk.numSamplers++];
    e->minFilter  = minFilter;
    e->magFilter  = magFilter;
    e->mipmapMode = mipmapMode;
    e->mipmaps    = mipmaps;
    e->wrapS      = wrapS;
    e->wrapT      = wrapT;
    e->anisotropy = anisotropy;
    VK_CHECK(qvkCreateSampler(vk.device, &info, NULL, &e->sampler));
    return e->sampler;
}

/*
==============================================================================

Pipelines and draws

==============================================================================
*/

static VkBlendFactor BlendFactor(uint32_t gl)
{
    switch (gl) {
    case 0:
        return VK_BLEND_FACTOR_ZERO;
    case 1:
        return VK_BLEND_FACTOR_ONE;
    case 0x0300:
        return VK_BLEND_FACTOR_SRC_COLOR;
    case 0x0301:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case 0x0302:
        return VK_BLEND_FACTOR_SRC_ALPHA;
    case 0x0303:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case 0x0304:
        return VK_BLEND_FACTOR_DST_ALPHA;
    case 0x0305:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    case 0x0306:
        return VK_BLEND_FACTOR_DST_COLOR;
    case 0x0307:
        return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case 0x0308:
        return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
    }
    return VK_BLEND_FACTOR_ONE;
}

static VkCompareOp CompareOp(uint32_t gl)
{
    // GL_NEVER .. GL_ALWAYS are 0x0200 .. 0x0207 in the same order as VkCompareOp
    return gl >= 0x0200 && gl <= 0x0207 ? (VkCompareOp)(gl - 0x0200) : VK_COMPARE_OP_ALWAYS;
}

static VkStencilOp StencilOp(uint32_t gl)
{
    switch (gl) {
    case 0:
        return VK_STENCIL_OP_ZERO;
    case 0x1E01:
        return VK_STENCIL_OP_REPLACE;
    case 0x1E02:
        return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
    case 0x1E03:
        return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
    case 0x150A:
        return VK_STENCIL_OP_INVERT;
    case 0x8507:
        return VK_STENCIL_OP_INCREMENT_AND_WRAP;
    case 0x8508:
        return VK_STENCIL_OP_DECREMENT_AND_WRAP;
    }
    return VK_STENCIL_OP_KEEP; // GL_KEEP
}

static VkPipeline CreatePipeline(const vkPipelineKey_t *key)
{
    VkPipelineShaderStageCreateInfo        stages[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
                                                        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
    VkVertexInputBindingDescription        binding   = {0, sizeof(vkVertex_t), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription      attrs[4]  = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vkVertex_t, xyz)},
        {1, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(vkVertex_t, color)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vkVertex_t, st0)},
        {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(vkVertex_t, st1)},
    };
    VkPipelineVertexInputStateCreateInfo   vertex   = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo assembly = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    VkPipelineViewportStateCreateInfo      viewport = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    VkPipelineRasterizationStateCreateInfo raster   = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    VkPipelineMultisampleStateCreateInfo   msaa     = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    VkPipelineDepthStencilStateCreateInfo  depth    = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkPipelineColorBlendAttachmentState    blendAtt = {0};
    VkPipelineColorBlendStateCreateInfo    blend    = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    VkDynamicState                         dynamic[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_BIAS,
        VK_DYNAMIC_STATE_LINE_WIDTH,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE,
        VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
        VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
    };
    VkPipelineDynamicStateCreateInfo dyn  = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    VkGraphicsPipelineCreateInfo     info = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    VkStencilOpState                 stencil;
    VkPipeline                       pipeline;

    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vk.vertModule;
    stages[0].pName  = "main";
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = vk.fragModule;
    stages[1].pName  = "main";

    vertex.vertexBindingDescriptionCount   = 1;
    vertex.pVertexBindingDescriptions      = &binding;
    vertex.vertexAttributeDescriptionCount = 4;
    vertex.pVertexAttributeDescriptions    = attrs;

    assembly.topology = key->topology == 1   ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST
                      : key->topology == 2 ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST
                                           : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    viewport.viewportCount = 1;
    viewport.scissorCount  = 1;

    raster.polygonMode = key->polygonLine && vk.features.fillModeNonSolid ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
    raster.cullMode    = key->cullMode == 1 ? VK_CULL_MODE_FRONT_BIT : key->cullMode == 2 ? VK_CULL_MODE_BACK_BIT : 0;
    // GL's counter-clockwise front faces: Vulkan's framebuffer y points down, and the vertex
    // shader flips y back, so the winding Vulkan sees is the one GL sees
    raster.frontFace       = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.depthBiasEnable = key->depthBias;
    raster.lineWidth       = 1.0f;

    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    depth.depthTestEnable  = key->depthFunc != 0;
    depth.depthWriteEnable = key->depthFunc != 0 && key->depthWrite; // GL writes no depth without the test
    depth.depthCompareOp   = CompareOp(key->depthFunc);
    if (key->stencilTest) {
        stencil.failOp        = StencilOp(key->stencilFail);
        stencil.passOp        = StencilOp(key->stencilZPass);
        stencil.depthFailOp   = StencilOp(key->stencilZFail);
        stencil.compareOp     = CompareOp(key->stencilFunc);
        stencil.compareMask   = 0xFF;
        stencil.writeMask     = 0xFF;
        stencil.reference     = 0;
        depth.stencilTestEnable = VK_TRUE;
        depth.front             = stencil;
        depth.back              = stencil;
    }

    if (key->blendSrc || key->blendDst) {
        blendAtt.blendEnable         = VK_TRUE;
        blendAtt.srcColorBlendFactor = BlendFactor(key->blendSrc);
        blendAtt.dstColorBlendFactor = BlendFactor(key->blendDst);
        blendAtt.colorBlendOp        = VK_BLEND_OP_ADD;
        blendAtt.srcAlphaBlendFactor = blendAtt.srcColorBlendFactor;
        blendAtt.dstAlphaBlendFactor = blendAtt.dstColorBlendFactor;
        blendAtt.alphaBlendOp        = VK_BLEND_OP_ADD;
    }
    blendAtt.colorWriteMask = key->colorMask;
    blend.attachmentCount   = 1;
    blend.pAttachments      = &blendAtt;

    dyn.dynamicStateCount = ARRAY_LEN(dynamic);
    dyn.pDynamicStates    = dynamic;

    info.stageCount          = 2;
    info.pStages             = stages;
    info.pVertexInputState   = &vertex;
    info.pInputAssemblyState = &assembly;
    info.pViewportState      = &viewport;
    info.pRasterizationState = &raster;
    info.pMultisampleState   = &msaa;
    info.pDepthStencilState  = &depth;
    info.pColorBlendState    = &blend;
    info.pDynamicState       = &dyn;
    info.layout              = vk.pipelineLayout;
    info.renderPass          = vk.renderPass;
    VK_CHECK(qvkCreateGraphicsPipelines(vk.device, vk.pipelineCache, 1, &info, NULL, &pipeline));
    return pipeline;
}

static VkPipeline GetPipeline(const vkPipelineKey_t *key)
{
    int i;

    for (i = 0; i < vk.numPipelines; i++) {
        if (!memcmp(&vk.pipelines[i].key, key, sizeof(*key))) {
            return vk.pipelines[i].pipeline;
        }
    }
    if (vk.numPipelines == MAX_PIPELINES) {
        ri.Error(ERR_FATAL, "Vulkan: more than %d pipelines\n", MAX_PIPELINES);
    }
    vk.pipelines[vk.numPipelines].key      = *key;
    vk.pipelines[vk.numPipelines].pipeline = CreatePipeline(key);
    return vk.pipelines[vk.numPipelines++].pipeline;
}

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
)
{
    VkDeviceSize           vOffset, iOffset, uOffset;
    VkPipeline             pipeline;
    VkViewport             vp;
    VkDescriptorBufferInfo ubo;
    VkDescriptorImageInfo  images[2];
    VkWriteDescriptorSet   writes[3] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    vkTexture_t           *textures[2];
    VkSampler              samplers[2];
    int                    i;

    if (!numVertices || !numIndices) {
        return;
    }

    pipeline = GetPipeline(key);
    // before the ring allocations: may submit when too many old views wait for deletion
    if (tex0) {
        UpdateView(tex0);
    }
    if (tex1) {
        UpdateView(tex1);
    }
    BeginPass();
    vOffset = RingAlloc(&vk.vertexRing, (VkDeviceSize)numVertices * sizeof(vkVertex_t), sizeof(vkVertex_t));
    iOffset = RingAlloc(&vk.indexRing, (VkDeviceSize)numIndices * sizeof(uint32_t), sizeof(uint32_t));
    uOffset = RingAlloc(&vk.uniformRing, sizeof(*state), vk.props.limits.minUniformBufferOffsetAlignment);
    if (vk.vertexRing.used < vOffset + numVertices * sizeof(vkVertex_t) || !vk.passActive) {
        // a ring was flushed by a later allocation: allocate everything again from the start
        BeginPass();
        vOffset = RingAlloc(&vk.vertexRing, (VkDeviceSize)numVertices * sizeof(vkVertex_t), sizeof(vkVertex_t));
        iOffset = RingAlloc(&vk.indexRing, (VkDeviceSize)numIndices * sizeof(uint32_t), sizeof(uint32_t));
        uOffset = RingAlloc(&vk.uniformRing, sizeof(*state), vk.props.limits.minUniformBufferOffsetAlignment);
    }
    Com_Memcpy(vk.vertexRing.mapped + vOffset, vertices, numVertices * sizeof(vkVertex_t));
    Com_Memcpy(vk.indexRing.mapped + iOffset, indices, numIndices * sizeof(uint32_t));
    Com_Memcpy(vk.uniformRing.mapped + uOffset, state, sizeof(*state));

    if (pipeline != vk.boundPipeline) {
        qvkCmdBindPipeline(vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vk.boundPipeline = pipeline;
    }

    vp.x        = viewport->x;
    vp.y        = viewport->y;
    vp.width    = viewport->w;
    vp.height   = viewport->h;
    vp.minDepth = viewport->minDepth;
    vp.maxDepth = viewport->maxDepth;
    qvkCmdSetViewport(vk.cmd, 0, 1, &vp);
    qvkCmdSetScissor(vk.cmd, 0, 1, scissor);
    qvkCmdSetDepthBias(vk.cmd, depthBiasConstant, 0.0f, depthBiasSlope);
    qvkCmdSetLineWidth(vk.cmd, vk.features.wideLines ? Q_max(1.0f, lineWidth) : 1.0f);
    qvkCmdSetStencilReference(vk.cmd, VK_STENCIL_FACE_FRONT_AND_BACK, stencilRef);
    qvkCmdSetStencilCompareMask(vk.cmd, VK_STENCIL_FACE_FRONT_AND_BACK, stencilMask);
    qvkCmdSetStencilWriteMask(vk.cmd, VK_STENCIL_FACE_FRONT_AND_BACK, stencilWriteMask);

    ubo.buffer = vk.uniformRing.buffer;
    ubo.offset = uOffset;
    ubo.range  = sizeof(*state);
    textures[0] = tex0;
    textures[1] = tex1;
    samplers[0] = sampler0;
    samplers[1] = sampler1;
    for (i = 0; i < 2; i++) {
        vkTexture_t *t = textures[i] && textures[i]->view ? textures[i] : vk.white;
        images[i].sampler     = samplers[i];
        images[i].imageView   = t->view;
        images[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    writes[0].dstBinding      = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo     = &ubo;
    for (i = 0; i < 2; i++) {
        writes[i + 1].dstBinding      = i + 1;
        writes[i + 1].descriptorCount = 1;
        writes[i + 1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i + 1].pImageInfo      = &images[i];
    }
    qvkCmdPushDescriptorSetKHR(vk.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.pipelineLayout, 0, 3, writes);
    qvkCmdDrawIndexed(
        vk.cmd, numIndices, 1, (uint32_t)(iOffset / sizeof(uint32_t)), (int32_t)(vOffset / sizeof(vkVertex_t)), 0
    );
}

void VK_Clear(
    const VkRect2D *rect,
    qboolean        color,
    const float     rgba[4],
    qboolean        depth,
    float           depthValue,
    qboolean        stencil,
    uint32_t        stencilValue
)
{
    VkClearAttachment att[2];
    VkClearRect       clearRect;
    uint32_t          n = 0;

    if (rect->extent.width == 0 || rect->extent.height == 0) {
        return;
    }
    BeginPass();
    if (color) {
        att[n].aspectMask      = VK_IMAGE_ASPECT_COLOR_BIT;
        att[n].colorAttachment = 0;
        Com_Memcpy(att[n].clearValue.color.float32, rgba, sizeof(float) * 4);
        n++;
    }
    if (depth || stencil) {
        att[n].aspectMask = (depth ? VK_IMAGE_ASPECT_DEPTH_BIT : 0) | (stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
        att[n].colorAttachment                 = 0;
        att[n].clearValue.depthStencil.depth   = depthValue;
        att[n].clearValue.depthStencil.stencil = stencilValue;
        n++;
    }
    if (!n) {
        return;
    }
    clearRect.rect           = *rect;
    clearRect.baseArrayLayer = 0;
    clearRect.layerCount     = 1;
    qvkCmdClearAttachments(vk.cmd, n, att, 1, &clearRect);
}

void VK_ReadPixels(int x, int y, int width, int height, byte *rgba)
{
    VkBuffer          buffer;
    VkDeviceMemory    memory;
    byte             *mapped;
    VkBufferImageCopy copy = {0};
    qboolean          pass = vk.passActive;

    x      = Com_Clamp(0, vk.width, x);
    y      = Com_Clamp(0, vk.height, y);
    width  = Q_min(width, vk.width - x);
    height = Q_min(height, vk.height - y);
    if (width <= 0 || height <= 0) {
        return;
    }

    CreateBuffer((VkDeviceSize)width * height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &buffer, &memory, &mapped);
    BeginCommands();
    EndPass();
    Barrier(
        vk.colorImage,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT
    );
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageOffset.x               = x;
    copy.imageOffset.y               = y;
    copy.imageExtent.width           = width;
    copy.imageExtent.height          = height;
    copy.imageExtent.depth           = 1;
    qvkCmdCopyImageToBuffer(vk.cmd, vk.colorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &copy);
    Barrier(
        vk.colorImage,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_ACCESS_TRANSFER_READ_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    );
    Submit(qfalse, 0);
    Com_Memcpy(rgba, mapped, (size_t)width * height * 4);
    qvkDestroyBuffer(vk.device, buffer, NULL);
    qvkFreeMemory(vk.device, memory, NULL);
    if (pass) {
        BeginPass();
    }
}
