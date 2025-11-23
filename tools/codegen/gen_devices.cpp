#include <yaml-cpp/yaml.h>

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

[[noreturn]] void Fail(const std::string& message) {
    throw std::runtime_error(message);
}

void ExpectKeys(const YAML::Node& node, std::string_view context,
                std::initializer_list<std::string_view> allowed) {
    std::unordered_set<std::string_view> allowed_set(allowed.begin(), allowed.end());
    for (const auto& kv : node) {
        if (!kv.first.IsScalar()) {
            std::ostringstream oss;
            oss << "Non-scalar key encountered in " << context;
            Fail(oss.str());
        }
        const std::string key = kv.first.as<std::string>();
        if (!allowed_set.count(key)) {
            std::ostringstream oss;
            oss << "Unknown key '" << key << "' in " << context;
            Fail(oss.str());
        }
    }
}

std::string EscapeStringLiteral(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 4);
    for (char ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += ch;
            break;
        }
    }
    return escaped;
}

bool LooksLikeIdentifier(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    if (!(std::isalpha(static_cast<unsigned char>(value.front())) || value.front() == '_')) {
        return false;
    }
    for (char ch : value) {
        if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch == '_')) {
            return false;
        }
    }
    return true;
}

std::string ReadRequiredString(const YAML::Node& node, std::string_view key, std::string_view context) {
    const auto value = node[key];
    if (!value) {
        std::ostringstream oss;
        oss << "Missing required key '" << key << "' in " << context;
        Fail(oss.str());
    }
    if (!value.IsScalar()) {
        std::ostringstream oss;
        oss << "Key '" << key << "' must be a scalar in " << context;
        Fail(oss.str());
    }
    const std::string result = value.as<std::string>();
    if (result.empty()) {
        std::ostringstream oss;
        oss << "Key '" << key << "' must not be empty in " << context;
        Fail(oss.str());
    }
    return result;
}

std::optional<std::string> ReadOptionalString(const YAML::Node& node, std::string_view key, std::string_view context) {
    const auto value = node[key];
    if (!value) {
        return std::nullopt;
    }
    if (!value.IsScalar()) {
        std::ostringstream oss;
        oss << "Key '" << key << "' must be a scalar in " << context;
        Fail(oss.str());
    }
    return value.as<std::string>();
}

std::uint64_t ReadRequiredUint64(const YAML::Node& node, std::string_view key, std::string_view context) {
    const auto value = node[key];
    if (!value) {
        std::ostringstream oss;
        oss << "Missing required key '" << key << "' in " << context;
        Fail(oss.str());
    }
    if (!value.IsScalar()) {
        std::ostringstream oss;
        oss << "Key '" << key << "' must be a scalar in " << context;
        Fail(oss.str());
    }
    const auto raw = value.as<std::string>();
    try {
        auto parsed = static_cast<std::uint64_t>(std::stoull(raw));
        return parsed;
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "Failed to parse uint64 '" << key << "' in " << context << ": " << e.what();
        Fail(oss.str());
    }
}

std::uint64_t ReadOptionalUint64(const YAML::Node& node, std::string_view key, std::string_view context,
                                 std::uint64_t default_value) {
    const auto value = node[key];
    if (!value) {
        return default_value;
    }
    if (!value.IsScalar()) {
        std::ostringstream oss;
        oss << "Key '" << key << "' must be a scalar in " << context;
        Fail(oss.str());
    }
    const auto raw = value.as<std::string>();
    try {
        auto parsed = static_cast<std::uint64_t>(std::stoull(raw));
        return parsed;
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "Failed to parse uint64 '" << key << "' in " << context << ": " << e.what();
        Fail(oss.str());
    }
}

std::vector<std::string> ReadStringList(const YAML::Node& node, std::string_view key, std::string_view context) {
    const auto seq = node[key];
    if (!seq || !seq.IsSequence()) {
        std::ostringstream oss;
        oss << "Missing required sequence key '" << key << "' in " << context;
        Fail(oss.str());
    }
    std::vector<std::string> values;
    values.reserve(seq.size());
    for (std::size_t i = 0; i < seq.size(); ++i) {
        const auto& item = seq[i];
        if (!item.IsScalar()) {
            std::ostringstream oss;
            oss << "Sequence '" << key << "' must contain only scalars (" << context << ")";
            Fail(oss.str());
        }
        const std::string value = item.as<std::string>();
        if (value.empty()) {
            std::ostringstream oss;
            oss << "Sequence '" << key << "' contains an empty string (" << context << ")";
            Fail(oss.str());
        }
        values.push_back(value);
    }
    if (values.empty()) {
        std::ostringstream oss;
        oss << "Sequence '" << key << "' in " << context << " must not be empty";
        Fail(oss.str());
    }
    return values;
}

struct BackendCatalog {
    std::vector<std::string> ids;
    std::unordered_map<std::string, std::size_t> index_by_id;
};

BackendCatalog ParseBackendCatalog(const fs::path& backend_yaml_path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(backend_yaml_path.string());
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "Failed to load backend YAML '" << backend_yaml_path << "': " << e.what();
        Fail(oss.str());
    }

    if (!root || !root.IsMap()) {
        Fail("Backend YAML root must be a mapping");
    }

    const auto backends_node = root["backends"];
    if (!backends_node || !backends_node.IsSequence()) {
        Fail("Backend YAML must contain a sequence 'backends'");
    }

    BackendCatalog catalog;
    catalog.ids.reserve(backends_node.size());

    for (std::size_t i = 0; i < backends_node.size(); ++i) {
        const auto& node = backends_node[i];
        if (!node.IsMap()) {
            std::ostringstream oss;
            oss << "Each backend entry must be a mapping (index " << i << ")";
            Fail(oss.str());
        }
        const std::string context = "backends[" + std::to_string(i) + "]";
        const std::string id = ReadRequiredString(node, "id", context);
        if (!LooksLikeIdentifier(id)) {
            std::ostringstream oss;
            oss << "Backend id '" << id << "' is not a valid identifier (" << context << ")";
            Fail(oss.str());
        }
        if (!catalog.index_by_id.emplace(id, catalog.ids.size()).second) {
            std::ostringstream oss;
            oss << "Duplicate backend id '" << id << "'";
            Fail(oss.str());
        }
        catalog.ids.push_back(id);
    }

    if (catalog.ids.empty()) {
        Fail("At least one backend must be defined");
    }

    return catalog;
}

struct ArchitectureCatalog {
    std::unordered_map<std::string, std::unordered_map<std::string, std::uint16_t>> local_index;
};

ArchitectureCatalog ParseArchitectureCatalog(const fs::path& architecture_yaml_path,
                                             const BackendCatalog& backends) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(architecture_yaml_path.string());
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "Failed to load architecture YAML '" << architecture_yaml_path << "': " << e.what();
        Fail(oss.str());
    }

    if (!root || !root.IsMap()) {
        Fail("Architecture YAML root must be a mapping");
    }

    const auto schema_node = root["schema_version"];
    if (!schema_node || !schema_node.IsScalar()) {
        Fail("Missing required scalar key 'schema_version' in architecture YAML");
    }

    ArchitectureCatalog catalog;

    for (const auto& backend_id : backends.ids) {
        catalog.local_index[backend_id]["Generic"] = 0;
    }

    const auto architectures_node = root["architectures"];
    if (!architectures_node || !architectures_node.IsSequence()) {
        Fail("Missing required sequence key 'architectures'");
    }

    std::unordered_map<std::string, std::uint16_t> next_local_index;
    for (const auto& backend_id : backends.ids) {
        next_local_index[backend_id] = 1;  // 0 reserved for Generic
    }

    for (std::size_t i = 0; i < architectures_node.size(); ++i) {
        const auto& node = architectures_node[i];
        if (!node.IsMap()) {
            std::ostringstream oss;
            oss << "Each architecture entry must be a mapping (index " << i << ")";
            Fail(oss.str());
        }
        const std::string context = "architectures[" + std::to_string(i) + "]";

        const std::string id = ReadRequiredString(node, "id", context);
        if (!LooksLikeIdentifier(id)) {
            std::ostringstream oss;
            oss << "Architecture id '" << id << "' is not a valid identifier (" << context << ")";
            Fail(oss.str());
        }

        const std::string backend_id = ReadRequiredString(node, "backend", context);
        const auto backend_it = catalog.local_index.find(backend_id);
        if (backend_it == catalog.local_index.end()) {
            std::ostringstream oss;
            oss << "Architecture '" << id << "' references unknown backend '" << backend_id << "'";
            Fail(oss.str());
        }

        if (!backend_it->second.emplace(id, next_local_index[backend_id]).second) {
            std::ostringstream oss;
            oss << "Duplicate architecture id '" << id << "' for backend '" << backend_id << "'";
            Fail(oss.str());
        }
        ++next_local_index[backend_id];
    }

    return catalog;
}

struct DTypeCatalog {
    std::unordered_map<std::string, std::size_t> index_by_id;
};

DTypeCatalog ParseDTypeCatalog(const fs::path& dtype_yaml_path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(dtype_yaml_path.string());
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "Failed to load dtype YAML '" << dtype_yaml_path << "': " << e.what();
        Fail(oss.str());
    }

    if (!root || !root.IsMap()) {
        Fail("DType YAML root must be a mapping");
    }

    const auto dtypes_node = root["dtypes"];
    if (!dtypes_node || !dtypes_node.IsSequence()) {
        Fail("Missing required sequence key 'dtypes'");
    }

    DTypeCatalog catalog;
    catalog.index_by_id.reserve(dtypes_node.size());

    for (std::size_t i = 0; i < dtypes_node.size(); ++i) {
        const auto& node = dtypes_node[i];
        if (!node.IsMap()) {
            std::ostringstream oss;
            oss << "Each dtype entry must be a mapping (index " << i << ")";
            Fail(oss.str());
        }
        const std::string context = "dtypes[" + std::to_string(i) + "]";
        const std::string id = ReadRequiredString(node, "id", context);
        if (!catalog.index_by_id.emplace(id, i).second) {
            std::ostringstream oss;
            oss << "Duplicate dtype id '" << id << "'";
            Fail(oss.str());
        }
    }

    return catalog;
}

struct OpCatalog {
    std::unordered_map<std::string, std::size_t> index_by_id;
};

OpCatalog ParseOpCatalog(const fs::path& ops_yaml_path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(ops_yaml_path.string());
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "Failed to load ops YAML '" << ops_yaml_path << "': " << e.what();
        Fail(oss.str());
    }

    if (!root || !root.IsMap()) {
        Fail("Ops YAML root must be a mapping");
    }

    const auto ops_node = root["ops"];
    if (!ops_node || !ops_node.IsSequence()) {
        Fail("Missing required sequence key 'ops'");
    }

    OpCatalog catalog;
    catalog.index_by_id.reserve(ops_node.size());

    for (std::size_t i = 0; i < ops_node.size(); ++i) {
        const auto& node = ops_node[i];
        if (!node.IsMap()) {
            std::ostringstream oss;
            oss << "Each op entry must be a mapping (index " << i << ")";
            Fail(oss.str());
        }
        const std::string context = "ops[" + std::to_string(i) + "]";
        const std::string id = ReadRequiredString(node, "id", context);
        if (!catalog.index_by_id.emplace(id, i).second) {
            std::ostringstream oss;
            oss << "Duplicate op id '" << id << "'";
            Fail(oss.str());
        }
    }

    if (catalog.index_by_id.empty()) {
        Fail("At least one op must be defined");
    }

    return catalog;
}

struct CapabilityEntry {
    std::string key;
    std::string value;
};

struct DeviceDefinition {
    std::string id;
    std::string display_name;
    std::size_t backend_index;
    std::uint16_t architecture_local_index;
    std::uint64_t memory_max_bytes;
    std::uint64_t memory_shared_bytes;
    std::vector<std::uint16_t> dtype_indices;
    std::vector<std::uint16_t> op_indices;
    std::vector<CapabilityEntry> capabilities;
    std::string notes;
};

std::uint16_t ToUint16Index(const std::string& value, std::size_t index, std::string_view what) {
    if (index > std::numeric_limits<std::uint16_t>::max()) {
        std::ostringstream oss;
        oss << what << " index for '" << value << "' exceeds uint16_t range";
        Fail(oss.str());
    }
    return static_cast<std::uint16_t>(index);
}

std::vector<DeviceDefinition> ParseDeviceConfig(
    const fs::path& device_yaml_path,
    const BackendCatalog& backends,
    const ArchitectureCatalog& architectures,
    const DTypeCatalog& dtypes,
    const OpCatalog& ops) {

    YAML::Node root;
    try {
        root = YAML::LoadFile(device_yaml_path.string());
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "Failed to load device YAML '" << device_yaml_path << "': " << e.what();
        Fail(oss.str());
    }

    if (!root || !root.IsMap()) {
        Fail("Device YAML root must be a mapping");
    }

    const auto schema_node = root["schema_version"];
    if (!schema_node || !schema_node.IsScalar()) {
        Fail("Missing required scalar key 'schema_version' in device YAML");
    }

    const auto devices_node = root["devices"];
    if (!devices_node || !devices_node.IsSequence()) {
        Fail("Missing required sequence key 'devices'");
    }

    std::vector<DeviceDefinition> devices;
    devices.reserve(devices_node.size());
    std::unordered_set<std::string> seen_ids;

    for (std::size_t i = 0; i < devices_node.size(); ++i) {
        const auto& node = devices_node[i];
        if (!node.IsMap()) {
            std::ostringstream oss;
            oss << "Each device entry must be a mapping (index " << i << ")";
            Fail(oss.str());
        }
        const std::string context = "devices[" + std::to_string(i) + "]";

        DeviceDefinition device;
        device.id = ReadRequiredString(node, "id", context);
        if (!LooksLikeIdentifier(device.id)) {
            std::ostringstream oss;
            oss << "Device id '" << device.id << "' is not a valid identifier (" << context << ")";
            Fail(oss.str());
        }
        if (!seen_ids.insert(device.id).second) {
            std::ostringstream oss;
            oss << "Duplicate device id '" << device.id << "'";
            Fail(oss.str());
        }

        device.display_name = ReadRequiredString(node, "display_name", context);

        const std::string backend_id = ReadRequiredString(node, "backend", context);
        const auto backend_it = backends.index_by_id.find(backend_id);
        if (backend_it == backends.index_by_id.end()) {
            std::ostringstream oss;
            oss << "Device '" << device.id << "' references unknown backend '" << backend_id << "'";
            Fail(oss.str());
        }
        device.backend_index = backend_it->second;

        const std::string architecture_id = ReadRequiredString(node, "architecture", context);
        const auto arch_backend_it = architectures.local_index.find(backend_id);
        if (arch_backend_it == architectures.local_index.end()) {
            std::ostringstream oss;
            oss << "No architecture catalog found for backend '" << backend_id << "'";
            Fail(oss.str());
        }
        const auto arch_it = arch_backend_it->second.find(architecture_id);
        if (arch_it == arch_backend_it->second.end()) {
            std::ostringstream oss;
            oss << "Device '" << device.id << "' references unknown architecture '" << architecture_id
                << "' for backend '" << backend_id << "'";
            Fail(oss.str());
        }
        device.architecture_local_index = arch_it->second;

        const auto memory_node = node["memory"];
        if (!memory_node || !memory_node.IsMap()) {
            std::ostringstream oss;
            oss << "Device '" << device.id << "' must define a mapping 'memory'";
            Fail(oss.str());
        }
        ExpectKeys(memory_node, context + ".memory", {"max_bytes", "shared_bytes"});
        device.memory_max_bytes = ReadRequiredUint64(memory_node, "max_bytes", context + ".memory");
        if (device.memory_max_bytes == 0) {
            std::ostringstream oss;
            oss << "Device '" << device.id << "' must have memory.max_bytes > 0";
            Fail(oss.str());
        }
        device.memory_shared_bytes =
            ReadOptionalUint64(memory_node, "shared_bytes", context + ".memory", 0);

        const auto dtype_ids = ReadStringList(node, "supported_dtypes", context);
        std::unordered_set<std::string> dtype_seen;
        for (const auto& dtype_id : dtype_ids) {
            const auto dtype_it = dtypes.index_by_id.find(dtype_id);
            if (dtype_it == dtypes.index_by_id.end()) {
                std::ostringstream oss;
                oss << "Device '" << device.id << "' references unknown dtype '" << dtype_id << "'";
                Fail(oss.str());
            }
            if (!dtype_seen.insert(dtype_id).second) {
                std::ostringstream oss;
                oss << "Device '" << device.id << "' lists dtype '" << dtype_id << "' multiple times";
                Fail(oss.str());
            }
            device.dtype_indices.push_back(
                ToUint16Index(dtype_id, dtype_it->second, "dtype"));
        }

        const auto op_ids = ReadStringList(node, "supported_ops", context);
        std::unordered_set<std::string> op_seen;
        for (const auto& op_id : op_ids) {
            const auto op_it = ops.index_by_id.find(op_id);
            if (op_it == ops.index_by_id.end()) {
                std::ostringstream oss;
                oss << "Device '" << device.id << "' references unknown op '" << op_id << "'";
                Fail(oss.str());
            }
            if (!op_seen.insert(op_id).second) {
                std::ostringstream oss;
                oss << "Device '" << device.id << "' lists op '" << op_id << "' multiple times";
                Fail(oss.str());
            }
            device.op_indices.push_back(
                ToUint16Index(op_id, op_it->second, "op"));
        }

        const auto capabilities_node = node["capabilities"];
        if (capabilities_node) {
            if (!capabilities_node.IsMap()) {
                std::ostringstream oss;
                oss << "Device '" << device.id << "' capabilities must be a mapping";
                Fail(oss.str());
            }
            for (const auto& capability : capabilities_node) {
                if (!capability.first.IsScalar() || !capability.second.IsScalar()) {
                    std::ostringstream oss;
                    oss << "Device '" << device.id << "' capabilities must map scalars to scalars";
                    Fail(oss.str());
                }
                CapabilityEntry entry;
                entry.key = capability.first.as<std::string>();
                entry.value = capability.second.as<std::string>();
                device.capabilities.push_back(std::move(entry));
            }
        }

        device.notes = ReadOptionalString(node, "notes", context).value_or("");

        devices.push_back(std::move(device));
    }

    if (devices.empty()) {
        Fail("Device YAML must define at least one device");
    }

    return devices;
}

struct GeneratedData {
    std::string device_def;
    std::string device_tables_header;
};

GeneratedData GenerateOutputs(const std::vector<DeviceDefinition>& devices) {
    GeneratedData generated;

    {
        std::ostringstream def_stream;
        def_stream << "// Auto-generated. Do not edit.\n";
        for (const auto& device : devices) {
            def_stream << "DEVICE(" << device.id << ", \""
                       << EscapeStringLiteral(device.display_name) << "\")\n";
        }
        generated.device_def = def_stream.str();
    }

    std::vector<std::size_t> dtype_offsets;
    dtype_offsets.reserve(devices.size() + 1);
    std::vector<std::uint16_t> dtype_entries;

    std::vector<std::size_t> op_offsets;
    op_offsets.reserve(devices.size() + 1);
    std::vector<std::uint16_t> op_entries;

    std::vector<std::size_t> capability_offsets;
    capability_offsets.reserve(devices.size() + 1);
    struct CapabilityKV {
        std::string key;
        std::string value;
    };
    std::vector<CapabilityKV> capability_entries;

    dtype_offsets.push_back(0);
    op_offsets.push_back(0);
    capability_offsets.push_back(0);

    for (const auto& device : devices) {
        dtype_entries.insert(dtype_entries.end(), device.dtype_indices.begin(), device.dtype_indices.end());
        dtype_offsets.push_back(dtype_entries.size());

        op_entries.insert(op_entries.end(), device.op_indices.begin(), device.op_indices.end());
        op_offsets.push_back(op_entries.size());

        for (const auto& capability : device.capabilities) {
            capability_entries.push_back({capability.key, capability.value});
        }
        capability_offsets.push_back(capability_entries.size());
    }

    const auto emit_uint_array = [](std::ostringstream& os, const std::string& name,
                                    const std::vector<std::uint16_t>& values) {
        os << "inline constexpr std::array<std::uint16_t, " << values.size() << "> " << name << " = {\n";
        for (const auto value : values) {
            os << "    " << value << ",\n";
        }
        os << "};\n\n";
    };

    const auto emit_size_array = [](std::ostringstream& os, const std::string& name,
                                    const std::vector<std::size_t>& values) {
        os << "inline constexpr std::array<std::size_t, " << values.size() << "> " << name << " = {\n";
        for (const auto value : values) {
            os << "    " << value << ",\n";
        }
        os << "};\n\n";
    };

    {
        std::ostringstream header_stream;
        header_stream << "// Auto-generated. Do not edit.\n";
        header_stream << "#pragma once\n\n";
        header_stream << "#include <array>\n";
        header_stream << "#include <cstddef>\n";
        header_stream << "#include <cstdint>\n";
        header_stream << "#include <string_view>\n\n";
        header_stream << "namespace orteaf::generated::device_tables {\n";
        header_stream << "inline constexpr std::size_t kDeviceCount = " << devices.size() << ";\n";
        header_stream << "inline constexpr std::size_t kDeviceDTypeEntryCount = " << dtype_entries.size() << ";\n";
        header_stream << "inline constexpr std::size_t kDeviceOpEntryCount = " << op_entries.size() << ";\n";
        header_stream << "inline constexpr std::size_t kDeviceCapabilityEntryCount = " << capability_entries.size() << ";\n\n";

        // Backend indices
        header_stream << "inline constexpr std::array<std::uint16_t, kDeviceCount> kDeviceBackendIndices = {\n";
        for (const auto& device : devices) {
            header_stream << "    " << device.backend_index << ",\n";
        }
        header_stream << "};\n\n";

        // Architecture local indices
        header_stream << "inline constexpr std::array<std::uint16_t, kDeviceCount> kDeviceArchitectureLocalIndices = {\n";
        for (const auto& device : devices) {
            header_stream << "    " << device.architecture_local_index << ",\n";
        }
        header_stream << "};\n\n";

        // Memory
        header_stream << "inline constexpr std::array<std::uint64_t, kDeviceCount> kDeviceMemoryMaxBytes = {\n";
        for (const auto& device : devices) {
            header_stream << "    " << device.memory_max_bytes << ",\n";
        }
        header_stream << "};\n\n";

        header_stream << "inline constexpr std::array<std::uint64_t, kDeviceCount> kDeviceMemorySharedBytes = {\n";
        for (const auto& device : devices) {
            header_stream << "    " << device.memory_shared_bytes << ",\n";
        }
        header_stream << "};\n\n";

        // Notes
        header_stream << "inline constexpr std::array<std::string_view, kDeviceCount> kDeviceNotes = {\n";
        for (const auto& device : devices) {
            header_stream << "    \"" << EscapeStringLiteral(device.notes) << "\",\n";
        }
        header_stream << "};\n\n";

        // Supported dtype arrays
        emit_size_array(header_stream, "kDeviceDTypeOffsets", dtype_offsets);
        emit_uint_array(header_stream, "kDeviceDTypeIndices", dtype_entries);

        // Supported op arrays
        emit_size_array(header_stream, "kDeviceOpOffsets", op_offsets);
        emit_uint_array(header_stream, "kDeviceOpIndices", op_entries);

        // Capabilities
        emit_size_array(header_stream, "kDeviceCapabilityOffsets", capability_offsets);
        header_stream << "struct CapabilityKV {\n";
        header_stream << "    std::string_view key;\n";
        header_stream << "    std::string_view value;\n";
        header_stream << "};\n\n";
        header_stream << "inline constexpr std::array<CapabilityKV, kDeviceCapabilityEntryCount> "
                         "kDeviceCapabilityEntries = {\n";
        for (const auto& capability : capability_entries) {
            header_stream << "    CapabilityKV{\"" << EscapeStringLiteral(capability.key) << "\", \""
                          << EscapeStringLiteral(capability.value) << "\"},\n";
        }
        header_stream << "};\n\n";

        header_stream << "}  // namespace orteaf::generated::device_tables\n";

        generated.device_tables_header = header_stream.str();
    }

    return generated;
}

void WriteFile(const fs::path& path, const std::string& content) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    if (ec) {
        std::ostringstream oss;
        oss << "Failed to create directories for '" << path << "': " << ec.message();
        Fail(oss.str());
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::ostringstream oss;
        oss << "Failed to open output file '" << path << "'";
        Fail(oss.str());
    }
    file << content;
    if (!file.good()) {
        std::ostringstream oss;
        oss << "Failed to write output file '" << path << "'";
        Fail(oss.str());
    }
}

int main(int argc, char** argv) {
    if (argc != 7) {
        std::cerr << "Usage: gen_devices <devices.yml> <backends.yml> <architectures.yml> "
                     "<dtypes.yml> <ops.yml> <output_dir>\n";
        return 1;
    }

    const fs::path devices_yaml = argv[1];
    const fs::path backends_yaml = argv[2];
    const fs::path architectures_yaml = argv[3];
    const fs::path dtypes_yaml = argv[4];
    const fs::path ops_yaml = argv[5];
    const fs::path output_dir = argv[6];

    try {
        const auto backends = ParseBackendCatalog(backends_yaml);
        const auto architectures = ParseArchitectureCatalog(architectures_yaml, backends);
        const auto dtype_catalog = ParseDTypeCatalog(dtypes_yaml);
        const auto op_catalog = ParseOpCatalog(ops_yaml);
        const auto devices = ParseDeviceConfig(devices_yaml, backends, architectures,
                                               dtype_catalog, op_catalog);

        const auto generated = GenerateOutputs(devices);
        WriteFile(output_dir / "device.def", generated.device_def);
        WriteFile(output_dir / "device_tables.h", generated.device_tables_header);
    } catch (const std::exception& e) {
        std::cerr << "[gen_devices] Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
