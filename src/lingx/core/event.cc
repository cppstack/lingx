#include <lingx/core/event.h>
#include <lingx/core/module.h>
#include <lingx/core/conf_file.h>
#include <lingx/core/cycle.h>
#include <lingx/core/log.h>
#include <cstddef>

namespace lnx {
namespace {

const uint DEFAULT_CONNECTIONS = 512;

const char* Events_block_(const Conf& cf, const Command& cmd, MConfPtr& conf);

const char* Events_init_conf_(Cycle* cycle, MConf* conf);

MConfPtr Event_core_create_conf_(Cycle* cycle);
const char* Event_core_init_conf_(Cycle* cycle, MConf* conf);

rc_t Event_core_module_init_(Cycle* cycle);

std::vector<Command> Events_commands_ {
    Command {
        "events",
        MAIN_CONF|CONF_BLOCK|CONF_NOARGS,
        Events_block_,
        0
    }
};

CoreModuleCtx Events_module_ctx_ {
    "events",
    nullptr,
    Events_init_conf_
};


std::vector<Command> Event_core_commands_ {
    Command {
        "multi_accept",
        EVENT_CONF|CONF_FLAG,
        Set_flag_slot,
        offsetof(EventConf, multi_accept)
    }
};

EventModuleCtx Event_core_module_ctx_ {
    "event_core",
    Event_core_create_conf_,
    Event_core_init_conf_,
    { nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr}
};

}

/* TODO: move it into epoll module */
bool Use_epoll_rdhup = false;

sig_atomic_t Event_timer_alarm = 0;
int Event_flags = 0;
EventActions  Event_actions;

Module Events_module {
    "lnx_events_module",
    Events_module_ctx_,
    Events_commands_,
    CORE_MODULE,
    nullptr
};

Module Event_core_module {
    "lnx_event_core_module",
    Event_core_module_ctx_,
    Event_core_commands_,
    EVENT_MODULE,
    Event_core_module_init_
};

namespace {

const char* Events_block_(const Conf& cf, const Command&, MConfPtr& conf)
{
    if (conf.get())
        return "is duplicate";

    /* count the number of the event modules and set up their indices */
    uint event_max_module = cf.cycle()->count_modules(EVENT_MODULE);

    std::shared_ptr<MConfs> econfs = std::make_shared<MConfs>();
    econfs->ctxs.resize(event_max_module);

    conf = econfs;

    for (const Module& mod : cf.cycle()->modules()) {
        if (mod.type() != EVENT_MODULE)
            continue;

        const EventModuleCtx& ctx = static_cast<const EventModuleCtx&>(mod.ctx());

        if (ctx.create_conf) {
            econfs->ctxs[mod.ctx_index()] = ctx.create_conf(cf.cycle());
            if (econfs->ctxs[mod.ctx_index()] == nullptr)
                return CONF_ERROR;
        }
    }

    Conf ncf = cf;
    ncf.set_ctxs(&econfs->ctxs);
    ncf.set_module_type(EVENT_MODULE);
    ncf.set_cmd_type(EVENT_CONF);

    const char* rv = ncf.parse("");
    if (rv != CONF_OK)
        return rv;

    for (const Module& mod : cf.cycle()->modules()) {
        if (mod.type() != EVENT_MODULE)
            continue;

        const EventModuleCtx& ctx = static_cast<const EventModuleCtx&>(mod.ctx());

        if (ctx.init_conf) {
            rv = ctx.init_conf(cf.cycle(), econfs->ctxs[mod.ctx_index()].get());
            if (rv != CONF_OK)
                return rv;
        }
    }

    return CONF_OK;
}

const char* Events_init_conf_(Cycle* cycle, MConf*)
{
    if (Get_conf(MConfs, cycle, Events_module) == nullptr) {
        Log_error(cycle->log(), Log::EMERG, 0,
                  "no \"events\" section in configuration");
        return CONF_ERROR;
    }

    return CONF_OK;
}

MConfPtr Event_core_create_conf_(Cycle*)
{
    std::shared_ptr<EventConf> ecf = std::make_shared<EventConf>();

    ecf->connections = UNSET;
    ecf->use = UNSET;
    ecf->multi_accept = UNSET;
    ecf->name = (const char*) UNSET;

    return ecf;
}

const char* Event_core_init_conf_(Cycle* cycle, MConf* conf)
{
    EventConf* ecf = static_cast<EventConf*>(conf);

    const Module* module = nullptr;

    /* TODO: select a default one */

    if (module == nullptr) {
        for (const Module& mod : cycle->modules()) {
            if (mod.type() != EVENT_MODULE)
                continue;

            const EventModuleCtx& ctx = static_cast<const EventModuleCtx&>(mod.ctx());
            if (ctx.name == Event_core_module_ctx_.name)
                continue;

            module = &mod;
            break;
        }
    }

    if (module == nullptr) {
        Log_error(cycle->log(), Log::EMERG, 0, "no events module found");
        return CONF_ERROR;
    }

    Conf_init_value(ecf->connections, DEFAULT_CONNECTIONS);
    cycle->set_connection_n(ecf->connections);

    Conf_init_value(ecf->use, module->ctx_index());

    const EventModuleCtx& ctx = static_cast<const EventModuleCtx&>(module->ctx());
    Conf_init_value(ecf->name, ctx.name.data());

    Conf_init_value(ecf->multi_accept, OFF);

    return CONF_OK;
}

rc_t Event_core_module_init_(Cycle* cycle)
{
    std::shared_ptr<CoreConf> ccf = Get_conf(CoreConf, cycle, Core_module);
    if (!ccf->master)
        return OK;

    return OK;
}

}

}
