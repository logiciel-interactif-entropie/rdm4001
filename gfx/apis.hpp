#pragma once

#include <functional>
#include <stdexcept>
#include <string>

#include "base_context.hpp"
#include "base_device.hpp"
namespace rdm::gfx {
class ApiFactory {
 public:
  struct ApiReg {
    std::function<BaseDevice*(BaseContext*)> createDevice;
    std::function<BaseContext*(void*)> createContext;
    std::function<int()> prepareSdl;
  };

  bool valid(const char* nam) { return (regs.find(nam) != regs.end()); }
  const char* getDefault() { return regs.begin()->first.c_str(); }
  bool platformSupportsGraphics() { return regs.size(); }

  void registerFunctions(const char* nam, ApiReg regs) {
    this->regs[nam] = regs;
  }
  ApiReg getFunctions(const char* nam) {
    if (regs.find(nam) == regs.end()) {
      throw std::runtime_error("Could not find graphics API");
    }
    return regs[nam];
  };

  void printSupportedApis();

  static ApiFactory* singleton();

 private:
  std::map<std::string, ApiReg> regs;
};

template <typename Context, typename Device>
class ApiInstantiator {
 public:
  ApiInstantiator(const char* nam) {
    ApiFactory::ApiReg reg;
    reg.createDevice = [](BaseContext* C) {
      return (BaseDevice*)(new Device(dynamic_cast<Context*>(C)));
    };
    reg.createContext = [](void* hwnd) {
      return (BaseContext*)(new Context(hwnd));
    };
    reg.prepareSdl = []() { return Context::prepareSdl(); };
    ApiFactory::singleton()->registerFunctions(nam, reg);
  }
};

#define GFX_API_INSTANTIATOR(Name, Context, Device)                        \
  static rdm::gfx::ApiInstantiator<Context, Device> __API_FACTORY__##Name( \
      #Name);

};  // namespace rdm::gfx
