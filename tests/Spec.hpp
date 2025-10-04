#pragma once

#include <hyprwire/core/types/MessageMagic.hpp>
#include <hyprwire/hyprwire.hpp>

class CTestObjectManager : public Hyprwire::IProtocolObjectSpec {
  public:
    CTestObjectManager()          = default;
    virtual ~CTestObjectManager() = default;

    virtual std::string objectName() {
        return "my_object";
    }

    std::vector<Hyprwire::SMethod>                m_c2s = {Hyprwire::SMethod{
                       .idx       = 0,
                       .params    = {Hyprwire::HW_MESSAGE_MAGIC_TYPE_VARCHAR},
                       .returnsId = false,
    }};

    std::vector<Hyprwire::SMethod>                m_s2c = {Hyprwire::SMethod{
                       .idx       = 0,
                       .params    = {Hyprwire::HW_MESSAGE_MAGIC_TYPE_VARCHAR},
                       .returnsId = false,
    }};

    virtual const std::vector<Hyprwire::SMethod>& c2s() {
        return m_c2s;
    }
    virtual const std::vector<Hyprwire::SMethod>& s2c() {
        return m_s2c;
    }
};

class CTestProtocolSpec : public Hyprwire::IProtocolSpec {
  public:
    CTestProtocolSpec()          = default;
    virtual ~CTestProtocolSpec() = default;

    virtual std::string specName() {
        return "my_protocol";
    }

    virtual uint32_t specVer() {
        return 1;
    }

    virtual std::vector<Hyprutils::Memory::CSharedPointer<Hyprwire::IProtocolObjectSpec>> objects() {
        return {Hyprutils::Memory::makeShared<CTestObjectManager>()};
    }
};
