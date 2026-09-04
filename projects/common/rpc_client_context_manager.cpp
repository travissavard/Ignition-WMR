#include "rpc_interfaces.h"
#include <iostream>

// --- ClientContextManager ---

ClientContextManager::ClientContextManager(vr::IVRDriverContext *real_context) : RpcObject(), real_context_(real_context)
{
    if (!IsProxy()) { // This is the real object on the client side
        this->RegisterFunction(RPCFunction_ClientContextManager_GetGenericInterface, [this](const auto& args) {
            std::string interfaceVersion = args[0].asString();
            const char* pchInterfaceVersion = interfaceVersion.c_str();
            
            if (interface_cache_.count(pchInterfaceVersion)) {
                return RpcValue(static_cast<RpcObject*>(interface_cache_.at(pchInterfaceVersion)));
            }
            
            vr::EVRInitError err = vr::VRInitError_None;
            void* real_interface = this->GetGenericInterface(pchInterfaceVersion, &err);

            std::cout << "ClientContextManager: Wrapping interface " << pchInterfaceVersion << std::endl;

            if (!real_interface && err != vr::VRInitError_None) {
                return RpcValue(static_cast<RpcObject*>(nullptr));
            }

            RpcObject* rpc_wrapper = nullptr;
            std::string interface_str = pchInterfaceVersion;

            if (interface_str == vr::IVRServerDriverHost_Version) {
                rpc_wrapper = new RpcDriverHost(static_cast<vr::IVRServerDriverHost*>(real_interface));
            } else if (interface_str == vr::IVRDriverLog_Version) {
                rpc_wrapper = new RpcDriverLog(static_cast<vr::IVRDriverLog*>(real_interface));
            } else if (interface_str == vr::IVRSettings_Version) {
                rpc_wrapper = new RpcSettings(static_cast<vr::IVRSettings*>(real_interface));
            } else if (interface_str == "IVRDriverInput_003" || interface_str == vr::IVRDriverInput_Version) {
                rpc_wrapper = new RpcDriverInput(static_cast<vr::IVRDriverInput*>(real_interface));
            } else if (interface_str == vr::IVRDriverManager_Version) {
                rpc_wrapper = new RpcDriverManager(static_cast<vr::IVRDriverManager*>(real_interface));
            } else if (interface_str == vr::IVRProperties_Version) {
                rpc_wrapper = new RpcProperties(static_cast<vr::IVRProperties*>(real_interface));
            } else if (interface_str == vr::IVRResources_Version) {
                rpc_wrapper = new RpcResources(static_cast<vr::IVRResources*>(real_interface));
            } else if (interface_str == vr::IVRPaths_Version) {
                rpc_wrapper = new RpcPaths(static_cast<vr::IVRPaths*>(real_interface));
            } else if (interface_str == vr::IVRBlockQueue_Version) {
                rpc_wrapper = new RpcBlockQueue(static_cast<vr::IVRBlockQueue*>(real_interface));
            } else {
                // If we don't have a specific wrapper, we can't vend it.
                return RpcValue(static_cast<RpcObject*>(nullptr));
            }

            if (rpc_wrapper) {
                interface_cache_[interface_str] = rpc_wrapper;
                return RpcValue(static_cast<RpcObject*>(rpc_wrapper));
            }

            // If we don't have a wrapper, we can't return it over RPC.
            // For now, we'll return nullptr and an error.
            return RpcValue(static_cast<RpcObject*>(nullptr));
        });

        this->RegisterFunction(RPCFunction_ClientContextManager_GetDriverHandleContext, [this](const auto& args) {
            // vr::DriverHandle_t is a uint64_t
            return RpcValue(this->GetDriverHandle());
        });
    }
}

ClientContextManager::ClientContextManager(RpcObjectId id) : RpcObject(id) {}

ClientContextManager::~ClientContextManager() {
    // Clean up cached wrappers
    for(auto const& [key, val] : interface_cache_) {
        delete val;
    }
}

RpcClassEnum ClientContextManager::GetRpcClassId() const {
    return RPCClassClientContextManager;
}

void* ClientContextManager::GetGenericInterface(const char *pchInterfaceVersion, vr::EVRInitError *peError) {
    if (IsProxy()) {
        std::cout << "ClientContextManager: Getting interface " << pchInterfaceVersion << std::endl;

        // Server-side proxy implementation
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_ClientContextManager_GetGenericInterface, RpcValue(std::string(pchInterfaceVersion)));
        if (result.isObject()) {
            std::cout << "ClientContextManager: Id: " << result.asObject()->GetId() << std::endl;
            RpcObject* obj = result.asObject();
            if (!obj) {
                if (peError) *peError = vr::VRInitError_Init_InterfaceNotFound;
                return nullptr;
            }

            if (peError) *peError = vr::VRInitError_None;

            // Casting to void* and then back is dangerous with multiple inheritance.
            // We must cast to the correct interface type here to ensure the vtable is correct.
            // dynamic_cast is the safe way to do this.
            std::string interface_str = pchInterfaceVersion;
            if (interface_str == vr::IVRServerDriverHost_Version) {
                return dynamic_cast<vr::IVRServerDriverHost*>(obj);
            } else if (interface_str == vr::IVRDriverLog_Version) {
                return dynamic_cast<vr::IVRDriverLog*>(obj);
            } else if (interface_str == vr::IVRSettings_Version) {
                return dynamic_cast<vr::IVRSettings*>(obj);
            } else if (interface_str == "IVRDriverInput_003" || interface_str == vr::IVRDriverInput_Version) {
                return dynamic_cast<vr::IVRDriverInput*>(obj);
            } else if (interface_str == vr::IVRDriverManager_Version) {
                return dynamic_cast<vr::IVRDriverManager*>(obj);
            } else if (interface_str == vr::IVRProperties_Version) {
                return dynamic_cast<vr::IVRProperties*>(obj);
            } else if (interface_str == vr::IVRResources_Version) {
                return dynamic_cast<vr::IVRResources*>(obj);
            } else if (interface_str == vr::IVRPaths_Version) {
                return dynamic_cast<vr::IVRPaths*>(obj);
            } else if (interface_str == vr::IVRBlockQueue_Version) {
                return dynamic_cast<vr::IVRBlockQueue*>(obj);
            }

            // If we don't know the interface, we can't safely cast it.
            if (peError) *peError = vr::VRInitError_Init_InterfaceNotFound;
            return nullptr;
        } else {
            if (peError) *peError = vr::VRInitError_Init_InterfaceNotFound;
            return nullptr;
        }
    } else {
        return real_context_->GetGenericInterface(pchInterfaceVersion, peError);
    }
}

vr::DriverHandle_t ClientContextManager::GetDriverHandle() {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_ClientContextManager_GetDriverHandleContext);
        return result.asUint64();
    } else {
        return real_context_->GetDriverHandle();
    }
}
