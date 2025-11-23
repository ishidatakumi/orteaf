#include "orteaf/internal/architecture/cpu_detect.h"

#include "orteaf/internal/backend/backend.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <sys/sysctl.h>
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#define ORTEAF_HAS_X86_CPUID 1
#endif

namespace orteaf::internal::architecture {

namespace {

namespace tables = ::orteaf::generated::architecture_tables;

struct CpuInfo {
    std::string vendor; // lower-case identifier (intel/amd/apple/...)
    std::optional<int> family;
    std::optional<int> model;
    std::unordered_set<std::string> features; // lower-case feature tokens (avx2, avx512, ...)
    std::string machine_id; // lower-case hardware identifier (hw.model or DMI strings)
};

std::string toLowerCopy(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

#if defined(__APPLE__)
std::string readSysctlString(const char* key) {
    std::size_t size = 0;
    if (sysctlbyname(key, nullptr, &size, nullptr, 0) != 0 || size == 0) {
        return {};
    }
    std::string buffer(size, '\0');
    if (sysctlbyname(key, buffer.data(), &size, nullptr, 0) != 0 || size == 0) {
        return {};
    }
    if (!buffer.empty() && buffer.back() == '\0') {
        buffer.pop_back();
    }
    return buffer;
}
#endif

#if defined(__APPLE__)
std::string readHardwareModelFromIORegistry() {
    io_registry_entry_t entry = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
    if (entry == MACH_PORT_NULL) {
        return {};
    }

    std::string result;
    if (CFTypeRef model = IORegistryEntryCreateCFProperty(entry, CFSTR("model"), kCFAllocatorDefault, 0)) {
        if (CFGetTypeID(model) == CFDataGetTypeID()) {
            CFDataRef data = static_cast<CFDataRef>(model);
            const UInt8* bytes = CFDataGetBytePtr(data);
            if (bytes != nullptr) {
                result.assign(reinterpret_cast<const char*>(bytes),
                              reinterpret_cast<const char*>(bytes) + CFDataGetLength(data));
                while (!result.empty() && result.back() == '\0') {
                    result.pop_back();
                }
            }
        }
        CFRelease(model);
    }

    IOObjectRelease(entry);
    return result;
}
#endif

std::string readFirstExistingFileLower(std::initializer_list<const char*> paths) {
    for (const char* path : paths) {
        std::ifstream stream(path);
        if (!stream.is_open()) {
            continue;
        }
        std::string line;
        std::getline(stream, line);
        if (!line.empty()) {
            return toLowerCopy(line);
        }
    }
    return {};
}

#if defined(ORTEAF_HAS_X86_CPUID)
#if defined(_MSC_VER)
bool cpuId(unsigned int leaf, unsigned int subleaf, unsigned int& eax, unsigned int& ebx,
           unsigned int& ecx, unsigned int& edx) {
    int regs[4];
    __cpuidex(regs, static_cast<int>(leaf), static_cast<int>(subleaf));
    eax = static_cast<unsigned int>(regs[0]);
    ebx = static_cast<unsigned int>(regs[1]);
    ecx = static_cast<unsigned int>(regs[2]);
    edx = static_cast<unsigned int>(regs[3]);
    return true;
}
#else
bool cpuId(unsigned int leaf, unsigned int subleaf, unsigned int& eax, unsigned int& ebx,
           unsigned int& ecx, unsigned int& edx) {
    return __get_cpuid_count(leaf, subleaf, &eax, &ebx, &ecx, &edx);
}
#endif
#endif
/**
 * @brief Gather the signals used to match the host CPU against the architecture tables.
 *
 * The helper reads machine identifiers (sysctl or DMI strings), vendor/family/model via CPUID
 * when available, and builds a lower-case feature set. This normalized `CpuInfo` is consumed by
 * `matchesDetectSpec`.
 */
CpuInfo collectCpuInfo() {
    CpuInfo info;

#if defined(__APPLE__)
    if (info.machine_id.empty()) {
        info.machine_id = toLowerCopy(readSysctlString("hw.model"));
    }
    if (info.machine_id.empty()) {
        info.machine_id = toLowerCopy(readHardwareModelFromIORegistry());
    }
#else
    if (info.machine_id.empty()) {
        info.machine_id = readFirstExistingFileLower({
            "/sys/devices/virtual/dmi/id/product_name",
            "/sys/devices/virtual/dmi/id/board_name",
        });
    }
#endif

#if defined(ORTEAF_HAS_X86_CPUID)
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    if (cpuId(0, 0, eax, ebx, ecx, edx)) {
        char vendor_chars[13] = {};
        std::memcpy(vendor_chars + 0, &ebx, sizeof(unsigned int));
        std::memcpy(vendor_chars + 4, &edx, sizeof(unsigned int));
        std::memcpy(vendor_chars + 8, &ecx, sizeof(unsigned int));
        std::string vendor_raw(vendor_chars, 12);
        auto vendor_lower = toLowerCopy(vendor_raw);
        if (vendor_lower.find("intel") != std::string::npos) {
            info.vendor = "intel";
        } else if (vendor_lower.find("amd") != std::string::npos) {
            info.vendor = "amd";
        } else {
            info.vendor = vendor_lower;
        }
    }
    if (cpuId(1, 0, eax, ebx, ecx, edx)) {
        const unsigned int base_family = (eax >> 8) & 0xF;
        const unsigned int ext_family = (eax >> 20) & 0xFF;
        unsigned int family = base_family;
        if (base_family == 0xF) {
            family += ext_family;
        }
        info.family = static_cast<int>(family);

        const unsigned int base_model = (eax >> 4) & 0xF;
        const unsigned int ext_model = (eax >> 16) & 0xF;
        unsigned int model = base_model;
        if (base_family == 0x6 || base_family == 0xF) {
            model |= (ext_model << 4);
        }
        info.model = static_cast<int>(model);
    }
    if (cpuId(7, 0, eax, ebx, ecx, edx)) {
        if (ebx & (1u << 5)) {
            info.features.insert("avx2");
        }
        if (ebx & (1u << 16)) {
            info.features.insert("avx512");
        }
    }
#elif defined(__APPLE__)
    info.vendor = "apple";
#else
    info.vendor = "generic";
#endif

#if defined(__APPLE__)
    if (info.vendor.empty()) {
        info.vendor = "apple";
    }
#endif
    if (info.vendor.empty()) {
        info.vendor = "generic";
    }
    if (!info.machine_id.empty()) {
        info.machine_id = toLowerCopy(info.machine_id);
    }

    return info;
}

bool hasAllFeatures(const CpuInfo& info, std::size_t begin, std::size_t end) {
    if (begin == end) {
        return true;
    }
    if (info.features.empty()) {
        return false;
    }
    for (std::size_t i = begin; i < end; ++i) {
        const auto spec = toLowerCopy(tables::kArchitectureDetectFeatures[i]);
        if (!info.features.count(spec)) {
            return false;
        }
    }
    return true;
}

bool matchesMachineId(const CpuInfo& info, std::size_t begin, std::size_t end) {
    if (begin == end) {
        return true;
    }
    if (info.machine_id.empty()) {
        return false;
    }
    for (std::size_t i = begin; i < end; ++i) {
        const auto spec = toLowerCopy(tables::kArchitectureDetectMachineIds[i]);
        if (spec == info.machine_id) {
            return true;
        }
    }
    return false;
}

bool matchesCpuModels(const CpuInfo& info, std::size_t begin, std::size_t end) {
    if (begin == end) {
        return true;
    }
    if (!info.model) {
        return false;
    }
    for (std::size_t i = begin; i < end; ++i) {
        if (tables::kArchitectureDetectCpuModels[i] == *info.model) {
            return true;
        }
    }
    return false;
}

bool matchesDetectSpec(std::size_t index, const CpuInfo& info) {
    const auto vendor = tables::kArchitectureDetectVendors[index];
    if (!vendor.empty()) {
        if (info.vendor != toLowerCopy(vendor)) {
            return false;
        }
    }

    const int family = tables::kArchitectureDetectCpuFamilies[index];
    if (family >= 0) {
        if (!info.family || *info.family != family) {
            return false;
        }
    }

    const auto model_begin = tables::kArchitectureDetectCpuModelOffsets[index];
    const auto model_end = tables::kArchitectureDetectCpuModelOffsets[index + 1];
    if (!matchesCpuModels(info, model_begin, model_end)) {
        return false;
    }

    const auto feature_begin = tables::kArchitectureDetectFeatureOffsets[index];
    const auto feature_end = tables::kArchitectureDetectFeatureOffsets[index + 1];
    if (!hasAllFeatures(info, feature_begin, feature_end)) {
        return false;
    }

    const auto machine_begin = tables::kArchitectureDetectMachineIdOffsets[index];
    const auto machine_end = tables::kArchitectureDetectMachineIdOffsets[index + 1];
    if (!matchesMachineId(info, machine_begin, machine_end)) {
        return false;
    }

    return true;
}

}  // namespace

/**
 * @copydoc orteaf::internal::architecture::detectCpuArchitecture
 */
Architecture detectCpuArchitecture() {
    const CpuInfo info = collectCpuInfo();
    const std::size_t count = tables::kArchitectureCount;
    Architecture fallback = Architecture::CpuGeneric;

    for (std::size_t index = 0; index < count; ++index) {
        const Architecture arch = kAllArchitectures[index];
        if (localIndexOf(arch) == 0) {
            continue; // skip generic, reserve as fallback
        }
        if (backendOf(arch) != backend::Backend::Cpu) {
            continue;
        }
        if (!matchesDetectSpec(index, info)) {
            continue;
        }
        return arch;
    }
    return fallback;
}

}  // namespace orteaf::internal::architecture
