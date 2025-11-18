#pragma once

#include <stdexcept>

namespace orteaf::tests {

/**
 * @brief Registry that lets static helper functions forward calls into a bound mock instance.
 *
 * Tests should create a `Guard` on the stack to bind a mock object. Production code can call
 * `StaticMockRegistry<Mock>::get()` to access the currently-bound mock without introducing
 * virtual dispatch.
 */
template <class Mock>
class StaticMockRegistry {
public:
    class Guard {
    public:
        explicit Guard(Mock& mock) noexcept : mock_(&mock) {
            instance() = mock_;
        }

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

        ~Guard() {
            if (instance() == mock_) {
                instance() = nullptr;
            }
        }

    private:
        Mock* mock_;
    };

    static Mock& get() {
        Mock* ptr = instance();
        if (ptr == nullptr) {
            throw std::logic_error("StaticMockRegistry: mock not bound");
        }
        return *ptr;
    }

    static void bind(Mock& mock) noexcept { instance() = &mock; }
    static void unbind(Mock& mock) noexcept {
        if (instance() == &mock) {
            instance() = nullptr;
        }
    }

private:
    static Mock*& instance() {
        static Mock* ptr = nullptr;
        return ptr;
    }
};

}  // namespace orteaf::tests

