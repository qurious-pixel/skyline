// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <adrenotools/bcenabler.h>
#include "trait_manager.h"

namespace skyline::gpu {
    TraitManager::TraitManager(const DeviceFeatures2 &deviceFeatures2, DeviceFeatures2 &enabledFeatures2, const std::vector<vk::ExtensionProperties> &deviceExtensions, std::vector<std::array<char, VK_MAX_EXTENSION_NAME_SIZE>> &enabledExtensions, const DeviceProperties2 &deviceProperties2, const vk::raii::PhysicalDevice &physicalDevice) : quirks(deviceProperties2.get<vk::PhysicalDeviceProperties2>().properties, deviceProperties2.get<vk::PhysicalDeviceDriverProperties>()) {
        bool hasCustomBorderColorExt{}, hasShaderAtomicInt64Ext{}, hasShaderFloat16Int8Ext{}, hasShaderDemoteToHelperExt{}, hasVertexAttributeDivisorExt{}, hasProvokingVertexExt{}, hasPrimitiveTopologyListRestartExt{}, hasImagelessFramebuffersExt{};
        bool supportsUniformBufferStandardLayout{}; // We require VK_KHR_uniform_buffer_standard_layout but assume it is implicitly supported even when not present

        for (auto &extension : deviceExtensions) {
            #define EXT_SET(name, property)                                                          \
            case util::Hash(name):                                                                   \
                if (name == extensionName) {                                                         \
                    property = true;                                                                 \
                    enabledExtensions.push_back(std::array<char, VK_MAX_EXTENSION_NAME_SIZE>{name}); \
                }                                                                                    \
                break

            #define EXT_SET_V(name, property, version)                                               \
            case util::Hash(name):                                                                   \
                if (name == extensionName && extensionVersion >= version) {                          \
                    property = true;                                                                 \
                    enabledExtensions.push_back(std::array<char, VK_MAX_EXTENSION_NAME_SIZE>{name}); \
                }                                                                                    \
                break

            std::string_view extensionName{extension.extensionName};
            auto extensionVersion{extension.specVersion};
            switch (util::Hash(extensionName)) {
                EXT_SET("VK_EXT_index_type_uint8", supportsUint8Indices);
                EXT_SET("VK_EXT_sampler_mirror_clamp_to_edge", supportsSamplerMirrorClampToEdge);
                EXT_SET("VK_EXT_sampler_filter_minmax", supportsSamplerReductionMode);
                EXT_SET("VK_EXT_custom_border_color", hasCustomBorderColorExt);
                EXT_SET("VK_EXT_provoking_vertex", hasProvokingVertexExt);
                EXT_SET("VK_EXT_vertex_attribute_divisor", hasVertexAttributeDivisorExt);
                EXT_SET("VK_KHR_push_descriptor", supportsPushDescriptors);
                EXT_SET("VK_KHR_imageless_framebuffer", hasImagelessFramebuffersExt);
                EXT_SET("VK_EXT_global_priority", supportsGlobalPriority);
                EXT_SET("VK_EXT_shader_viewport_index_layer", supportsShaderViewportIndexLayer);
                EXT_SET("VK_KHR_spirv_1_4", supportsSpirv14);
                EXT_SET("VK_EXT_shader_demote_to_helper_invocation", hasShaderDemoteToHelperExt);
                EXT_SET("VK_KHR_shader_atomic_int64", hasShaderAtomicInt64Ext);
                EXT_SET("VK_KHR_shader_float16_int8", hasShaderFloat16Int8Ext);
                EXT_SET("VK_KHR_shader_float_controls", supportsFloatControls);
                EXT_SET("VK_KHR_uniform_buffer_standard_layout", supportsUniformBufferStandardLayout);
                EXT_SET("VK_EXT_primitive_topology_list_restart", hasPrimitiveTopologyListRestartExt);
            }

            #undef EXT_SET
            #undef EXT_SET_V
        }

        #define FEAT_SET(structName, feature, property)            \
        do {                                                       \
            if (deviceFeatures2.get<structName>().feature) {       \
                property = true;                                   \
                enabledFeatures2.get<structName>().feature = true; \
            }                                                      \
        } while(false);

        FEAT_SET(vk::PhysicalDeviceFeatures2, features.logicOp, supportsLogicOp)
        FEAT_SET(vk::PhysicalDeviceFeatures2, features.multiViewport, supportsMultipleViewports)
        FEAT_SET(vk::PhysicalDeviceFeatures2, features.shaderInt16, supportsInt16)
        FEAT_SET(vk::PhysicalDeviceFeatures2, features.shaderInt64, supportsInt64)
        FEAT_SET(vk::PhysicalDeviceFeatures2, features.shaderStorageImageReadWithoutFormat, supportsImageReadWithoutFormat)

        if (hasCustomBorderColorExt) {
            bool hasCustomBorderColorFeature{};
            FEAT_SET(vk::PhysicalDeviceCustomBorderColorFeaturesEXT, customBorderColors, hasCustomBorderColorFeature)
            if (hasCustomBorderColorFeature)
                // We only want to mark custom border colors as supported if it can be done without supplying a format
                FEAT_SET(vk::PhysicalDeviceCustomBorderColorFeaturesEXT, customBorderColorWithoutFormat, supportsCustomBorderColor)
        } else {
            enabledFeatures2.unlink<vk::PhysicalDeviceCustomBorderColorFeaturesEXT>();
        }

        if (hasVertexAttributeDivisorExt) {
            FEAT_SET(vk::PhysicalDeviceVertexAttributeDivisorFeaturesEXT, vertexAttributeInstanceRateDivisor, supportsVertexAttributeDivisor)
            FEAT_SET(vk::PhysicalDeviceVertexAttributeDivisorFeaturesEXT, vertexAttributeInstanceRateZeroDivisor, supportsVertexAttributeZeroDivisor)
        } else {
            enabledFeatures2.unlink<vk::PhysicalDeviceVertexAttributeDivisorFeaturesEXT>();
        }

        if (hasProvokingVertexExt)
            FEAT_SET(vk::PhysicalDeviceProvokingVertexFeaturesEXT, provokingVertexLast, supportsLastProvokingVertex)
        else
            enabledFeatures2.unlink<vk::PhysicalDeviceProvokingVertexFeaturesEXT>();

        auto &shaderAtomicFeatures{deviceFeatures2.get<vk::PhysicalDeviceShaderAtomicInt64Features>()};
        if (hasShaderAtomicInt64Ext && shaderAtomicFeatures.shaderBufferInt64Atomics && shaderAtomicFeatures.shaderSharedInt64Atomics) {
            supportsAtomicInt64 = true;
        } else {
            enabledFeatures2.unlink<vk::PhysicalDeviceShaderAtomicInt64Features>();
        }

        if (hasShaderFloat16Int8Ext) {
            FEAT_SET(vk::PhysicalDeviceShaderFloat16Int8Features, shaderFloat16, supportsFloat16)
            FEAT_SET(vk::PhysicalDeviceShaderFloat16Int8Features, shaderInt8, supportsInt8)
        } else {
            enabledFeatures2.unlink<vk::PhysicalDeviceShaderFloat16Int8Features>();
        }

        if (hasShaderDemoteToHelperExt)
            FEAT_SET(vk::PhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT, shaderDemoteToHelperInvocation, supportsShaderDemoteToHelper)
        else
            enabledFeatures2.unlink<vk::PhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT>();

        if (supportsUniformBufferStandardLayout) {
            FEAT_SET(vk::PhysicalDeviceUniformBufferStandardLayoutFeatures, uniformBufferStandardLayout, supportsUniformBufferStandardLayout)
        } else {
            enabledFeatures2.unlink<vk::PhysicalDeviceUniformBufferStandardLayoutFeatures>();
            Logger::Warn("Cannot find VK_KHR_uniform_buffer_standard_layout, assuming implicit support");
        }

        if (hasPrimitiveTopologyListRestartExt) {
            FEAT_SET(vk::PhysicalDevicePrimitiveTopologyListRestartFeaturesEXT, primitiveTopologyListRestart, supportsTopologyListRestart)
            FEAT_SET(vk::PhysicalDevicePrimitiveTopologyListRestartFeaturesEXT, primitiveTopologyPatchListRestart, supportsTopologyPatchListRestart)
        } else {
            enabledFeatures2.unlink<vk::PhysicalDevicePrimitiveTopologyListRestartFeaturesEXT>();
        }

        if (hasImagelessFramebuffersExt) {
            FEAT_SET(vk::PhysicalDeviceImagelessFramebufferFeatures, imagelessFramebuffer, supportsImagelessFramebuffers)
        } else {
            enabledFeatures2.unlink<vk::PhysicalDeviceImagelessFramebufferFeatures>();
        }

        #undef FEAT_SET

        if (supportsFloatControls)
            floatControls = deviceProperties2.get<vk::PhysicalDeviceFloatControlsProperties>();

        auto &subgroupProperties{deviceProperties2.get<vk::PhysicalDeviceSubgroupProperties>()};
        supportsSubgroupVote = static_cast<bool>(subgroupProperties.supportedOperations & vk::SubgroupFeatureFlagBits::eVote);
        subgroupSize = deviceProperties2.get<vk::PhysicalDeviceSubgroupProperties>().subgroupSize;

        auto isFormatSupported{[&physicalDevice](vk::Format format) {
            auto features{physicalDevice.getFormatProperties(format)};
            // We may get false positives here by not checking specifics but this is not seen in practice while the reverse often is of drivers (Such as Adreno 512.6xx drivers which don't report any support aside from buffer features but entirely support BC formats)
            return static_cast<bool>(features.linearTilingFeatures) ||
                static_cast<bool>(features.optimalTilingFeatures) ||
                static_cast<bool>(features.bufferFeatures);
        }};

        bcnSupport[0] = isFormatSupported(vk::Format::eBc1RgbaUnormBlock) && isFormatSupported(vk::Format::eBc1RgbaSrgbBlock);
        bcnSupport[1] = isFormatSupported(vk::Format::eBc2UnormBlock) && isFormatSupported(vk::Format::eBc2SrgbBlock);
        bcnSupport[2] = isFormatSupported(vk::Format::eBc3UnormBlock) && isFormatSupported(vk::Format::eBc3SrgbBlock);
        bcnSupport[3] = isFormatSupported(vk::Format::eBc4UnormBlock) && isFormatSupported(vk::Format::eBc4SnormBlock);
        bcnSupport[4] = isFormatSupported(vk::Format::eBc5UnormBlock) && isFormatSupported(vk::Format::eBc5SnormBlock);
        bcnSupport[5] = isFormatSupported(vk::Format::eBc6HSfloatBlock) && isFormatSupported(vk::Format::eBc6HUfloatBlock);
        bcnSupport[6] = isFormatSupported(vk::Format::eBc7UnormBlock) && isFormatSupported(vk::Format::eBc7SrgbBlock);
    }

    std::string TraitManager::Summary() {
        return fmt::format(
            "\n* Supports U8 Indices: {}\n* Supports Sampler Mirror Clamp To Edge: {}\n* Supports Sampler Reduction Mode: {}\n* Supports Custom Border Color (Without Format): {}\n* Supports Last Provoking Vertex: {}\n* Supports Logical Operations: {}\n* Supports Vertex Attribute Divisor: {}\n* Supports Vertex Attribute Zero Divisor: {}\n* Supports Push Descriptors: {}\n* Supports Imageless Framebuffers: {}\n* Supports Global Priority: {}\n* Supports Multiple Viewports: {}\n* Supports Shader Viewport Index: {}\n* Supports SPIR-V 1.4: {}\n* Supports Shader Invocation Demotion: {}\n* Supports 16-bit FP: {}\n* Supports 8-bit Integers: {}\n* Supports 16-bit Integers: {}\n* Supports 64-bit Integers: {}\n* Supports Atomic 64-bit Integers: {}\n* Supports Floating Point Behavior Control: {}\n* Supports Image Read Without Format: {}\n* Supports List Primitive Topology Restart: {}\n* Supports Patch List Primitive Topology Restart: {}\n* Supports Subgroup Vote: {}\n* Subgroup Size: {}\n* BCn Support: {}",
            supportsUint8Indices, supportsSamplerMirrorClampToEdge, supportsSamplerReductionMode, supportsCustomBorderColor, supportsLastProvokingVertex, supportsLogicOp, supportsVertexAttributeDivisor, supportsVertexAttributeZeroDivisor, supportsPushDescriptors, supportsImagelessFramebuffers, supportsGlobalPriority, supportsMultipleViewports, supportsShaderViewportIndexLayer, supportsSpirv14, supportsShaderDemoteToHelper, supportsFloat16, supportsInt8, supportsInt16, supportsInt64, supportsAtomicInt64, supportsFloatControls, supportsImageReadWithoutFormat, supportsTopologyListRestart, supportsTopologyPatchListRestart, supportsSubgroupVote, subgroupSize, bcnSupport.to_string()
        );
    }

    TraitManager::QuirkManager::QuirkManager(const vk::PhysicalDeviceProperties &deviceProperties, const vk::PhysicalDeviceDriverProperties &driverProperties) {
        switch (driverProperties.driverID) {
            case vk::DriverId::eQualcommProprietary: {
                needsIndividualTextureBindingWrites = true;
                vkImageMutableFormatCostly = true; // Disables UBWC
                adrenoRelaxedFormatAliasing = true;
                adrenoBrokenFormatReport = true;
                brokenDescriptorAliasing = true;
                relaxedRenderPassCompatibility = true; // Adreno drivers support relaxed render pass compatibility rules

                if (deviceProperties.driverVersion < VK_MAKE_VERSION(512, 600, 0))
                    maxSubpassCount = 64; // Driver will segfault while destroying the renderpass and associated objects if this is exceeded on all 5xx and below drivers
                maxGlobalPriority = vk::QueueGlobalPriorityEXT::eHigh;
                break;
            }

            case vk::DriverId::eMesaTurnip: {
                vkImageMutableFormatCostly = true; // Disables UBWC and forces linear tiling
                adrenoRelaxedFormatAliasing = true;
                break;
            }

            case vk::DriverId::eArmProprietary: {
                maxGlobalPriority = vk::QueueGlobalPriorityEXT::eHigh;
                break;
            }

            case vk::DriverId::eNvidiaProprietary: {
                relaxedRenderPassCompatibility = true;
                break;
            }

            case vk::DriverId::eAmdProprietary: {
                maxGlobalPriority = vk::QueueGlobalPriorityEXT::eHigh;
                break;
            }

            default:
                break;
        }
    }

    std::string TraitManager::QuirkManager::Summary() {
        return fmt::format(
            "\n* Needs Individual Texture Binding Writes: {}\n* VkImage Mutable Format is costly: {}\n* Adreno Relaxed Format Aliasing: {}\n* Adreno Broken Format Reporting: {}\n* Broken Descriptor Aliasing: {}\n* Relaxed Render Pass Compatibility: {}\n* Max Subpass Count: {}\n* Max Global Queue Priority: {}",
            needsIndividualTextureBindingWrites, vkImageMutableFormatCostly, adrenoRelaxedFormatAliasing, adrenoBrokenFormatReport, brokenDescriptorAliasing, relaxedRenderPassCompatibility, maxSubpassCount, vk::to_string(maxGlobalPriority)
        );
    }

    void TraitManager::ApplyDriverPatches(const vk::raii::Context &context) {
        // Create an instance without validation layers in order to get pointers to the functions we need to patch from the driver
        vk::ApplicationInfo applicationInfo{
            .apiVersion = VK_API_VERSION_1_0,
        };

        auto instance{vk::raii::Instance(context, vk::InstanceCreateInfo{
            .pApplicationInfo = &applicationInfo
        })};

        auto physicalDevice{std::move(instance.enumeratePhysicalDevices().front())};
        auto properties{physicalDevice.getProperties()};

        // Apply BCeNabler for Adreno devices
        auto type{adrenotools_get_bcn_type(VK_VERSION_MAJOR(properties.driverVersion), VK_VERSION_MINOR(properties.driverVersion), properties.vendorID)};
        if (type == ADRENOTOOLS_BCN_PATCH) {
            if (adrenotools_patch_bcn(reinterpret_cast<void *>(physicalDevice.getDispatcher()->vkGetPhysicalDeviceFormatProperties)))
                Logger::Info("Applied BCeNabler patch");
            else
                throw exception("Failed to apply BCeNabler patch!");
            bcnSupport.set();
        } else if (type == ADRENOTOOLS_BCN_BLOB) {
            Logger::Info("BCeNabler skipped, blob BCN support is present");
            bcnSupport.set();
        }
    }
}
