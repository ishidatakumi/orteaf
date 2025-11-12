#include <gtest/gtest.h>

#include <system_error>

#include "orteaf/internal/runtime/manager/cpu/cpu_device_manager.h"
#include "orteaf/internal/diagnostics/error/error.h"

namespace architecture = orteaf::internal::architecture;
namespace base = orteaf::internal::base;
namespace cpu_rt = orteaf::internal::runtime::cpu;

namespace {

struct MockCpuBackendOps {
    static inline architecture::Architecture next_arch =
        architecture::Architecture::cpu_generic;
    static inline int detect_calls = 0;

    static void reset(architecture::Architecture arch) {
        next_arch = arch;
        detect_calls = 0;
    }

    static architecture::Architecture detectArchitecture() {
        ++detect_calls;
        return next_arch;
    }
};

using MockCpuDeviceManager = cpu_rt::CpuDeviceManager<MockCpuBackendOps>;

}  // namespace

TEST(CpuDeviceManagerMockTest, UsesInjectedBackendOps) {
    MockCpuBackendOps::reset(architecture::Architecture::cpu_zen4);
    MockCpuDeviceManager manager;

    EXPECT_EQ(manager.getDeviceCount(), 0u);

    manager.initializeDevices();
    EXPECT_EQ(MockCpuBackendOps::detect_calls, 1);
    EXPECT_EQ(manager.getDeviceCount(), 1u);
    EXPECT_EQ(manager.getArch(base::DeviceId{0}), architecture::Architecture::cpu_zen4);
    EXPECT_TRUE(manager.isAlive(base::DeviceId{0}));

    manager.initializeDevices();
    EXPECT_EQ(MockCpuBackendOps::detect_calls, 1) << "should not re-detect when already initialized";

    manager.shutdown();
    EXPECT_EQ(manager.getDeviceCount(), 0u);
    EXPECT_THROW(manager.getArch(base::DeviceId{0}), std::system_error);
}
