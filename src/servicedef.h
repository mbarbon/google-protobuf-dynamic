#ifndef _GPD_XS_DYNAMIC_SERVICE_DESC
#define _GPD_XS_DYNAMIC_SERVICE_DESC

#undef New
#undef Move

#include <upb/def.h>

#include <string>
#include <vector>

// uPB does not have service_def/method_def yet, and adding it here
// is much quicker than implementing it in uPB
namespace gpd {
    struct ServiceDef;

    struct MethodDef {
        MethodDef(const std::string &_name,
                  const std::string &_full_name,
                  const ServiceDef *_containing_service,
                  const upb::MessageDef *_input_type,
                  const upb::MessageDef *_output_type,
                  bool _client_streaming,
                  bool _server_streaming) :
            name(_name),
            full_name(_full_name),
            containing_service(_containing_service),
            input_type(_input_type),
            output_type(_output_type),
            client_streaming(_client_streaming),
            server_streaming(_server_streaming)
        { }

        const std::string name;
        const std::string full_name;

        const ServiceDef *containing_service;
        const upb::MessageDef *input_type;
        const upb::MessageDef *output_type;

        const bool client_streaming, server_streaming;
    };

    struct ServiceDef {
        ServiceDef(const std::string &_full_name) :
            full_name(_full_name)
        { }

        const std::string full_name;
        std::vector<MethodDef> methods;
    };
}

#endif
