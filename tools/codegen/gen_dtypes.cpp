#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

[[noreturn]] void Fail(const std::string& message) {
    throw std::runtime_error(message);
}

std::string ReadString(const YAML::Node& node, std::string_view key, bool required, std::string_view context) {
    const auto value = node[key];
    if (!value) {
        if (required) {
            std::ostringstream oss;
            oss << "Missing required key '" << key << "' in " << context;
            Fail(oss.str());
        }
        return {};
    }
    if (!value.IsScalar()) {
        std::ostringstream oss;
        oss << "Key '" << key << "' must be a scalar in " << context;
        Fail(oss.str());
    }
    return value.as<std::string>();
}

std::vector<std::string> ReadStringList(const YAML::Node& node, std::string_view key, std::string_view context) {
    const auto value = node[key];
    if (!value) {
        return {};
    }
    if (!value.IsSequence()) {
        std::ostringstream oss;
        oss << "Key '" << key << "' must be a sequence in " << context;
        Fail(oss.str());
    }
    std::vector<std::string> result;
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        const auto& entry = value[i];
        if (!entry.IsScalar()) {
            std::ostringstream oss;
            oss << "All entries of '" << key << "' must be scalars in " << context;
            Fail(oss.str());
        }
        result.emplace_back(entry.as<std::string>());
    }
    return result;
}

int ReadInt(const YAML::Node& node, std::string_view key, int default_value, std::string_view context) {
    const auto value = node[key];
    if (!value) {
        return default_value;
    }
    if (!value.IsScalar()) {
        std::ostringstream oss;
        oss << "Key '" << key << "' must be a scalar integer in " << context;
        Fail(oss.str());
    }
    try {
        return value.as<int>();
    } catch (const YAML::BadConversion&) {
        std::ostringstream oss;
        oss << "Key '" << key << "' must be a valid integer in " << context;
        Fail(oss.str());
    }
}

std::string EscapeStringLiteral(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 2);
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

std::string FormatBitMask(std::uint64_t mask) {
    std::ostringstream oss;
    oss << "0x" << std::uppercase << std::hex << mask << "ULL";
    return oss.str();
}

struct DTypeDefinition {
    std::string id;
    std::string cpp_type;
    std::string display_name;
    std::string category;
    int promotion_priority = 0;
    std::string compute_dtype;
    std::vector<std::string> implicit_cast_to;
    std::vector<std::string> explicit_cast_to;
    YAML::Node metadata;
};

struct PromotionOverride {
    std::string lhs;
    std::string rhs;
    std::string result;
};

struct ParsedConfig {
    std::string schema_version;
    std::vector<DTypeDefinition> dtypes;
    std::vector<PromotionOverride> promotion_overrides;
};

ParsedConfig ParseConfig(const fs::path& yaml_path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(yaml_path.string());
    } catch (const std::exception& e) {
        std::ostringstream oss;
        oss << "Failed to load YAML '" << yaml_path << "': " << e.what();
        Fail(oss.str());
    }

    if (!root || !root.IsMap()) {
        std::ostringstream oss;
        oss << "Root of '" << yaml_path << "' must be a mapping";
        Fail(oss.str());
    }

    ParsedConfig config;
    const auto schema_version_node = root["schema_version"];
    if (!schema_version_node || !schema_version_node.IsScalar()) {
        Fail("Missing required scalar key 'schema_version'");
    }
    config.schema_version = schema_version_node.as<std::string>();

    const auto dtypes_node = root["dtypes"];
    if (!dtypes_node || !dtypes_node.IsSequence()) {
        Fail("Missing required sequence key 'dtypes'");
    }

    std::unordered_set<std::string> seen_ids;
    std::unordered_set<std::string> seen_cpp_types;

    config.dtypes.reserve(dtypes_node.size());
    for (std::size_t idx = 0; idx < dtypes_node.size(); ++idx) {
        const auto& node = dtypes_node[idx];
        if (!node.IsMap()) {
            std::ostringstream oss;
            oss << "Each dtype entry must be a mapping (index " << idx << ")";
            Fail(oss.str());
        }
        const std::string context = "dtypes[" + std::to_string(idx) + "]";
        DTypeDefinition dtype;
        dtype.id = ReadString(node, "id", true, context);
        if (dtype.id.empty()) {
            std::ostringstream oss;
            oss << "Key 'id' must not be empty (" << context << ")";
            Fail(oss.str());
        }
        if (!seen_ids.insert(dtype.id).second) {
            std::ostringstream oss;
            oss << "Duplicate dtype id '" << dtype.id << "'";
            Fail(oss.str());
        }

        dtype.cpp_type = ReadString(node, "cpp_type", true, context);
        if (dtype.cpp_type.empty()) {
            std::ostringstream oss;
            oss << "Key 'cpp_type' must not be empty (" << context << ")";
            Fail(oss.str());
        }
        if (!seen_cpp_types.insert(dtype.cpp_type).second) {
            std::ostringstream oss;
            oss << "Duplicate cpp_type '" << dtype.cpp_type << "'";
            Fail(oss.str());
        }

        dtype.display_name = ReadString(node, "display_name", false, context);
        if (dtype.display_name.empty()) {
            dtype.display_name = dtype.id;
        }
        dtype.category = ReadString(node, "category", false, context);
        if (dtype.category.empty()) {
            dtype.category = "unknown";
        }
        dtype.promotion_priority = ReadInt(node, "promotion_priority", 0, context);
        dtype.compute_dtype = ReadString(node, "compute_dtype", false, context);
        if (dtype.compute_dtype.empty()) {
            dtype.compute_dtype = dtype.id;
        }
        dtype.implicit_cast_to = ReadStringList(node, "implicit_cast_to", context);
        dtype.explicit_cast_to = ReadStringList(node, "explicit_cast_to", context);
        dtype.metadata = node["metadata"];

        config.dtypes.emplace_back(std::move(dtype));
    }

    const auto overrides_node = root["promotion_overrides"];
    if (overrides_node) {
        if (!overrides_node.IsSequence()) {
            Fail("Key 'promotion_overrides' must be a sequence");
        }
        config.promotion_overrides.reserve(overrides_node.size());
        for (std::size_t idx = 0; idx < overrides_node.size(); ++idx) {
            const auto& node = overrides_node[idx];
            if (!node.IsMap()) {
                std::ostringstream oss;
                oss << "Each promotion override must be a mapping (index " << idx << ")";
                Fail(oss.str());
            }
            const std::string context = "promotion_overrides[" + std::to_string(idx) + "]";
            PromotionOverride override;
            override.lhs = ReadString(node, "lhs", true, context);
            override.rhs = ReadString(node, "rhs", true, context);
            override.result = ReadString(node, "result", true, context);
            config.promotion_overrides.emplace_back(std::move(override));
        }
    }

    return config;
}

struct ResolvedConfig {
    std::vector<DTypeDefinition> dtypes;
    std::vector<PromotionOverride> promotion_overrides;
    std::unordered_map<std::string, std::size_t> id_to_index;
};

ResolvedConfig ResolveConfig(ParsedConfig config) {
    if (config.schema_version != "1.0") {
        std::ostringstream oss;
        oss << "Unsupported schema_version '" << config.schema_version << "', expected '1.0'";
        Fail(oss.str());
    }

    ResolvedConfig resolved;
    resolved.id_to_index.reserve(config.dtypes.size());
    for (std::size_t i = 0; i < config.dtypes.size(); ++i) {
        resolved.id_to_index.emplace(config.dtypes[i].id, i);
    }

    auto ensure_known = [&](const std::string& id, std::string_view field, std::string_view owner) {
        if (!resolved.id_to_index.count(id)) {
            std::ostringstream oss;
            oss << "Unknown dtype '" << id << "' referenced by '" << field << "' in " << owner;
            Fail(oss.str());
        }
    };

    for (std::size_t i = 0; i < config.dtypes.size(); ++i) {
        auto& dtype = config.dtypes[i];
        const std::string owner = "dtypes[" + std::to_string(i) + "]";
        ensure_known(dtype.compute_dtype, "compute_dtype", owner);

        std::unordered_set<std::string> seen_implicit;
        for (const auto& target : dtype.implicit_cast_to) {
            ensure_known(target, "implicit_cast_to", owner);
            if (!seen_implicit.insert(target).second) {
                std::ostringstream oss;
                oss << "Duplicate implicit_cast_to target '" << target << "' in " << owner;
                Fail(oss.str());
            }
        }
        std::unordered_set<std::string> seen_explicit;
        for (const auto& target : dtype.explicit_cast_to) {
            ensure_known(target, "explicit_cast_to", owner);
            if (!seen_explicit.insert(target).second) {
                std::ostringstream oss;
                oss << "Duplicate explicit_cast_to target '" << target << "' in " << owner;
                Fail(oss.str());
            }
        }
    }

    for (std::size_t i = 0; i < config.promotion_overrides.size(); ++i) {
        const auto& override = config.promotion_overrides[i];
        const std::string owner = "promotion_overrides[" + std::to_string(i) + "]";
        ensure_known(override.lhs, "lhs", owner);
        ensure_known(override.rhs, "rhs", owner);
        ensure_known(override.result, "result", owner);
    }

    resolved.dtypes = std::move(config.dtypes);
    resolved.promotion_overrides = std::move(config.promotion_overrides);
    return resolved;
}

struct GeneratedData {
    std::string dtype_def;
    std::string dtype_tables_header;
};

GeneratedData GenerateOutputs(const ResolvedConfig& resolved) {
    const auto dtype_count = resolved.dtypes.size();
    if (dtype_count == 0) {
        Fail("No dtypes defined");
    }
    if (dtype_count > 64) {
        Fail("Currently only up to 64 dtypes are supported by the generator (bitset limitation)");
    }

    std::vector<std::vector<std::size_t>> promotion_table(dtype_count, std::vector<std::size_t>(dtype_count));
    std::vector<std::uint64_t> implicit_masks(dtype_count, 0);
    std::vector<std::uint64_t> explicit_masks(dtype_count, 0);

    for (std::size_t i = 0; i < dtype_count; ++i) {
        implicit_masks[i] |= (1ULL << i);
        explicit_masks[i] |= (1ULL << i);
        for (std::size_t j = 0; j < dtype_count; ++j) {
            const auto& lhs = resolved.dtypes[i];
            const auto& rhs = resolved.dtypes[j];
            std::size_t result_index = (lhs.promotion_priority >= rhs.promotion_priority) ? i : j;
            promotion_table[i][j] = result_index;
        }
    }

    for (std::size_t i = 0; i < dtype_count; ++i) {
        const auto& dtype = resolved.dtypes[i];
        for (const auto& target : dtype.implicit_cast_to) {
            auto index = resolved.id_to_index.at(target);
            implicit_masks[i] |= (1ULL << index);
            explicit_masks[i] |= (1ULL << index);
        }
        for (const auto& target : dtype.explicit_cast_to) {
            auto index = resolved.id_to_index.at(target);
            explicit_masks[i] |= (1ULL << index);
        }
    }

    for (const auto& override_entry : resolved.promotion_overrides) {
        const auto lhs_index = resolved.id_to_index.at(override_entry.lhs);
        const auto rhs_index = resolved.id_to_index.at(override_entry.rhs);
        const auto result_index = resolved.id_to_index.at(override_entry.result);
        promotion_table[lhs_index][rhs_index] = result_index;
    }

    std::ostringstream def_stream;
    def_stream << "// Auto-generated. Do not edit.\n";
    for (const auto& dtype : resolved.dtypes) {
        def_stream << "DTYPE(" << dtype.id << ", " << dtype.cpp_type << ", \""
                   << EscapeStringLiteral(dtype.display_name) << "\")\n";
    }

    std::ostringstream header_stream;
    header_stream << "// Auto-generated. Do not edit.\n";
    header_stream << "#pragma once\n\n";
    header_stream << "#include <array>\n";
    header_stream << "#include <bitset>\n";
    header_stream << "#include <cstddef>\n";
    header_stream << "#include <cstdint>\n";
    header_stream << "#include <string_view>\n\n";
    header_stream << "namespace orteaf::generated::dtype_tables {\n";
    header_stream << "using ::orteaf::internal::DType;\n\n";
    header_stream << "inline constexpr std::size_t kDTypeCount = " << dtype_count << ";\n\n";

    header_stream << "inline constexpr std::array<std::string_view, kDTypeCount> kDTypeDisplayNames = {\n";
    for (const auto& dtype : resolved.dtypes) {
        header_stream << "    \"" << EscapeStringLiteral(dtype.display_name) << "\",\n";
    }
    header_stream << "};\n\n";

    header_stream << "inline constexpr std::array<std::string_view, kDTypeCount> kDTypeCategories = {\n";
    for (const auto& dtype : resolved.dtypes) {
        header_stream << "    \"" << EscapeStringLiteral(dtype.category) << "\",\n";
    }
    header_stream << "};\n\n";

    header_stream << "inline constexpr std::array<int, kDTypeCount> kDTypePromotionPriorities = {\n";
    for (const auto& dtype : resolved.dtypes) {
        header_stream << "    " << dtype.promotion_priority << ",\n";
    }
    header_stream << "};\n\n";

    header_stream << "inline constexpr std::array<std::size_t, kDTypeCount> kDTypeSize = {\n";
    for (const auto& dtype : resolved.dtypes) {
        header_stream << "    sizeof(" << dtype.cpp_type << "),\n";
    }
    header_stream << "};\n\n";

    header_stream << "inline constexpr std::array<std::size_t, kDTypeCount> kDTypeAlignment = {\n";
    for (const auto& dtype : resolved.dtypes) {
        header_stream << "    alignof(" << dtype.cpp_type << "),\n";
    }
    header_stream << "};\n\n";

    header_stream << "inline constexpr std::array<::orteaf::internal::DType, kDTypeCount> kDTypeComputeType = {\n";
    for (const auto& dtype : resolved.dtypes) {
        header_stream << "    ::orteaf::internal::DType::" << dtype.compute_dtype << ",\n";
    }
    header_stream << "};\n\n";

    header_stream << "inline constexpr std::array<std::array<::orteaf::internal::DType, kDTypeCount>, kDTypeCount> kPromotionTable = {\n";
    for (std::size_t i = 0; i < dtype_count; ++i) {
        header_stream << "    std::array<::orteaf::internal::DType, kDTypeCount>{\n";
        for (std::size_t j = 0; j < dtype_count; ++j) {
            const auto result_index = promotion_table[i][j];
            const auto& result_dtype = resolved.dtypes[result_index];
            header_stream << "        ::orteaf::internal::DType::" << result_dtype.id << ",\n";
        }
        header_stream << "    },\n";
    }
    header_stream << "};\n\n";

    header_stream << "inline constexpr std::array<std::bitset<kDTypeCount>, kDTypeCount> kImplicitCastMatrix = {\n";
    for (std::size_t i = 0; i < dtype_count; ++i) {
        header_stream << "    std::bitset<kDTypeCount>{" << FormatBitMask(implicit_masks[i]) << "},\n";
    }
    header_stream << "};\n\n";

    header_stream << "inline constexpr std::array<std::bitset<kDTypeCount>, kDTypeCount> kExplicitCastMatrix = {\n";
    for (std::size_t i = 0; i < dtype_count; ++i) {
        header_stream << "    std::bitset<kDTypeCount>{" << FormatBitMask(explicit_masks[i]) << "},\n";
    }
    header_stream << "};\n\n";

    header_stream << "}  // namespace orteaf::generated::dtype_tables\n";

    GeneratedData generated;
    generated.dtype_def = def_stream.str();
    generated.dtype_tables_header = header_stream.str();
    return generated;
}

void WriteFile(const fs::path& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::ostringstream oss;
        oss << "Failed to open file '" << path << "' for writing";
        Fail(oss.str());
    }
    file << content;
    if (!file.good()) {
        std::ostringstream oss;
        oss << "Failed to write file '" << path << "'";
        Fail(oss.str());
    }
}

}  // namespace

int main(int argc, char** argv) try {
    if (argc != 3) {
        std::cerr << "Usage: gen_dtypes <dtypes.yml> <output_dir>\n";
        return 1;
    }

    const fs::path input_path = argv[1];
    const fs::path output_dir = argv[2];
    if (!fs::exists(input_path)) {
        std::cerr << "Input file does not exist: " << input_path << "\n";
        return 1;
    }

    ParsedConfig parsed = ParseConfig(input_path);
    ResolvedConfig resolved = ResolveConfig(std::move(parsed));
    GeneratedData generated = GenerateOutputs(resolved);

    std::error_code ec;
    fs::create_directories(output_dir, ec);
    if (ec) {
        std::cerr << "Failed to create output directory '" << output_dir << "': " << ec.message() << "\n";
        return 1;
    }

    WriteFile(output_dir / "dtype.def", generated.dtype_def);
    WriteFile(output_dir / "dtype_tables.h", generated.dtype_tables_header);

    return 0;
} catch (const std::exception& e) {
    std::cerr << "gen_dtypes error: " << e.what() << "\n";
    return 1;
}

