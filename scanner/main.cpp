#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include <cstdint>

#include <pugixml.hpp>

#include <hyprwire/core/types/MessageMagic.hpp>

struct SRequestArgument {
    Hyprwire::eMessageMagic magic = Hyprwire::HW_MESSAGE_MAGIC_END, arrType = Hyprwire::HW_MESSAGE_MAGIC_END;
    std::string             interface;
    std::string             name;
    bool                    allowNull = false;
};

struct SMethodSpec {
    std::vector<SRequestArgument> args;
    std::string                   name;
    uint32_t                      since;
    std::string                   returns    = "";
    bool                          destructor = false;
    uint32_t                      idx        = 0;
};

struct SObjectSpec {
    std::vector<SMethodSpec> s2c;
    std::vector<SMethodSpec> c2s;
    std::string              name, nameCamel;
    int                      version = 1;
};

static std::vector<SObjectSpec> OBJECT_SPECS;

static bool                     clientCode = false;

static std::string              HEADER_PROTOCOL, HEADER_IMPL;
static std::string              SOURCE;

static std::string              camelize(std::string snake) {
    std::string result = "";
    for (size_t i = 0; i < snake.length(); ++i) {
        if (snake[i] == '_' && i != 0 && i + 1 < snake.length() && snake[i + 1] != '_') {
            result += ::toupper(snake[i + 1]);
            i++;
            continue;
        }

        result += snake[i];
    }

    return result;
}

static std::string capitalize(std::string str) {
    if (str.empty())
        return "";
    str[0] = ::toupper(str[0]);
    return str;
}

static Hyprwire::eMessageMagic strToMagic(const std::string_view& sv) {
    if (sv == "varchar")
        return Hyprwire::HW_MESSAGE_MAGIC_TYPE_VARCHAR;
    if (sv == "uint")
        return Hyprwire::HW_MESSAGE_MAGIC_TYPE_UINT;
    if (sv == "int")
        return Hyprwire::HW_MESSAGE_MAGIC_TYPE_INT;
    if (sv == "f32")
        return Hyprwire::HW_MESSAGE_MAGIC_TYPE_F32;
    // if (sv == "object")
    //     return Hyprwire::HW_MESSAGE_MAGIC_TYPE_OBJECT_ID;
    if (sv.starts_with("array "))
        return Hyprwire::HW_MESSAGE_MAGIC_TYPE_ARRAY;
    return Hyprwire::HW_MESSAGE_MAGIC_END;
}

static std::string magicToString(Hyprwire::eMessageMagic m, Hyprwire::eMessageMagic arrType = Hyprwire::HW_MESSAGE_MAGIC_END) {
    switch (m) {
        case Hyprwire::HW_MESSAGE_MAGIC_TYPE_VARCHAR: return "Hyprwire::HW_MESSAGE_MAGIC_TYPE_VARCHAR";
        case Hyprwire::HW_MESSAGE_MAGIC_TYPE_UINT: return "Hyprwire::HW_MESSAGE_MAGIC_TYPE_UINT";
        case Hyprwire::HW_MESSAGE_MAGIC_TYPE_INT: return "Hyprwire::HW_MESSAGE_MAGIC_TYPE_INT";
        case Hyprwire::HW_MESSAGE_MAGIC_TYPE_F32: return "Hyprwire::HW_MESSAGE_MAGIC_TYPE_F32";
        case Hyprwire::HW_MESSAGE_MAGIC_TYPE_ARRAY: return "Hyprwire::HW_MESSAGE_MAGIC_TYPE_ARRAY, " + magicToString(arrType);
        default: return "";
    }
}

static std::string argToC(Hyprwire::eMessageMagic m) {
    switch (m) {
        case Hyprwire::HW_MESSAGE_MAGIC_TYPE_VARCHAR: return "const char*";
        case Hyprwire::HW_MESSAGE_MAGIC_TYPE_UINT: return "uint32_t";
        case Hyprwire::HW_MESSAGE_MAGIC_TYPE_INT: return "int32_t";
        case Hyprwire::HW_MESSAGE_MAGIC_TYPE_F32: return "float";
        default: return "";
    }
}

static std::string argToC(const SRequestArgument& arg) {
    switch (arg.magic) {
        case Hyprwire::HW_MESSAGE_MAGIC_TYPE_VARCHAR: return "const char*";
        case Hyprwire::HW_MESSAGE_MAGIC_TYPE_UINT: return "uint32_t";
        case Hyprwire::HW_MESSAGE_MAGIC_TYPE_INT: return "int32_t";
        case Hyprwire::HW_MESSAGE_MAGIC_TYPE_F32: return "float";
        case Hyprwire::HW_MESSAGE_MAGIC_TYPE_ARRAY: return "std::vector<" + argToC(arg.arrType) + ">";
        default: return "";
    }
}

static std::string argsToC(const std::vector<SRequestArgument>& args, bool noNames = false, bool noTypes = false, bool addSequence = false, bool pureC = false, bool unC = false) {
    std::string cstr;

    if (addSequence) {
        if (noTypes)
            cstr += "seq, ";
        else {
            if (noNames)
                cstr += "uint32_t, ";
            else
                cstr += "uint32_t seq, ";
        }
    }

    for (const auto& m : args) {
        if (m.arrType != Hyprwire::HW_MESSAGE_MAGIC_END && pureC) {
            if (noTypes)
                cstr += std::format("{}.data(), (uint32_t){}.size(), ", m.name, m.name);
            else {
                if (noNames)
                    cstr += std::format("{}*, uint32_t, ", argToC(m.arrType));
                else
                    cstr += std::format("{}* {}, uint32_t {}, ", argToC(m.arrType), m.name, m.name + "_len");
            }
        } else if (m.arrType != Hyprwire::HW_MESSAGE_MAGIC_END && unC) {
            if (noTypes)
                cstr += std::format("std::vector<{}>{{ {}, {} + {} }}, ", argToC(m.arrType), m.name, m.name, m.name + "_len");
            else {
                if (noNames)
                    cstr += std::format("{}, ", argToC(m));
                else
                    cstr += std::format("{} {}, ", argToC(m), m.name);
            }
        } else {
            if (noTypes)
                cstr += std::format("{}, ", m.name);
            else {
                if (noNames)
                    cstr += std::format("{}, ", argToC(m));
                else
                    cstr += std::format("{} {}, ", argToC(m), m.name);
            }
        }
    }

    if (!cstr.empty())
        cstr = cstr.substr(0, cstr.size() - 2);

    return cstr;
}

static struct {
    std::string name;
    std::string nameOriginal;
    std::string fileName;
    uint32_t    version = 1;
} PROTO_DATA;

static bool scanProtocol(const pugi::xml_document& doc) {

    for (const auto& c : doc.child("protocol").children()) {
        if (c.name() != std::string_view{"object"})
            continue;

        const auto& object = c;

        SObjectSpec spec;
        spec.name      = object.attribute("name").as_string();
        spec.nameCamel = camelize(spec.name);
        spec.version   = object.attribute("version").as_int();

        // Generate s2c methods
        uint32_t currentIdx = 0;
        for (const auto& m : c.children()) {
            if (m.name() != std::string_view{"s2c"})
                continue;

            SMethodSpec method;
            method.name       = m.attribute("name").as_string();
            method.destructor = m.attribute("destructor").as_bool();
            method.since      = m.attribute("since").as_int();
            method.idx        = currentIdx++;

            for (const auto& param : m.children()) {
                if (param.name() == std::string_view{"arg"}) {
                    auto& a = method.args.emplace_back(SRequestArgument{
                        .magic     = strToMagic(param.attribute("type").as_string()),
                        .name      = param.attribute("name").as_string(),
                        .allowNull = param.attribute("allow_null").as_bool(),
                    });
                    if (a.magic == Hyprwire::HW_MESSAGE_MAGIC_TYPE_ARRAY)
                        a.arrType = strToMagic(std::string{param.attribute("type").as_string()}.substr(6));
                }

                if (param.name() == std::string_view{"returns"}) {
                    method.returns = param.attribute("iface").as_string();
                    continue;
                }
            }

            spec.s2c.emplace_back(std::move(method));
        }

        // Generate c2s methods
        currentIdx = 0;
        for (const auto& m : c.children()) {
            if (m.name() != std::string_view{"c2s"})
                continue;

            SMethodSpec method;
            method.name       = m.attribute("name").as_string();
            method.destructor = m.attribute("destructor").as_bool();
            method.since      = m.attribute("since").as_int();
            method.idx        = currentIdx++;

            for (const auto& param : m.children()) {
                if (param.name() == std::string_view{"arg"}) {
                    auto& a = method.args.emplace_back(SRequestArgument{
                        .magic     = strToMagic(param.attribute("type").as_string()),
                        .name      = param.attribute("name").as_string(),
                        .allowNull = param.attribute("allow_null").as_bool(),
                    });
                    if (a.magic == Hyprwire::HW_MESSAGE_MAGIC_TYPE_ARRAY)
                        a.arrType = strToMagic(std::string{param.attribute("type").as_string()}.substr(6));
                    continue;
                }

                if (param.name() == std::string_view{"returns"}) {
                    method.returns = param.attribute("iface").as_string();
                    continue;
                }
            }

            spec.c2s.emplace_back(std::move(method));
        }

        OBJECT_SPECS.emplace_back(std::move(spec));
    }

    return true;
}

static bool generateProtocolHeader(const pugi::xml_document& doc) {

    HEADER_PROTOCOL += R"#(
#pragma once

#include <hyprwire/core/types/MessageMagic.hpp>
#include <hyprwire/hyprwire.hpp>
#include <vector>
    )#";

    // begin objects

    for (const auto& object : OBJECT_SPECS) {
        HEADER_PROTOCOL += std::format(R"#(
class C{}Spec : public Hyprwire::IProtocolObjectSpec {{
  public:
    C{}Spec() = default;
    virtual ~C{}Spec() = default;

    virtual std::string objectName() {{
        return "{}";
    }}

        )#",
                                       capitalize(object.nameCamel), capitalize(object.nameCamel), capitalize(object.nameCamel), object.name);

        // Add C2S method arr and fn

        HEADER_PROTOCOL += "\n\tstd::vector<Hyprwire::SMethod>                m_c2s = {";

        for (const auto& m : object.c2s) {

            std::string argArrayStr;
            for (const auto& p : m.args) {
                argArrayStr += magicToString(p.magic, p.arrType) + ", ";
            }

            if (!argArrayStr.empty())
                argArrayStr = argArrayStr.substr(0, argArrayStr.size() - 2);

            HEADER_PROTOCOL += std::format(R"#(
Hyprwire::SMethod{{
.idx = {},
.params = {{ {} }},
.returnsType = "{}",
.since = {},
}},)#",
                                           m.idx, argArrayStr, m.returns, m.since);
        }

        if (!object.c2s.empty())
            HEADER_PROTOCOL.pop_back();

        HEADER_PROTOCOL += R"#(
    };

    virtual const std::vector<Hyprwire::SMethod>& c2s() {
        return m_c2s;
    }
)#";

        HEADER_PROTOCOL += "\n\tstd::vector<Hyprwire::SMethod>                m_s2c = {";

        for (const auto& m : object.s2c) {

            std::string argArrayStr;
            for (const auto& p : m.args) {
                argArrayStr += magicToString(p.magic) + ", ";
            }

            if (!argArrayStr.empty())
                argArrayStr = argArrayStr.substr(0, argArrayStr.size() - 2);

            HEADER_PROTOCOL += std::format(R"#(
Hyprwire::SMethod{{
.idx = {},
.params = {{ {} }},
.since = {},
}},)#",
                                           m.idx, argArrayStr, m.since);
        }

        if (!object.s2c.empty())
            HEADER_PROTOCOL.pop_back();

        HEADER_PROTOCOL += R"#(
    };

    virtual const std::vector<Hyprwire::SMethod>& s2c() {
        return m_s2c;
    }

};
)#";
    }

    // end objects

    // protocol object

    std::string objectVecStr = "";
    for (const auto& o : OBJECT_SPECS) {
        objectVecStr += std::format("Hyprutils::Memory::makeShared<C{}Spec>(), ", capitalize(o.nameCamel));
    }

    objectVecStr = objectVecStr.substr(0, objectVecStr.size() - 2);

    HEADER_PROTOCOL +=
        std::format(R"#(
class C{}ProtocolSpec : public Hyprwire::IProtocolSpec {{
  public:
    C{}ProtocolSpec()          = default;
    virtual ~C{}ProtocolSpec() = default;

    virtual std::string specName() {{
        return "{}";
    }}

    virtual uint32_t specVer() {{
        return {};
    }}

    virtual std::vector<Hyprutils::Memory::CSharedPointer<Hyprwire::IProtocolObjectSpec>> objects() {{
        return {{ {} }};
    }}
}};
)#",
                    capitalize(PROTO_DATA.name), capitalize(PROTO_DATA.name), capitalize(PROTO_DATA.name), PROTO_DATA.nameOriginal, PROTO_DATA.version, objectVecStr);

    return true;
}

static bool generateClientCodeHeader(const pugi::xml_document& doc) {
    HEADER_IMPL += std::format(R"#(
#pragma once

#include <functional>
#include "{}-spec.hpp"
    )#",
                               PROTO_DATA.nameOriginal);

    for (const auto& o : OBJECT_SPECS) {
        HEADER_IMPL += std::format(R"#(
class CC{}Object {{
  public:
    CC{}Object(Hyprutils::Memory::CSharedPointer<Hyprwire::IObject>&& object);
    ~CC{}Object();

    Hyprutils::Memory::CSharedPointer<Hyprwire::IObject> getObject() {{
        return m_object;
    }}

)#",
                                   capitalize(o.nameCamel), capitalize(o.nameCamel), capitalize(o.nameCamel));

        for (const auto& m : o.c2s) {
            const auto RETURN_TYPE = m.returns.empty() ? "void" : "Hyprutils::Memory::CSharedPointer<Hyprwire::IObject>";
            HEADER_IMPL += std::format(R"#(
    {} send{}({});
            )#",
                                       RETURN_TYPE, capitalize(camelize(m.name)), argsToC(m.args));
        }

        for (const auto& m : o.s2c) {
            HEADER_IMPL += std::format(R"#(
    void set{}(std::function<void({})>&& fn);
            )#",
                                       capitalize(camelize(m.name)), argsToC(m.args, true));
        }

        HEADER_IMPL += "\n  private:\n\tstruct {\n";

        for (const auto& m : o.s2c) {
            HEADER_IMPL += std::format(R"#( std::function<void({})> {};
)#",
                                       argsToC(m.args, true), m.name);
        }

        HEADER_IMPL += R"#( } m_listeners;
        
    Hyprutils::Memory::CSharedPointer<Hyprwire::IObject> m_object;
};
)#";
    }

    HEADER_IMPL += std::format(R"#(
class CC{}Impl : public Hyprwire::IProtocolClientImplementation {{
  public:
    CC{}Impl(uint32_t version);
    virtual ~CC{}Impl() = default;

    virtual Hyprutils::Memory::CSharedPointer<Hyprwire::IProtocolSpec> protocol();

    virtual std::vector<Hyprutils::Memory::CSharedPointer<Hyprwire::SClientObjectImplementation>> implementation();

  private:
    uint32_t m_version = 0;
}};
)#",
                               capitalize(PROTO_DATA.name), capitalize(PROTO_DATA.name), capitalize(PROTO_DATA.name));

    return true;
}

static bool generateClientCodeSource(const pugi::xml_document& doc) {
    SOURCE += std::format(R"#(
#define private public
#include "{}-client.hpp"
#undef private

using namespace Hyprutils::Memory;
#define SP CSharedPointer
    )#",
                          PROTO_DATA.nameOriginal);

    for (const auto& o : OBJECT_SPECS) {
        for (const auto& m : o.s2c) {
            SOURCE += std::format(R"#(
static void {}_method{}(Hyprwire::IObject* r{}) {{
    auto& fn = rc<{}*>(r->getData())->m_listeners.{};
    if (fn)
        fn({});
}}
)#",
                                  o.nameCamel, m.idx, m.args.empty() ? "" : ", " + argsToC(m.args, false, false, false, true), std::format("CC{}Object", capitalize(o.nameCamel)),
                                  m.name, argsToC(m.args, false, true, false, false, true));
        }

        SOURCE += std::format(R"#(
CC{}Object::CC{}Object(Hyprutils::Memory::CSharedPointer<Hyprwire::IObject>&& object) : m_object(std::move(object)) {{
    m_object->setData(this);
            )#",
                              capitalize(o.nameCamel), capitalize(o.nameCamel));

        for (const auto& m : o.s2c) {
            SOURCE += std::format(R"#(
    m_object->listen({}, rc<void*>(::{}_method{}));)#",
                                  m.idx, o.nameCamel, m.idx);
        }

        SOURCE += std::format(R"#(
}}

CC{}Object::~CC{}Object() {{
    ; // TODO: call destructor if present
}})#",
                              capitalize(o.nameCamel), capitalize(o.nameCamel));

        for (const auto& m : o.c2s) {
            if (m.returns.empty()) {
                SOURCE += std::format(R"#(
void CC{}Object::send{}({}) {{
    m_object->call({}{});
}}
)#",
                                      capitalize(o.nameCamel), capitalize(camelize(m.name)), argsToC(m.args), m.idx,
                                      m.args.empty() ? "" : ", " + argsToC(m.args, false, true, false, true));
            } else {
                SOURCE += std::format(R"#(

SP<Hyprwire::IObject> CC{}Object::send{}({}) {{
    auto id = m_object->call({}{});
    return m_object->clientSock()->objectForId(id);
}}
)#",
                                      capitalize(o.nameCamel), capitalize(camelize(m.name)), argsToC(m.args), m.idx,
                                      m.args.empty() ? "" : ", " + argsToC(m.args, false, true, false, true));
            }
        }

        for (const auto& m : o.s2c) {
            SOURCE += std::format(R"#(
void CC{}Object::set{}(std::function<void({})>&& fn) {{
    m_listeners.{} = std::move(fn);
}}
)#",
                                  capitalize(o.nameCamel), capitalize(camelize(m.name)), argsToC(m.args, true), m.name);
        }
    }

    SOURCE += std::format(R"#(
CC{}Impl::CC{}Impl(uint32_t ver) : m_version(ver) {{
    ;
}}

static auto {}Spec = makeShared<C{}ProtocolSpec>();

SP<Hyprwire::IProtocolSpec> CC{}Impl::protocol() {{
    return {}Spec;
}}

std::vector<SP<Hyprwire::SClientObjectImplementation>> CC{}Impl::implementation() {{
    return {{
)#",
                          capitalize(PROTO_DATA.name), capitalize(PROTO_DATA.name), PROTO_DATA.name, capitalize(PROTO_DATA.name), capitalize(PROTO_DATA.name), PROTO_DATA.name,
                          capitalize(PROTO_DATA.name));

    for (const auto& o : OBJECT_SPECS) {
        SOURCE += std::format(R"#(
            makeShared<Hyprwire::SClientObjectImplementation>(Hyprwire::SClientObjectImplementation{{
                .objectName = "{}",
                .version    = m_version,
            }}),
)#",
                              o.name);
    }

    SOURCE += "};\n}\n";

    return true;
}

static bool generateServerCodeHeader(const pugi::xml_document& doc) {
    HEADER_IMPL += std::format(R"#(
#pragma once

#include <functional>
#include "{}-spec.hpp"
    )#",
                               PROTO_DATA.nameOriginal);

    for (const auto& o : OBJECT_SPECS) {
        HEADER_IMPL += std::format(R"#(
class C{}Object {{
  public:
    C{}Object(Hyprutils::Memory::CSharedPointer<Hyprwire::IObject>&& object);
    ~C{}Object();

    Hyprutils::Memory::CSharedPointer<Hyprwire::IObject> getObject() {{
        return m_object;
    }}

    void setOnDestroy(std::function<void()>&& fn) {{
        m_object->setOnDestroy(std::move(fn));
    }}

)#",
                                   capitalize(o.nameCamel), capitalize(o.nameCamel), capitalize(o.nameCamel));

        for (const auto& m : o.s2c) {
            HEADER_IMPL += std::format(R"#(
    void send{}({});
            )#",
                                       capitalize(camelize(m.name)), argsToC(m.args));
        }

        for (const auto& m : o.c2s) {
            HEADER_IMPL += std::format(R"#(
    void set{}(std::function<void({})>&& fn);
            )#",
                                       capitalize(camelize(m.name)), argsToC(m.args, true, false, !m.returns.empty()));
        }

        HEADER_IMPL += "\n  private:\n\tstruct {\n";

        for (const auto& m : o.c2s) {
            HEADER_IMPL += std::format(R"#( std::function<void({})> {};
)#",
                                       argsToC(m.args, true, false, !m.returns.empty()), m.name);
        }

        HEADER_IMPL += R"#( } m_listeners;
        
    Hyprutils::Memory::CSharedPointer<Hyprwire::IObject> m_object;
};
)#";
    }

    HEADER_IMPL += std::format(R"#(
class C{}Impl : public Hyprwire::IProtocolServerImplementation {{
  public:
    C{}Impl(uint32_t version, std::function<void(Hyprutils::Memory::CSharedPointer<Hyprwire::IObject>)>&& bindFn);
    virtual ~C{}Impl() = default;

    virtual Hyprutils::Memory::CSharedPointer<Hyprwire::IProtocolSpec> protocol();

    virtual std::vector<Hyprutils::Memory::CSharedPointer<Hyprwire::SServerObjectImplementation>> implementation();

  private:
    uint32_t m_version = 0;
    std::function<void(Hyprutils::Memory::CSharedPointer<Hyprwire::IObject>)> m_bindFn;
}};
)#",
                               capitalize(PROTO_DATA.name), capitalize(PROTO_DATA.name), capitalize(PROTO_DATA.name));

    return true;
}

static bool generateServerCodeSource(const pugi::xml_document& doc) {
    SOURCE += std::format(R"#(
#define private public
#include "{}-server.hpp"
#undef private

using namespace Hyprutils::Memory;
#define SP CSharedPointer
    )#",
                          PROTO_DATA.nameOriginal);

    for (const auto& o : OBJECT_SPECS) {
        for (const auto& m : o.c2s) {
            SOURCE += std::format(R"#(
static void {}_method{}(Hyprwire::IObject* r{}) {{
    auto& fn = rc<{}*>(r->getData())->m_listeners.{};
    if (fn)
        fn({});
}}
)#",
                                  o.nameCamel, m.idx, m.args.empty() && m.returns.empty() ? "" : ", " + argsToC(m.args, false, false, !m.returns.empty(), true),
                                  std::format("C{}Object", capitalize(o.nameCamel)), m.name, argsToC(m.args, false, true, !m.returns.empty(), false, true));
        }

        SOURCE += std::format(R"#(
C{}Object::C{}Object(Hyprutils::Memory::CSharedPointer<Hyprwire::IObject>&& object) : m_object(std::move(object)) {{
    m_object->setData(this);
            )#",
                              capitalize(o.nameCamel), capitalize(o.nameCamel));

        for (const auto& m : o.c2s) {
            SOURCE += std::format(R"#(
    m_object->listen({}, rc<void*>(::{}_method{}));)#",
                                  m.idx, o.nameCamel, m.idx);
        }

        SOURCE += std::format(R"#(
}}

C{}Object::~C{}Object() {{
    ; // TODO: call destructor if present
}})#",
                              capitalize(o.nameCamel), capitalize(o.nameCamel));

        for (const auto& m : o.s2c) {
            SOURCE +=
                std::format(R"#(
void C{}Object::send{}({}) {{
    m_object->call({}{});
}}
)#",
                            capitalize(o.nameCamel), capitalize(camelize(m.name)), argsToC(m.args), m.idx, m.args.empty() ? "" : ", " + argsToC(m.args, false, true, false, true));
        }

        for (const auto& m : o.c2s) {
            SOURCE += std::format(R"#(
void C{}Object::set{}(std::function<void({})>&& fn) {{
    m_listeners.{} = std::move(fn);
}}
)#",
                                  capitalize(o.nameCamel), capitalize(camelize(m.name)), argsToC(m.args, true, false, !m.returns.empty()), m.name);
        }
    }

    SOURCE += std::format(R"#(
C{}Impl::C{}Impl(uint32_t ver, std::function<void(Hyprutils::Memory::CSharedPointer<Hyprwire::IObject>)>&& bindFn) : m_version(ver), m_bindFn(bindFn) {{
    ;
}}

static auto {}Spec = makeShared<C{}ProtocolSpec>();

SP<Hyprwire::IProtocolSpec> C{}Impl::protocol() {{
    return {}Spec;
}}

std::vector<SP<Hyprwire::SServerObjectImplementation>> C{}Impl::implementation() {{
    return {{
)#",
                          capitalize(PROTO_DATA.name), capitalize(PROTO_DATA.name), PROTO_DATA.name, capitalize(PROTO_DATA.name), capitalize(PROTO_DATA.name), PROTO_DATA.name,
                          capitalize(PROTO_DATA.name), capitalize(PROTO_DATA.name));

    bool first = true;
    for (const auto& o : OBJECT_SPECS) {
        if (first) {
            SOURCE += std::format(R"#(
            makeShared<Hyprwire::SServerObjectImplementation>(Hyprwire::SServerObjectImplementation{{
                .objectName = "{}",
                .version    = m_version,
                .onBind = [this] (Hyprutils::Memory::CSharedPointer<Hyprwire::IObject> r) {{ if (m_bindFn) m_bindFn(r); }}
            }}),
)#",
                                  o.name);
        } else {
            SOURCE += std::format(R"#(
            makeShared<Hyprwire::SServerObjectImplementation>(Hyprwire::SServerObjectImplementation{{
                .objectName = "{}",
                .version    = m_version,
            }}),
)#",
                                  o.name);
        }

        first = false;
    }

    SOURCE += "};\n}\n";

    return true;
}

int main(int argc, char** argv, char** envp) {
    std::string outpath   = "";
    std::string protopath = "";

    int         pathsTaken = 0;

    for (int i = 1; i < argc; ++i) {
        std::string curarg = argv[i];

        if (curarg == "-v" || curarg == "--version") {
            std::cout << SCANNER_VERSION << "\n";
            return 0;
        }

        if (curarg == "-c" || curarg == "--client") {
            clientCode = true;
            continue;
        }

        if (pathsTaken == 0) {
            protopath = curarg;
            pathsTaken++;
            continue;
        } else if (pathsTaken == 1) {
            outpath = curarg;
            pathsTaken++;
            continue;
        }

        std::cout << "Too many args or unknown arg " << curarg << "\n";
        return 1;
    }

    if (outpath.empty() || protopath.empty()) {
        std::cerr << "Not enough args\n";
        return 1;
    }

    // build!

    pugi::xml_document doc;
    if (auto x = doc.load_file(protopath.c_str()); !x) {
        std::cerr << "Couldn't load proto: " << x.description() << std::endl;
        return 1;
    }

    PROTO_DATA.nameOriginal = doc.child("protocol").attribute("name").as_string();
    PROTO_DATA.name         = camelize(PROTO_DATA.nameOriginal);
    PROTO_DATA.fileName     = protopath.substr(protopath.find_last_of('/') + 1, protopath.length() - (protopath.find_last_of('/') + 1) - 4);
    PROTO_DATA.version      = doc.child("protocol").attribute("version").as_int(1);

    const auto COPYRIGHT =
        std::format("// Generated with hyprwire-scanner {}. Made with vaxry's keyboard and ❤️.\n// {}\n\n/*\n This protocol's authors' copyright notice is:\n\n{}\n*/\n\n",
                    SCANNER_VERSION, PROTO_DATA.nameOriginal, std::string{doc.child("protocol").child("copyright").child_value()});

    scanProtocol(doc);
    generateProtocolHeader(doc);
    if (clientCode) {
        generateClientCodeHeader(doc);
        generateClientCodeSource(doc);
    } else {
        generateServerCodeHeader(doc);
        generateServerCodeSource(doc);
    }
    std::ofstream ofs(outpath + "/" + PROTO_DATA.nameOriginal + "-spec.hpp", std::ios::trunc);
    ofs << COPYRIGHT << HEADER_PROTOCOL;
    ofs.close();

    if (clientCode) {
        ofs = std::ofstream(outpath + "/" + PROTO_DATA.nameOriginal + "-client.hpp", std::ios::trunc);
        ofs << COPYRIGHT << HEADER_IMPL;
        ofs.close();

        ofs = std::ofstream(outpath + "/" + PROTO_DATA.nameOriginal + "-client.cpp", std::ios::trunc);
        ofs << COPYRIGHT << SOURCE;
        ofs.close();
    } else {
        ofs = std::ofstream(outpath + "/" + PROTO_DATA.nameOriginal + "-server.hpp", std::ios::trunc);
        ofs << COPYRIGHT << HEADER_IMPL;
        ofs.close();

        ofs = std::ofstream(outpath + "/" + PROTO_DATA.nameOriginal + "-server.cpp", std::ios::trunc);
        ofs << COPYRIGHT << SOURCE;
        ofs.close();
    }

    return 0;
}