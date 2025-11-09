#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
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

bool LooksLikeIdentifier(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    const unsigned char first = static_cast<unsigned char>(value.front());
    if (!(std::isalpha(first) || value.front() == '_')) {
        return false;
    }
    for (const char ch : value) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (!(std::isalnum(uch) || ch == '_')) {
            return false;
        }
    }
    return true;
}

struct BackendInfo {
    std::string id;
    std::string display_name;
};

struct ArchitectureInput {
    std::string id;
    std::string backend_id;
    std::string display_name;
    std::string description;
};

std::vector<BackendInfo> ParseBackendConfig(const fs::path& backend_yaml_path) {
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

    std::vector<BackendInfo> backends;
    backends.reserve(backends_node.size());
    std::unordered_set<std::string> seen;

    for (std::size_t i = 0; i < backends_node.size(); ++i) {
        const auto& node = backends_node[i];
        if (!node.IsMap()) {
            std::ostringstream oss;
            oss << "Each backend entry must be a mapping (index " << i << ")";
            Fail(oss.str());
        }
        const std::string context = "backends[" + std::to_string(i) + "]";

        BackendInfo info;
        info.id = ReadRequiredString(node, "id", context);
        if (!LooksLikeIdentifier(info.id)) {
            std::ostringstream oss;
            oss << "Backend id '" << info.id << "' is not a valid identifier (" << context << ")";
            Fail(oss.str());
        }
        if (!seen.insert(info.id).second) {
            std::ostringstream oss;
            oss << "Duplicate backend id '" << info.id << "'";
            Fail(oss.str());
        }
        info.display_name = ReadRequiredString(node, "display_name", context);
        backends.push_back(std::move(info));
    }

    if (backends.empty()) {
        Fail("At least one backend must be defined");
    }

    return backends;
}

std::vector<ArchitectureInput> ParseArchitectureConfig(const fs::path& architecture_yaml_path,
                                                       const std::unordered_set<std::string>& valid_backends) {
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

    const auto architectures_node = root["architectures"];
    if (!architectures_node || !architectures_node.IsSequence()) {
        Fail("Missing required sequence key 'architectures'");
    }

    std::vector<ArchitectureInput> architectures;
    architectures.reserve(architectures_node.size());
    std::unordered_map<std::string, std::unordered_set<std::string>> seen_per_backend;

    for (std::size_t i = 0; i < architectures_node.size(); ++i) {
        const auto& node = architectures_node[i];
        if (!node.IsMap()) {
            std::ostringstream oss;
            oss << "Each architecture entry must be a mapping (index " << i << ")";
            Fail(oss.str());
        }
        const std::string context = "architectures[" + std::to_string(i) + "]";

        ArchitectureInput input;
        input.id = ReadRequiredString(node, "id", context);
        if (!LooksLikeIdentifier(input.id)) {
            std::ostringstream oss;
            oss << "Architecture id '" << input.id << "' is not a valid identifier (" << context << ")";
            Fail(oss.str());
        }

        input.backend_id = ReadRequiredString(node, "backend", context);
        if (!valid_backends.count(input.backend_id)) {
            std::ostringstream oss;
            oss << "Architecture '" << input.id << "' references unknown backend '" << input.backend_id << "'";
            Fail(oss.str());
        }

        if (!seen_per_backend[input.backend_id].insert(input.id).second) {
            std::ostringstream oss;
            oss << "Duplicate architecture id '" << input.id << "' for backend '" << input.backend_id << "'";
            Fail(oss.str());
        }

        input.display_name = ReadRequiredString(node, "display_name", context);

        const auto metadata_node = node["metadata"];
        if (metadata_node) {
            if (!metadata_node.IsMap()) {
                std::ostringstream oss;
                oss << "Metadata for " << context << " must be a mapping";
                Fail(oss.str());
            }
            const std::string metadata_context = context + ".metadata";
            ExpectKeys(metadata_node, metadata_context, {"description"});
            if (const auto desc = ReadOptionalString(metadata_node, "description", metadata_context); desc) {
                input.description = *desc;
            }
        }

        architectures.push_back(std::move(input));
    }

    return architectures;
}

struct ResolvedArchitecture {
    std::string enum_name;
    std::string architecture_id;
    std::string display_name;
    std::string description;
    std::size_t backend_index;
    std::uint16_t local_index;
};

struct GeneratedData {
    std::string architecture_def;
    std::string architecture_tables_header;
};

GeneratedData GenerateOutputs(const std::vector<BackendInfo>& backends,
                              const std::vector<ArchitectureInput>& architectures) {
    // Group user-defined architectures by backend.
    std::unordered_map<std::string, std::vector<ArchitectureInput>> by_backend;
    by_backend.reserve(backends.size());
    for (const auto& arch : architectures) {
        by_backend[arch.backend_id].push_back(arch);
    }

    // Prepare resolved list in backend order.
    std::vector<ResolvedArchitecture> resolved;
    resolved.reserve(backends.size() + architectures.size());

    std::vector<std::size_t> backend_offsets;
    backend_offsets.reserve(backends.size() + 1);
    std::vector<std::size_t> backend_counts(backends.size(), 0);

    for (std::size_t backend_index = 0; backend_index < backends.size(); ++backend_index) {
        const auto& backend = backends[backend_index];
        backend_offsets.push_back(resolved.size());

        const auto duplicate_it = by_backend.find(backend.id);
        if (duplicate_it != by_backend.end()) {
            const auto& entries = duplicate_it->second;
            if (std::any_of(entries.begin(), entries.end(), [](const ArchitectureInput& input) {
                    return input.id == "generic";
                })) {
                std::ostringstream oss;
                oss << "Backend '" << backend.id
                    << "' defines architecture id 'generic', which is reserved for the auto-generated fallback";
                Fail(oss.str());
            }
        }

        auto make_enum_name = [](const std::string& backend_id, const std::string& arch_id) {
            std::ostringstream oss;
            oss << backend_id << '_' << arch_id;
            return oss.str();
        };

        // Auto-insert generic entry (local index 0).
        ResolvedArchitecture generic;
        generic.enum_name = make_enum_name(backend.id, "generic");
        generic.architecture_id = "generic";
        generic.display_name = "Generic " + backend.display_name;
        generic.description = "Backend-wide fallback architecture for " + backend.display_name;
        generic.backend_index = backend_index;
        generic.local_index = 0;
        resolved.push_back(std::move(generic));

        // Append user entries honoring input order.
        const auto by_backend_it = by_backend.find(backend.id);
        if (by_backend_it != by_backend.end()) {
            const auto& entries = by_backend_it->second;
            std::uint16_t local_index = 1;
            for (const auto& entry : entries) {
                ResolvedArchitecture resolved_entry;
                resolved_entry.enum_name = make_enum_name(backend.id, entry.id);
                resolved_entry.architecture_id = entry.id;
                resolved_entry.display_name = entry.display_name;
                resolved_entry.description = entry.description;
                resolved_entry.backend_index = backend_index;
                resolved_entry.local_index = local_index++;
                resolved.push_back(std::move(resolved_entry));
            }
            backend_counts[backend_index] = entries.size() + 1;  // +1 for generic
        } else {
            backend_counts[backend_index] = 1;  // generic only
        }
    }
    backend_offsets.push_back(resolved.size());

    GeneratedData generated;

    {
        std::ostringstream def_stream;
        def_stream << "// Auto-generated. Do not edit.\n";
        for (const auto& arch : resolved) {
            def_stream << "ARCHITECTURE(" << arch.enum_name << ", backend::Backend::"
                       << backends[arch.backend_index].id << ", "
                       << arch.local_index << ", \""
                       << EscapeStringLiteral(arch.architecture_id) << "\", \""
                       << EscapeStringLiteral(arch.display_name) << "\", \""
                       << EscapeStringLiteral(arch.description) << "\")\n";
        }
        generated.architecture_def = def_stream.str();
    }

    {
        std::ostringstream header_stream;
        header_stream << "// Auto-generated. Do not edit.\n";
        header_stream << "#pragma once\n\n";
        header_stream << "#include <array>\n";
        header_stream << "#include <cstddef>\n";
        header_stream << "#include <cstdint>\n";
        header_stream << "#include <string_view>\n\n";
        header_stream << "namespace orteaf::generated::architecture_tables {\n";
        header_stream << "inline constexpr std::size_t kArchitectureCount = " << resolved.size() << ";\n";
        header_stream << "inline constexpr std::size_t kBackendCount = " << backends.size() << ";\n\n";

        auto emit_array = [&](const std::string_view decl, auto value_fn) {
            header_stream << "inline constexpr " << decl << " = {\n";
            for (const auto& arch : resolved) {
                header_stream << "    " << value_fn(arch) << ",\n";
            }
            header_stream << "};\n\n";
        };

        emit_array("std::array<std::uint16_t, kArchitectureCount> kArchitectureBackendIndices",
                   [](const ResolvedArchitecture& arch) { return std::to_string(arch.backend_index); });
        emit_array("std::array<std::uint16_t, kArchitectureCount> kArchitectureLocalIndices",
                   [](const ResolvedArchitecture& arch) { return std::to_string(arch.local_index); });
        emit_array("std::array<std::string_view, kArchitectureCount> kArchitectureIds",
                   [](const ResolvedArchitecture& arch) {
                       std::ostringstream oss;
                       oss << '"' << EscapeStringLiteral(arch.architecture_id) << '"';
                       return oss.str();
                   });
        emit_array("std::array<std::string_view, kArchitectureCount> kArchitectureDisplayNames",
                   [](const ResolvedArchitecture& arch) {
                       std::ostringstream oss;
                       oss << '"' << EscapeStringLiteral(arch.display_name) << '"';
                       return oss.str();
                   });
        emit_array("std::array<std::string_view, kArchitectureCount> kArchitectureDescriptions",
                   [](const ResolvedArchitecture& arch) {
                       std::ostringstream oss;
                       oss << '"' << EscapeStringLiteral(arch.description) << '"';
                       return oss.str();
                   });

        header_stream << "inline constexpr std::array<std::size_t, " << backends.size()
                      << "> kBackendArchitectureCounts = {\n";
        for (const auto count : backend_counts) {
            header_stream << "    " << count << ",\n";
        }
        header_stream << "};\n\n";

        header_stream << "inline constexpr std::array<std::size_t, " << (backends.size() + 1)
                      << "> kBackendArchitectureOffsets = {\n";
        for (const auto offset : backend_offsets) {
            header_stream << "    " << offset << ",\n";
        }
        header_stream << "};\n\n";

        header_stream << "}  // namespace orteaf::generated::architecture_tables\n";

        generated.architecture_tables_header = header_stream.str();
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
    if (argc != 4) {
        std::cerr << "Usage: gen_architectures <architectures.yml> <backends.yml> <output_dir>\n";
        return 1;
    }

    const fs::path architecture_yaml = argv[1];
    const fs::path backend_yaml = argv[2];
    const fs::path output_dir = argv[3];

    try {
        const auto backends = ParseBackendConfig(backend_yaml);
        std::unordered_set<std::string> backend_ids;
        backend_ids.reserve(backends.size());
        for (const auto& backend : backends) {
            backend_ids.insert(backend.id);
        }

        const auto architectures = ParseArchitectureConfig(architecture_yaml, backend_ids);
        const auto generated = GenerateOutputs(backends, architectures);

        WriteFile(output_dir / "architecture.def", generated.architecture_def);
        WriteFile(output_dir / "architecture_tables.h", generated.architecture_tables_header);
    } catch (const std::exception& e) {
        std::cerr << "[gen_architectures] Error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
