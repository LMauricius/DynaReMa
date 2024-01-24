#pragma once
#ifndef INCLUDED_DYNASMA_MAN_SIMPLE_H
#define INCLUDED_DYNASMA_MAN_SIMPLE_H

#include "dynasma/core_concepts.hpp"
#include "dynasma/managers/abstract.hpp"
#include "dynasma/pointer.hpp"
#include "dynasma/util/dynamic_typing.hpp"
#include "dynasma/util/helpful_concepts.hpp"
#include "dynasma/util/ref_management.hpp"

#include <concepts>
#include <list>
#include <variant>

namespace dynasma {

/**
 * @brief The naive asset manager. Does the job, but has no advanced memory
 * management or caching features
 * @tparam Seed A SeedLike type describing everything we need to know about the
 * Asset
 * @tparam Alloc The AllocatorLike type whose instance will be used to construct
 * instances of the Seed::Asset
 */
template <SeedLike Seed, AllocatorLike<typename Seed::Asset> Alloc>
class NaiveManager : public virtual AbstractManager<Seed> {
  public:
    using Asset = typename Seed::Asset;

  private:
    // reference counting response implementation
    class ProxyRefCtr : public TypeErasedReferenceCounter<Asset> {
        Seed m_seed;
        NaiveManager &m_manager;
        std::list<ProxyRefCtr>::iterator m_it;

      protected:
        void handle_usable_impl() override {
            Asset *p_asset = m_manager.m_allocator.allocate(1);
            this->p_obj = p_asset;
            std::visit(
                [p_asset](const auto &arg) { new (p_asset) Seed::Asset(arg); },
                m_seed.kernel);
        }
        void handle_unloadable_impl() override {
            Asset &asset_casted = *dynamic_cast<Asset *>(this->p_obj);
            this->p_obj->~PolymorphicBase();
            m_manager.m_allocator.deallocate(&asset_casted, 1);
            this->p_obj = nullptr;
        }
        void handle_forgettable_impl() override {
            m_manager.m_seed_registry.erase(m_it); // deletes this
        }

      public:
        ProxyRefCtr(Seed &&seed, NaiveManager &manager)
            : m_seed(seed), m_it(), m_manager(manager) {}

        void setSelfRegistryPos(std::list<ProxyRefCtr>::iterator it) {
            m_it = it;
        }
    };

    [[no_unique_address, msvc::no_unique_address]] Alloc m_allocator;
    std::list<ProxyRefCtr> m_seed_registry;

  public:
    NaiveManager(const NaiveManager &) = delete;
    NaiveManager(NaiveManager &&) = delete;
    NaiveManager &operator=(const NaiveManager &) = delete;
    NaiveManager &operator=(NaiveManager &&) = delete;

    NaiveManager()
        requires std::default_initializable<Alloc>
        : m_allocator(){};
    NaiveManager(const Alloc &a) : m_allocator(a) {}
    NaiveManager(Alloc &&a) : m_allocator(std::move(a)) {}
    ~NaiveManager() = default;

    using AbstractManager<Seed>::register_asset;

    WeakPtr<Asset> register_asset(Seed &&seed) override {
        m_seed_registry.emplace_front(std::move(seed), *this);
        m_seed_registry.front().setSelfRegistryPos(m_seed_registry.begin());
        return WeakPtr<Asset>(m_seed_registry.front());
    }
    void clean(std::size_t bytenum) override {
        // do nothing; cleans itself automatically
    }
};
} // namespace dynasma

#endif // INCLUDED_DYNASMA_MAN_SIMPLE_H