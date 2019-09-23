#ifndef _GPD_XS_DYNAMIC_SERVICE_DESC
#define _GPD_XS_DYNAMIC_SERVICE_DESC

#include "perl_unpollute.h"

#include <upb/def.h>

#include <string>
#include <vector>

// uPB does not have service_def/method_def yet, and adding it here
// is much quicker than implementing it in uPB
namespace gpd {
    struct MethodDef;
    struct ServiceDef;

    struct ServiceDefPtr {
        ServiceDefPtr(ServiceDef *_ptr) : ptr_(_ptr) {}

        ServiceDef *ptr() { return ptr_; }

        void add_method(const MethodDef &method);

    private:
        ServiceDef *ptr_;
    };

    struct MethodDef {
        MethodDef(const std::string &name,
                  const std::string &full_name,
                  const ServiceDefPtr containing_service,
                  const upb::MessageDefPtr input_type,
                  const upb::MessageDefPtr output_type,
                  bool client_streaming,
                  bool server_streaming) :
            _name(name),
            _full_name(full_name),
            _containing_service(containing_service),
            _input_type(input_type),
            _output_type(output_type),
            _client_streaming(client_streaming),
            _server_streaming(server_streaming)
        { }

        const std::string &name() const { return _name; }
        const std::string &full_name() const { return _full_name; }
        const ServiceDefPtr containing_service() const { return _containing_service; }
        const upb::MessageDefPtr input_type() const { return _input_type; }
        const upb::MessageDefPtr output_type() const { return _output_type; }
        bool client_streaming() const { return _client_streaming; }
        bool server_streaming() const { return _server_streaming; }

    private:
        std::string _name;
        std::string _full_name;

        const ServiceDefPtr _containing_service;
        const upb::MessageDefPtr _input_type;
        const upb::MessageDefPtr _output_type;

        bool _client_streaming, _server_streaming;
    };

    struct MethodDefPtr {
        MethodDef *ptr() { return ptr_; }

    private:
        MethodDef *ptr_;
    };

    struct ServiceDef {
        ServiceDef(const std::string &full_name) :
            _full_name(full_name)
        { }

        void add_method(const MethodDef &method) {
            _methods.push_back(method);
        }

        const std::string &full_name() const { return _full_name; }
        const std::vector<MethodDef> &methods() { return _methods; }

    private:
        std::string _full_name;
        std::vector<MethodDef> _methods;
    };

    inline void ServiceDefPtr::add_method(const MethodDef &method) {
        ptr_->add_method(method);
    }
}

#endif
