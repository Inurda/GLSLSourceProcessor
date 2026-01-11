// MIT License
//
// Copyright (c) 2026 Levin Tertilt
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <format>
#include <fstream>
#include <iostream>
#include <ranges>

template<bool LOG_AS_ERROR>
void STDIOLogger::log(const std::string& message) {
    if constexpr (LOG_AS_ERROR) {
        std::cerr << "[GLSL] " << message << std::endl;
    } else {
        std::cout << "[GLSL] " << message << std::endl;
    }
}

template<bool PRINT_IO_ERRORS, auto LOGGER>
static std::optional<std::string> readString(const std::filesystem::path& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        if constexpr (PRINT_IO_ERRORS) {
            LOGGER(std::format("Failed to open shader file \"{}\"", filepath.string()));
        }
        return std::nullopt;
    }

    const std::ifstream::pos_type fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::string buffer(fileSize, '\0');
    if (!file.read(buffer.data(), fileSize)) {
        if constexpr (PRINT_IO_ERRORS) {
            LOGGER(std::format("Failed to read shader file \"{}\"", filepath.string()));
        }
        return std::nullopt;
    }

    return buffer;
}

template<bool PRINT_IO_ERRORS, auto LOGGER>
std::optional<std::string> SimpleFileProvider<PRINT_IO_ERRORS, LOGGER>::getSource(const std::string& name,
    SourceType) {
    return readString<PRINT_IO_ERRORS, LOGGER>(name);
}

template<bool PRINT_IO_ERRORS, auto LOGGER>
std::optional<std::string> CachedFileProvider<PRINT_IO_ERRORS, LOGGER>::getSource(const std::string& name,
    SourceType) const {
    if (const auto it = sourceMap_.find(name); it != sourceMap_.end()) {
        return it->second;
    }
    std::optional<std::string> sourceString = readString<PRINT_IO_ERRORS, LOGGER>(name);
    if (sourceString.has_value()) {
        sourceMap_.emplace(name, sourceString.value());
    }

    return std::make_optional(std::move(*sourceString));
}

template<bool PRINT_IO_ERRORS, auto LOGGER>
std::optional<std::string> SplitDirectoryFileProvider<PRINT_IO_ERRORS, LOGGER>::getSource(const std::string& name,
    const SourceType type) const {
    if (type == SourceType::Include) {
        return readString<PRINT_IO_ERRORS, LOGGER>(includeRoot_ / name);
    }
    return readString<PRINT_IO_ERRORS, LOGGER>(srcRoot_ / name);
}

template<SourceProvider SOURCE_PROVIDER, bool PRINT_SYNTAX_ERRORS, auto LOGGER>
std::optional<std::string> GLSLSourceProcessor<SOURCE_PROVIDER, PRINT_SYNTAX_ERRORS, LOGGER>::getShaderSource(const
    std::string&
    name) const {
    std::optional<std::string> src = sourceProvider_.getSource(name, SourceType::Source);
    if (src.has_value()) {
        std::unordered_set<std::string> alreadyIncludedFiles;
        return process<SourceType::Source>(src.value(), alreadyIncludedFiles);
    }
    return src;
}

template<SourceProvider SOURCE_PROVIDER, bool PRINT_SYNTAX_ERRORS, auto LOGGER>
template<SourceType TYPE>
std::optional<std::string> GLSLSourceProcessor<SOURCE_PROVIDER, PRINT_SYNTAX_ERRORS, LOGGER>::process(
    std::string_view source, std::unordered_set<std::string>& alreadyIncludedFiles) const {
    std::string result;

    // Rough estimate. Definitely improvable
    result.reserve(static_cast<int>(static_cast<float>(source.size()) * 1.5f));

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

    for (auto lineRange : std::ranges::split_view(source, '\n')) {
        std::string_view line(lineRange.begin(), lineRange.end());

        constexpr std::string_view INCLUDE_PREFIX = "#include";

        if (line.starts_with(INCLUDE_PREFIX)) {
            size_t start = line.find('\"');
            if (start == std::string_view::npos) {
                if constexpr (PRINT_SYNTAX_ERRORS) {
                    LOGGER(std::format("#include declared but no value at '{}'", line));
                }
                return std::nullopt;
            }

            size_t end = line.find('\"', start + 1);
            if (end == std::string_view::npos) {
                if constexpr (PRINT_SYNTAX_ERRORS) {
                    LOGGER(std::format("missing closing '\"' in include at '{}'", line));
                }
                return std::nullopt;
            }

            std::string includeName(line.substr(start + 1, end - start - 1));
            if (alreadyIncludedFiles.contains(includeName))
                continue;

            alreadyIncludedFiles.insert(includeName);

            std::optional<std::string> include = getShaderInclude(includeName, alreadyIncludedFiles);
            if (!include.has_value()) {
                if constexpr (PRINT_SYNTAX_ERRORS) {
                    LOGGER(std::format("Failed to include file: {}", includeName));
                }
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

template<SourceProvider SOURCE_PROVIDER, bool PRINT_SYNTAX_ERRORS, auto LOGGER>
std::optional<std::string> GLSLSourceProcessor<SOURCE_PROVIDER, PRINT_SYNTAX_ERRORS, LOGGER>::getShaderInclude(
    const std::string& name, std::unordered_set<std::string>& alreadyIncludedFiles) const {
    std::optional<std::string> src = sourceProvider_.getSource(name, SourceType::Include);
    if (src.has_value()) {
        return process<SourceType::Include>(src.value(), alreadyIncludedFiles);
    }
    return src;
}
