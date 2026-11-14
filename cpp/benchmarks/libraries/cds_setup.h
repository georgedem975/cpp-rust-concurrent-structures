#ifndef CPP_TESTS_LIBRARIES_CDS_SETUP_H_
#define CPP_TESTS_LIBRARIES_CDS_SETUP_H_

#include <cds/gc/hp.h>
#include <cds/init.h>
#include <cds/threading/model.h>

#include <cstddef>
#include <memory>
#include <stdexcept>

namespace cdsbench {
    namespace detail {
        constexpr std::size_t kHazardPtrsPerThread = 96;
        constexpr std::size_t kMaxThreads = 128;

        constexpr std::size_t kMaxRetiredPtrCount = kHazardPtrsPerThread * kMaxThreads * 2;

        inline std::unique_ptr<cds::gc::HP>& Gc() {
            static std::unique_ptr<cds::gc::HP> gc;
            return gc;
        }

        struct ThreadAttachmentState {
            bool initialized = false;
            bool attachedByUs = false;

            ~ThreadAttachmentState() {
                if (attachedByUs && cds::threading::Manager::isThreadAttached()) {
                    cds::threading::Manager::detachThread();
                }
            }
        };

        inline ThreadAttachmentState& CurrentThreadState() {
            static thread_local ThreadAttachmentState state;
            return state;
        }
    }

    inline void Initialize() {
        if (detail::Gc()) {
            throw std::logic_error("libcds is already initialized");
        }

        cds::Initialize();

        try {
            detail::Gc() = std::make_unique<cds::gc::HP>(
                detail::kHazardPtrsPerThread,
                detail::kMaxThreads,
                detail::kMaxRetiredPtrCount
            );
        } catch (...) {
            cds::Terminate();
            throw;
        }
    }

    inline void AttachCurrentThread() {
        auto& state = detail::CurrentThreadState();

        if (state.initialized) {
            return;
        }

        if (!cds::threading::Manager::isThreadAttached()) {
            cds::threading::Manager::attachThread();
            state.attachedByUs = true;
        }

        state.initialized = true;
    }

    inline void DetachCurrentThread() {
        auto& state = detail::CurrentThreadState();

        if (!state.initialized) {
            return;
        }

        if (state.attachedByUs &&
            cds::threading::Manager::isThreadAttached()) {
            cds::threading::Manager::detachThread();
        }

        state.initialized = false;
        state.attachedByUs = false;
    }

    inline void Terminate() {
        if (!detail::Gc()) {
            return;
        }

        DetachCurrentThread();

        detail::Gc().reset();

        cds::Terminate();
    }
}

#endif