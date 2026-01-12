#pragma once

#include <format>
#include <fstream>
#include <iostream>
#include <ranges>

constexpr std::string_view INCLUDE_PREFIX = "#include";

inline void STDIOLogging::log(std::string_view msg) {
    std::cout << "[GLSL] Warning: " << msg << std::endl;
}

inline void STDIOLogging::logAsError(std::string_view msg) {
    std::cerr << "[GLSL] Error: " << msg << std::endl;
}

static std::optional<std::string> readString(const std::filesystem::path& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::ifstream::pos_type fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::string buffer(fileSize, '\0');
    if (!file.read(buffer.data(), fileSize)) {
        return std::nullopt;
    }

    return buffer;
}

inline std::optional<std::string> SillyFileProvider::getString(const std::filesystem::path& filepath) {
    return readString(filepath);
}

inline std::optional<std::string> CachedFileProvider::getString(const std::filesystem::path& filepath) const {
    std::string str = filepath.string();
    if (const auto it = cache_.find(str); it != cache_.end()) {
        return it->second;
    }

    std::optional<std::string> source = readString(filepath);
    if (!source.has_value()) {
        return std::nullopt;
    }

    return cache_.emplace(str, std::move(*source)).first->second;
}

inline std::optional<std::string> SmartCachedFileProvider::getString(const std::filesystem::path& filepath) const {
    try {
        CacheKey key(filepath);
        if (const auto it = cache_.find(key); it != cache_.end()) {
            return it->second;
        }

        std::optional<std::string> source = readString(filepath);
        if (!source.has_value()) {
            return std::nullopt;
        }

        return cache_.emplace(key, std::move(*source)).first->second;
    } catch (std::filesystem::filesystem_error&) {
        return std::nullopt;
    }
}

template<SourceProvider SOURCE_PROVIDER>
std::optional<std::string> GLSLSourceProcessor<SOURCE_PROVIDER>::getShaderSource(const std::string& name) const {
    std::optional<std::string> src = sourceProvider_.getSource(SourceType::Source, name);
    if (src.has_value()) {
        std::unordered_set<std::string> alreadyIncludedFiles;
        return process<SourceType::Source>(src.value(), alreadyIncludedFiles);
    }
    return src;
}

template<std::ranges::range R>
auto getLine(R&& range) {
    if constexpr (std::ranges::contiguous_range<R> && std::ranges::sized_range<R>) {
        return std::string_view(std::ranges::data(range), std::ranges::size(range));
    } else {
        return std::string(std::ranges::begin(range), std::ranges::end(range));
    }
}

template<SourceProvider SOURCE_PROVIDER>
template<SourceType TYPE>
std::optional<std::string> GLSLSourceProcessor<SOURCE_PROVIDER>::process(std::string_view source,
    std::unordered_set<std::string>& alreadyIncludedFiles) const {
    std::string result;

    // Rough estimate
    result.reserve(source.size() + glslVersion_.size() + definitionMap_.size() * 32);

    if constexpr (TYPE == SourceType::Source) {
        result += glslVersion_;
        result += '\n';

        for (const auto& [name, value] : definitionMap_) {
            constexpr std::string_view DEFINE_PREFIX = "#define ";
            result += DEFINE_PREFIX;
            result += name;
            result += ' ';
            result += value;
            result += '\n';
        }
    }

    for (auto range : std::ranges::split_view(source, '\n')) {
        if (auto line = getLine(range); line.starts_with(INCLUDE_PREFIX)) {
            size_t start = line.find('\"');
            size_t end = line.rfind('\"');

            if (start == std::string::npos || end <= start) {
                log_(std::format("Invalid include directive: {}", line));
                return std::nullopt;
            }

            std::string includeName(line.substr(start + 1, end - start - 1));
            if (alreadyIncludedFiles.contains(includeName))
                continue;

            alreadyIncludedFiles.insert(includeName);

            std::optional<std::string> include = getShaderInclude(includeName, alreadyIncludedFiles);
            if (!include.has_value()) {
                return std::nullopt;
            }

            result += include.value();
        } else {
            result += line;
            result += '\n';
        }
    }

    return std::make_optional(std::move(result));
}

template<SourceProvider SOURCE_PROVIDER>
std::optional<std::string> GLSLSourceProcessor<SOURCE_PROVIDER>::getShaderInclude(const std::string& name,
    std::unordered_set<std::string>& alreadyIncludedFiles) const {
    std::optional<std::string> src = sourceProvider_.getSource(SourceType::Include, name);
    if (src.has_value()) {
        return process<SourceType::Include>(src.value(), alreadyIncludedFiles);
    }
    return src;
}
