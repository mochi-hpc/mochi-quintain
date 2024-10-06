/*
 * (C) 2024 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "quintain-server.h"
#include <bedrock/AbstractComponent.hpp>

namespace tl = thallium;

class QuintainComponent : public bedrock::AbstractComponent {

    quintain_provider_t m_provider = nullptr;

    public:

    QuintainComponent(const tl::engine& engine,
                      uint16_t  provider_id,
                      const std::string& config,
                      const tl::pool& pool)
    {
        quintain_provider_init_info qargs = {
            /* .json_config = */ config.c_str(),
            /* .rpc_pool = */ pool.native_handle()
        };
        int ret = quintain_provider_register(
                engine.get_margo_instance(),
                provider_id,
                &qargs,
                &m_provider);
        if(ret != 0) {
            throw bedrock::Exception{
                "Could not create Quintain provider: quintain_provider_register returned {}", ret};
        }
    }

    ~QuintainComponent() {
        quintain_provider_deregister(m_provider);
    }

    void* getHandle() override {
        return static_cast<void*>(m_provider);
    }

    std::string getConfig() override {
        auto config_cstr = quintain_provider_get_config(m_provider);
        auto config = std::string{config_cstr};
        free(config_cstr);
        return config;
    }

    static std::shared_ptr<bedrock::AbstractComponent>
        Register(const bedrock::ComponentArgs& args) {
            tl::pool pool;
            auto it = args.dependencies.find("pool");
            if(it != args.dependencies.end() && !it->second.empty()) {
                pool = it->second[0]->getHandle<tl::pool>();
            }
            return std::make_shared<QuintainComponent>(
                args.engine, args.provider_id, args.config, pool);
        }

    static std::vector<bedrock::Dependency>
        GetDependencies(const bedrock::ComponentArgs& args) {
            (void)args;
            std::vector<bedrock::Dependency> dependencies{
                bedrock::Dependency{
                    /* name */ "pool",
                    /* type */ "pool",
                    /* is_required */ false,
                    /* is_array */ false,
                    /* is_updatable */ false
                }
            };
            return dependencies;
        }
};

BEDROCK_REGISTER_COMPONENT_TYPE(quintain, QuintainComponent)
