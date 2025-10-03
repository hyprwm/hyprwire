#pragma once

#include <hyprwire/core/implementation/Spec.hpp>

namespace Hyprwire {
    class CServerSpec : public IProtocolSpec {
      public:
        CServerSpec(const std::string& name, uint32_t ver) : m_name(name), m_version(ver) {
            ;
        }
        virtual ~CServerSpec() = default;

        virtual std::string specName() {
            return m_name;
        }

        virtual uint32_t specVer() {
            return m_version;
        }

        virtual std::vector<Hyprutils::Memory::CSharedPointer<IProtocolObjectSpec>> objects() {
            return {};
        }

        std::string m_name;
        uint32_t    m_version = 0;
    };
};