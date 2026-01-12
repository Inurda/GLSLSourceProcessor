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
#include <format>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

// TODO : Distinguish between cyclic inclusion (Error: A -> B -> A) and shared includes (Ok: A -> B, C, B -> C)
// TODO : Also support <> brackets for including instead of solely quotation marks
// TODO : Give the user the option to retrieve any faulty sources, in order to identify bugs that are generated after
//  processing is applied
// TODO : Maybe wrap this class inside a namespace to avoid possible conflicts, when used in a existing project
// TODO : Add more documentation

enum class SourceType {
    Source,
    Include
};

using LoggingImpl = void(*)(std::string_view);

constexpr LoggingImpl DISABLED_LOGGING = [](std::string_view) {};

/// Writes any generated error messages to the stdout stream
struct STDIOLogging {
    // Writes to std::cout
    static void log(std::string_view msg);

    // Writes to std::cerr
    static void logAsError(std::string_view msg);
};

template<typename T>
concept PathPolicy = requires(T t)
{
    { t.getFilepath(std::declval<SourceType>(), std::declval<std::string_view>()) } ->
        std::convertible_to<std::filesystem::path>;
};

/// An implementation of the PathPolicy concept that allows for the separation of source and include files in two
/// different directories
class SplitDirectories {
public:
    explicit SplitDirectories(const std::filesystem::path& root) :
        srcRoot_(root / "src"),
        includeRoot_(root / "include") {}
    SplitDirectories(std::filesystem::path srcRoot, std::filesystem::path includeRoot) :
        srcRoot_(std::move(srcRoot)),
        includeRoot_(std::move(includeRoot)) {}

    [[nodiscard]] std::filesystem::path getFilepath(SourceType type, std::string_view name) const {
        if (type == SourceType::Include) {
            return includeRoot_ / name;
        }
        return srcRoot_ / name;
    }

private:
    std::filesystem::path srcRoot_;
    std::filesystem::path includeRoot_;
};

template<typename T>
concept FileProviderImpl = requires(T t)
{
    { t.getString(std::declval<const std::filesystem::path&>()) } -> std::convertible_to<std::optional<std::string>>;
};

/// The default implementation of file provider. All files are loaded from disk for each request
struct SillyFileProvider {
    static std::optional<std::string> getString(const std::filesystem::path& filepath);
};

/// An implementation that caches files. This may be a good choice if the shader files never change at runtime. If
/// you use mechanism to reload files at runtime, you should refrain from using this as the contents are not updated
/// after they are in memory
class CachedFileProvider {
public:
    std::optional<std::string> getString(const std::filesystem::path& filepath) const;

private:
    mutable std::unordered_map<std::string, std::string> cache_;
};

/// An implementation that caches files, but additionally checks whether the resource has been modified, and if so
/// refetch that file from the file system
class SmartCachedFileProvider {
public:
    struct CacheKey {
        std::filesystem::path filepath;
        std::filesystem::file_time_type lastWrite;
        std::uintmax_t fileSize;

        explicit CacheKey(const std::filesystem::path& filepath) :
            filepath(filepath),
            lastWrite(std::filesystem::last_write_time(filepath)),
            fileSize(std::filesystem::file_size(filepath)) {}

        bool operator==(const CacheKey& rhs) const = default;
    };
    struct CacheKeyHasher {
        std::size_t operator()(const CacheKey& key) const noexcept {
            std::size_t hash = 0;

            auto combineHash = [&hash](std::size_t v) {
                hash ^= v + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
            };

            combineHash(std::hash<std::filesystem::path>{}(key.filepath));
            combineHash(std::hash<long long>{}(
                key.lastWrite.time_since_epoch().count()));
            combineHash(std::hash<std::uintmax_t>{}(key.fileSize));

            return hash;
        }
    };

    std::optional<std::string> getString(const std::filesystem::path& filepath) const;

private:
    mutable std::unordered_map<CacheKey, std::string, CacheKeyHasher> cache_;
};

template<typename T>
concept SourceProvider = requires(T t)
{
    { t.getSource(std::declval<SourceType>(), std::declval<std::string_view>()) } ->
        std::same_as<std::optional<std::string>>;
};

/// An implementation of SourceProvider that reads the shader sources from the file system
template<FileProviderImpl IMPL = SillyFileProvider, PathPolicy PATH_POLICY = SplitDirectories>
class FileSourceProvider {
public:
    explicit FileSourceProvider(IMPL impl = IMPL{}, PATH_POLICY policy = PATH_POLICY{}, LoggingImpl log = DISABLED_LOGGING) :
        impl_(std::move(impl)),
        policy_(std::move(policy)),
        log_(log) {}

    std::optional<std::string> getSource(SourceType type, std::string_view name) const {
        auto filepath = policy_.getFilepath(type, name);
        auto source = impl_.getString(filepath);
        if (!source.has_value()) {
            log_(std::format("Failed to open/read shader file: {}", filepath.string()));
        }
        return source;
    }

private:
    IMPL impl_;
    PATH_POLICY policy_;
    LoggingImpl log_;
};

template<typename T>
concept Stringable = requires(T t)
{
    { std::to_string(t) } -> std::convertible_to<std::string>;
};

template<SourceProvider SOURCE_PROVIDER>
class GLSLSourceProcessor {
public:
    explicit GLSLSourceProcessor(SOURCE_PROVIDER sourceProvider = SOURCE_PROVIDER{},
        std::string glslVersion = "#version 450 core", LoggingImpl log = DISABLED_LOGGING) :
        sourceProvider_(std::move(sourceProvider)),
        glslVersion_(std::move(glslVersion)),
        log_(log) {}

    std::optional<std::string> getShaderSource(const std::string& name) const;

    template<Stringable T>
    void define(std::string&& name, T&& value) {
        definitionMap_.insert_or_assign(std::move(name), std::to_string(std::forward<T>(value)));
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
    LoggingImpl log_;
    std::unordered_map<std::string, std::string> definitionMap_;
};

#include "glsl_source_processor.inl"