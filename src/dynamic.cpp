#include "dynamic.h"
#include "mapper.h"

#include <sstream>
#include <google/protobuf/dynamic_message.h>

using namespace gpd;
using namespace std;
using namespace STD_TR1;
using namespace google::protobuf;
using namespace upb;
using namespace upb::googlepb;

#if PERL_VERSION < 10
    #undef  newCONSTSUB
    #define newCONSTSUB(a, b,c) Perl_newCONSTSUB(aTHX_ a, const_cast<char *>(b), c)

    #undef newXS
    #define newXS(a, b, c) Perl_newXS(aTHX_ const_cast<char *>(a), b, const_cast<char *>(c))
#endif

void Dynamic::CollectErrors::AddError(const string &filename, int line, int column, const string &message) {
    croak("Error during protobuf parsing: %s:%d:%d: %s", filename.c_str(), line, column, message.c_str());
}

MappingOptions::MappingOptions(pTHX_ SV *options_ref) :
        use_bigints(sizeof(IV) < sizeof(int64_t)),
        check_required_fields(true),
        explicit_defaults(false),
        encode_defaults(false),
        check_enum_values(true),
        generic_extension_methods(true),
        implicit_maps(false),
        accessor_style(GetAndSet) {
    if (options_ref == NULL || !SvOK(options_ref))
        return;
    if (!SvROK(options_ref) || SvTYPE(SvRV(options_ref)) != SVt_PVHV)
        croak("options must be an hash reference");
    HV *options = (HV *) SvRV(options_ref);

#define BOOLEAN_OPTION(field, name) { \
        SV **value = hv_fetchs(options, #name, 0); \
        if (value) \
            field = SvTRUE(*value); \
    }

    BOOLEAN_OPTION(use_bigints, use_bigints);
    BOOLEAN_OPTION(check_required_fields, check_required_fields);
    BOOLEAN_OPTION(explicit_defaults, explicit_defaults);
    BOOLEAN_OPTION(encode_defaults, encode_defaults);
    BOOLEAN_OPTION(check_enum_values, check_enum_values);
    BOOLEAN_OPTION(generic_extension_methods, generic_extension_methods);
    BOOLEAN_OPTION(implicit_maps, implicit_maps);

    if (SV **value = hv_fetchs(options, "accessor_style", 0)) {
        const char *buf = SvPV_nolen(*value);

        if (strEQ(buf, "get_and_set"))
            accessor_style = GetAndSet;
        else if (strEQ(buf, "plain_and_set"))
            accessor_style = PlainAndSet;
        else if (strEQ(buf, "single_accessor"))
            accessor_style = SingleAccessor;
        else if (strEQ(buf, "plain"))
            accessor_style = Plain;
        else
            croak("Invalid value '%s' for 'accessor_style' option", buf);
    }

#undef BOOLEAN_OPTION
}

Dynamic::Dynamic(const string &root_directory) :
        overlay_source_tree(&memory_source_tree, &disk_source_tree),
        descriptor_loader(&overlay_source_tree, &die_on_error) {
    if (!root_directory.empty())
        disk_source_tree.MapPath("", root_directory);
}

Dynamic::~Dynamic() {
}

void Dynamic::load_file(pTHX_ const string &file) {
    const FileDescriptor *loaded = descriptor_loader.load_proto(file);

    if (loaded)
        files.insert(loaded);
}

void Dynamic::load_string(pTHX_ const string &file, SV *sv) {
    STRLEN len;
    const char *data = SvPV(sv, len);
    string actual_file = file.empty() ? "<string>" : file;

    memory_source_tree.AddFile(actual_file, data, len);
    load_file(aTHX_ actual_file);
}

void Dynamic::load_serialized_string(pTHX_ SV *sv) {
    STRLEN len;
    const char *data = SvPV(sv, len);
    const vector<const FileDescriptor *> loaded = descriptor_loader.load_serialized(data, len);

    files.insert(loaded.begin(), loaded.end());
}

namespace {
    int free_refcounted(pTHX_ SV *sv, MAGIC *mg) {
        Refcounted *refcounted = (Refcounted *) mg->mg_ptr;

        refcounted->unref();

        return 0;
    }

    MGVTBL manage_refcounted = {
        NULL, // get
        NULL, // set
        NULL, // len
        NULL, // clear
        free_refcounted,
        NULL, // copy
        NULL, // dup
        NULL, // local
    };

    void copy_and_bind(pTHX_ const char *name, const char *target, const string &perl_package, Refcounted *refcounted) {
        static const char prefix[] = "Google::ProtocolBuffers::Dynamic::Mapper::";
        size_t length = strlen(name);
        char buffer[sizeof(prefix) + length + 1];

        memcpy(buffer, prefix, sizeof(prefix));
        strcpy(buffer + sizeof(prefix) - 1, name);

        CV *src = get_cv(buffer, 0);
        CV *new_xs = newXS((perl_package + "::" + target).c_str(), CvXSUB(src), __FILE__);

        CvXSUBANY(new_xs).any_ptr = refcounted;
        sv_magicext((SV *) new_xs, NULL,
                    PERL_MAGIC_ext, &manage_refcounted,
                    (const char *) refcounted, 0);
        refcounted->ref();
    }

    void copy_and_bind(pTHX_ const char *name, const string &perl_package, Refcounted *refcounted) {
        copy_and_bind(aTHX_ name, name, perl_package, refcounted);
    }

    void copy_and_bind(pTHX_ const char *name, const char *prefix, const char *suffix, const string &perl_package, Mapper *mapper) {
        copy_and_bind(aTHX_ name, (string(prefix) + suffix).c_str(), perl_package, mapper);
    }

    void copy_and_bind_field(pTHX_ const char *name, const string &name_prefix, const string &name_suffix, const string &perl_package, MapperField *mapperfield) {
        string temp_name = name_prefix + mapperfield->name() + name_suffix;

        if (mapperfield->is_extension()) {
            for (int i = 0, max = temp_name.size(); i < max; ++i)
                temp_name[i] = temp_name[i] == '.' ? '_' : tolower(temp_name[i]);
        }

        copy_and_bind(aTHX_ name, temp_name.c_str(), perl_package, mapperfield);
    }

    bool is_map_entry(const MessageDef *message, bool check_implicit_map) {
        if (message->mapentry())
            return true;
        if (!check_implicit_map)
            return false;
        const FieldDef *key = message->FindFieldByNumber(1);
        const FieldDef *value = message->FindFieldByNumber(2);
        const char *msg_name = message->name();
        if (message->field_count() != 2 || message->oneof_count() != 0 ||
                key == NULL || value == NULL)
            return false;
        if (strcmp(key->name(), "key") != 0 || strcmp(value->name(), "value") != 0)
            return false;
        size_t msg_name_len = strlen(msg_name);
        if (msg_name_len < 6 || strcmp(msg_name + msg_name_len - 5, "Entry") != 0)
            return false;
        if (key->IsSequence() || value->IsSequence() ||
                key->is_extension() || value->is_extension() ||
                key->subdef() /* message or enum */)
            return false;
        return true;
    }
}

void Dynamic::map_message(pTHX_ const string &message, const string &perl_package, const MappingOptions &options) {
    const DescriptorPool *pool = descriptor_loader.pool();
    const Descriptor *descriptor = pool->FindMessageTypeByName(message);

    if (descriptor == NULL) {
        croak("Unable to find a descriptor for message '%s'", message.c_str());
    }

    map_message_recursive(aTHX_ descriptor, perl_package, options);
}

void Dynamic::map_package(pTHX_ const string &pb_package, const string &perl_package_prefix, const MappingOptions &options) {
    map_package_or_prefix(aTHX_ pb_package, false, perl_package_prefix, options);
}

void Dynamic::map_package_prefix(pTHX_ const string &pb_prefix, const string &perl_package_prefix, const MappingOptions &options) {
    map_package_or_prefix(aTHX_ pb_prefix, true, perl_package_prefix, options);
}

void Dynamic::map_package_or_prefix(pTHX_ const string &pb_package_or_prefix, bool is_prefix, const string &perl_package_prefix, const MappingOptions &options) {
    string prefix_and_dot = pb_package_or_prefix + ".";

    for (unordered_set<const FileDescriptor *>::iterator it = files.begin(), en = files.end(); it != en; ++it) {
        const FileDescriptor *file = *it;
        const string &file_package = file->package();
        bool is_exact = false;

        if (pb_package_or_prefix == file_package) {
            is_exact = true;
        } else if (is_prefix) {

            if (file_package.length() <= prefix_and_dot.length() ||
                    strncmp(file_package.data(), prefix_and_dot.data(), prefix_and_dot.length()) != 0)
                continue;
        } else {
            continue;
        }

        string perl_package = perl_package_prefix;
        if (!is_exact) {
            perl_package += "::";

            for (string::const_iterator it = file_package.begin() + prefix_and_dot.length(), en = file_package.end(); it != en; ++it) {
                if (*it == '.')
                    perl_package += "::";
                else
                    perl_package += *it;
            }
        }

        for (int i = 0, max = file->message_type_count(); i < max; ++i) {
            const Descriptor *descriptor = file->message_type(i);
            if (descriptor_map.find(descriptor->full_name()) != descriptor_map.end())
                continue;

            map_message_recursive(aTHX_ descriptor, perl_package + "::" + descriptor->name(), options);
        }

        for (int i = 0, max = file->enum_type_count(); i < max; ++i) {
            const EnumDescriptor *descriptor = file->enum_type(i);
            if (mapped_enums.find(descriptor->full_name()) != mapped_enums.end())
                continue;

            map_enum(aTHX_ descriptor, perl_package + "::" + descriptor->name(), options);
        }
    }
}

void Dynamic::map_enum(pTHX_ const string &enum_name, const string &perl_package, const MappingOptions &options) {
    const DescriptorPool *pool = descriptor_loader.pool();
    const EnumDescriptor *descriptor = pool->FindEnumTypeByName(enum_name);

    if (descriptor == NULL) {
        croak("Unable to find a descriptor for enum '%s'", enum_name.c_str());
    }

    map_enum(aTHX_ descriptor, perl_package, options);
}

void Dynamic::map_message_recursive(pTHX_ const Descriptor *descriptor, const string &perl_package, const MappingOptions &options) {
    for (int i = 0, max = descriptor->nested_type_count(); i < max; ++i) {
        const Descriptor *inner = descriptor->nested_type(i);
        if (descriptor_map.find(inner->full_name()) != descriptor_map.end())
            continue;

        map_message_recursive(aTHX_ inner, perl_package + "::" + inner->name(), options);
    }

    for (int i = 0, max = descriptor->enum_type_count(); i < max; ++i) {
        const EnumDescriptor *inner = descriptor->enum_type(i);
        if (mapped_enums.find(inner->full_name()) != mapped_enums.end())
            continue;

        map_enum(aTHX_ inner, perl_package + "::" + inner->name(), options);
    }

    map_message(aTHX_ descriptor, perl_package, options);
}

void Dynamic::check_package(pTHX_ const string &perl_package, const string &pb_name) {
    if (used_packages.find(perl_package) == used_packages.end())
        return;

    croak("Package '%s' has already been used in a mapping", perl_package.c_str());
}

void Dynamic::map_message(pTHX_ const Descriptor *descriptor, const string &perl_package, const MappingOptions &options) {
    check_package(aTHX_ perl_package, descriptor->full_name());
    if (descriptor_map.find(descriptor->full_name()) != descriptor_map.end())
        croak("Message '%s' has already been mapped", descriptor->full_name().c_str());
    if (options.use_bigints)
        eval_pv("require Math::BigInt", 1);
    HV *stash = gv_stashpvn(perl_package.data(), perl_package.size(), GV_ADD);
    const MessageDef *message_def = def_builder.GetMessageDef(descriptor);
    if (is_map_entry(message_def, options.implicit_maps))
        // it's likely I will regret this const_cast<>
        upb_msgdef_setmapentry(const_cast<MessageDef *>(message_def), true);
    Mapper *mapper = new Mapper(aTHX_ this, message_def, stash, options);
    const char *getter_prefix, *setter_prefix;
    bool plain_accessor = false;

    if (options.accessor_style == MappingOptions::SingleAccessor) {
        getter_prefix = setter_prefix = NULL;
    } else if (options.accessor_style == MappingOptions::Plain) {
        getter_prefix = setter_prefix = NULL;
        plain_accessor = true;
    } else {
        getter_prefix = options.accessor_style == MappingOptions::GetAndSet ?
            "get_" : "";
        setter_prefix = "set_";
    }

    descriptor_map[message_def->full_name()] = mapper;
    used_packages.insert(perl_package);
    pending.push_back(mapper);

    copy_and_bind(aTHX_ "decode", perl_package, mapper);
    copy_and_bind(aTHX_ "encode", perl_package, mapper);
    copy_and_bind(aTHX_ "decode_json", perl_package, mapper);
    copy_and_bind(aTHX_ "encode_json", perl_package, mapper);
    copy_and_bind(aTHX_ "new", perl_package, mapper);
    copy_and_bind(aTHX_ "new_and_check", perl_package, mapper);
    copy_and_bind(aTHX_ "message_descriptor", perl_package, mapper);

    if (options.generic_extension_methods) {
        copy_and_bind(aTHX_ "has_extension_field", "has_extension", perl_package, mapper);
        copy_and_bind(aTHX_ "clear_extension_field", "clear_extension", perl_package, mapper);
        copy_and_bind(aTHX_ "add_extension_item", perl_package, mapper);
        copy_and_bind(aTHX_ "extension_list_size", "extension_size", perl_package, mapper);
        if (getter_prefix) {
            copy_and_bind(aTHX_ "get_extension_scalar", getter_prefix, "extension", perl_package, mapper);
            copy_and_bind(aTHX_ "set_extension_scalar", setter_prefix, "extension", perl_package, mapper);
            copy_and_bind(aTHX_ "get_extension_item", getter_prefix, "extension_item", perl_package, mapper);
            copy_and_bind(aTHX_ "set_extension_item", setter_prefix, "extension_item", perl_package, mapper);
            copy_and_bind(aTHX_ "get_extension_list", getter_prefix, "extension_list", perl_package, mapper);
            copy_and_bind(aTHX_ "set_extension_list", setter_prefix, "extension_list", perl_package, mapper);
        } else {
            copy_and_bind(aTHX_ "get_or_set_extension_scalar", "extension", perl_package, mapper);
            copy_and_bind(aTHX_ "get_or_set_extension_item", "extension_item", perl_package, mapper);
            copy_and_bind(aTHX_ "get_or_set_extension_list", "extension_list", perl_package, mapper);
        }
    }

    for (int i = 0, max = mapper->field_count(); i < max; ++i) {
        const Mapper::Field *field = mapper->get_field(i);
        MapperField *mapperfield = new MapperField(aTHX_ mapper, field);

        {
            string upper_field;

            for (const char *field_name = field->field_def->name(); *field_name; ++field_name)
                upper_field.push_back(*field_name == '.' ? '_' : toupper(*field_name));

            newCONSTSUB(stash, (upper_field + "_FIELD_NUMBER").c_str(),
                        newSVuv(field->field_def->number()));

            if (field->field_def->is_extension()) {
                string temp = string() + "[" + field->field_def->full_name() + "]";

                newCONSTSUB(stash, (upper_field + "_KEY").c_str(),
                            newSVpvn_share(temp.data(), temp.size(), 0));
            }
        }

        copy_and_bind_field(aTHX_ "clear_field", "clear_", "", perl_package, mapperfield);
        if (mapperfield->is_map()) {
            if (plain_accessor) {
                copy_and_bind_field(aTHX_ "get_or_set_map", "", "", perl_package, mapperfield);
            } else if (getter_prefix) {
                copy_and_bind_field(aTHX_ "get_map_item", getter_prefix, "", perl_package, mapperfield);
                copy_and_bind_field(aTHX_ "set_map_item", setter_prefix, "", perl_package, mapperfield);
                copy_and_bind_field(aTHX_ "get_map", getter_prefix, "_map", perl_package, mapperfield);
                copy_and_bind_field(aTHX_ "set_map", setter_prefix, "_map", perl_package, mapperfield);
            } else {
                copy_and_bind_field(aTHX_ "get_or_set_map_item", "", "", perl_package, mapperfield);
                copy_and_bind_field(aTHX_ "get_or_set_map", "", "_map", perl_package, mapperfield);
            }
        } else if (mapperfield->is_repeated()) {
            copy_and_bind_field(aTHX_ "add_item", "add_", "", perl_package, mapperfield);
            copy_and_bind_field(aTHX_ "list_size", "", "_size", perl_package, mapperfield);
            if (plain_accessor) {
                copy_and_bind_field(aTHX_ "get_or_set_list", "", "", perl_package, mapperfield);
            } else if (getter_prefix) {
                copy_and_bind_field(aTHX_ "get_list_item", getter_prefix, "", perl_package, mapperfield);
                copy_and_bind_field(aTHX_ "set_list_item", setter_prefix, "", perl_package, mapperfield);
                copy_and_bind_field(aTHX_ "get_list", getter_prefix, "_list", perl_package, mapperfield);
                copy_and_bind_field(aTHX_ "set_list", setter_prefix, "_list", perl_package, mapperfield);
            } else {
                copy_and_bind_field(aTHX_ "get_or_set_list_item", "", "", perl_package, mapperfield);
                copy_and_bind_field(aTHX_ "get_or_set_list", "", "_list", perl_package, mapperfield);
            }
        } else {
            copy_and_bind_field(aTHX_ "has_field", "has_", "", perl_package, mapperfield);
            if (getter_prefix) {
                copy_and_bind_field(aTHX_ "get_scalar", getter_prefix, "", perl_package, mapperfield);
                copy_and_bind_field(aTHX_ "set_scalar", setter_prefix, "", perl_package, mapperfield);
            } else {
                copy_and_bind_field(aTHX_ "get_or_set_scalar", "", "", perl_package, mapperfield);
            }
        }

        mapperfield->unref();
    }

    mapper->unref(); // reference from constructor
}

void Dynamic::map_enum(pTHX_ const EnumDescriptor *descriptor, const string &perl_package, const MappingOptions &options) {
    check_package(aTHX_ perl_package, descriptor->full_name());
    if (mapped_enums.find(descriptor->full_name()) != mapped_enums.end())
        croak("Message '%s' has already been mapped", descriptor->full_name().c_str());

    const EnumDef *enum_def = def_builder.GetEnumDef(descriptor);
    EnumMapper *mapper = new EnumMapper(aTHX_ this, enum_def);

    mapped_enums.insert(descriptor->full_name());
    used_packages.insert(perl_package);

    HV *stash = gv_stashpvn(perl_package.data(), perl_package.size(), GV_ADD);

    copy_and_bind(aTHX_ "enum_descriptor", perl_package, mapper);

    for (int i = 0, max = descriptor->value_count(); i < max; ++i) {
        const EnumValueDescriptor *value = descriptor->value(i);

        newCONSTSUB(stash, value->name().c_str(),
                    newSVuv(value->number()));

        stringstream en;
        en << value->number();
        en << "_ENUM";
        const std::string tmp = en.str();
        newCONSTSUB(stash, tmp.c_str(),
                    newSVpv(value->name().c_str(),0));
    }
}

void Dynamic::resolve_references() {
    for (std::vector<Mapper *>::iterator it = pending.begin(), en = pending.end(); it != en; ++it)
        (*it)->resolve_mappers();
    for (std::vector<Mapper *>::iterator it = pending.begin(), en = pending.end(); it != en; ++it)
        (*it)->create_encoder_decoder();
    pending.clear();
}

const Mapper *Dynamic::find_mapper(const MessageDef *message_def) const {
    unordered_map<string, const Mapper *>::const_iterator item = descriptor_map.find(message_def->full_name());

    if (item == descriptor_map.end())
        croak("Unknown type '%s'", message_def->full_name());

    return item->second;
}
