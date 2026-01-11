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

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

enum class SourceType {
    Source,
    Include
};

struct STDIOLogger {
    template<bool LOG_AS_ERROR>
    static void log(const std::string& message);
};

template<bool PRINT_IO_ERRORS = true, auto LOGGER = STDIOLogger::log<true>>
class SimpleFileProvider {
public:
    explicit SimpleFileProvider(std::filesystem::path root) :
        root_(std::move(root)) {}

    [[nodiscard]] static std::optional<std::string> getSource(const std::string& name, SourceType type);

private:
    std::filesystem::path root_;
};

template<bool PRINT_IO_ERRORS = true, auto LOGGER = STDIOLogger::log<true>>
class CachedFileProvider {
public:
    explicit CachedFileProvider(std::filesystem::path root) :
        root_(std::move(root)) {}

    [[nodiscard]] std::optional<std::string> getSource(const std::string& name, SourceType type) const;

private:
    std::filesystem::path root_;
    mutable std::unordered_map<std::string, std::string> sourceMap_;
};

template<bool PRINT_IO_ERRORS = true, auto LOGGER = STDIOLogger::log<true>>
class SplitDirectoryFileProvider {
public:
    explicit SplitDirectoryFileProvider(const std::filesystem::path& root) :
        srcRoot_(root / "src"),
        includeRoot_(root / "include") {}

    [[nodiscard]] std::optional<std::string> getSource(const std::string& name, SourceType type) const;

private:
    std::filesystem::path srcRoot_;
    std::filesystem::path includeRoot_;
};

template<typename T>
concept SourceProvider = requires(T t)
{
    { t.getSource(
        std::declval<const std::string&>(),
        std::declval<SourceType>()) } -> std::same_as<std::optional<std::string>>;
};

template<typename T>
concept Stringable = requires(T t)
{
    { std::to_string(t) } -> std::convertible_to<std::string>;
};

template<SourceProvider SOURCE_PROVIDER, bool PRINT_SYNTAX_ERRORS = true, auto LOGGER = STDIOLogger::log<true>>
class GLSLSourceProcessor {
public:
    explicit GLSLSourceProcessor(SOURCE_PROVIDER sourceProvider, std::string glslVersion) :
        sourceProvider_(std::move(sourceProvider)),
        glslVersion_(std::move(glslVersion)) {}

    [[nodiscard]] std::optional<std::string> getShaderSource(const std::string& name) const;

    template<Stringable T>
    void define(std::string&& name, T&& value) {
        definitionMap_.emplace(std::move(name), std::to_string(std::forward<T>(value)));
    }

    void define(std::string&& name) { definitionMap_.emplace(std::move(name), ""); }
    void undef(const std::string& name) { definitionMap_.erase(name); }
    void undefAll() { definitionMap_.clear(); }

private:
    template<SourceType TYPE>
    std::optional<std::string> process(std::string_view source, std::unordered_set<std::string>&
        alreadyIncludedFiles) const;
    std::optional<std::string> getShaderInclude(const std::string& name, std::unordered_set<std::string>&
        alreadyIncludedFiles) const;

    SOURCE_PROVIDER sourceProvider_;
    std::string glslVersion_;
    std::unordered_map<std::string, std::string> definitionMap_;
};

#include "glsl_source_processor.inl"
