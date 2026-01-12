# GLSL Preprocessor

A compile-time modular GLSL preprocessor that can handle injection of definitions and the inclusion of other files

By default, multiple implementations for file handling are provided and one implementation for logging to _stdout_, 
but since the processor is fully modular, so you can provide your own implementation at compile time, so no runtime 
overhead

## Setup

Requires C++20 or higher

###### Simple
You can simply copy the files from in the [include](include) directory into your codebase, since it is header only

###### CMake

You can clone this repository and add as a target in CMake, like this:

```cmake
add_subdirectory(path/to/this/repository)

target_link_libraries(your_target PRIVATE glsl_sp)
```

Then you can include the file, like this:

```c++
#include <glsl/glsl_source_processor.h>
```

Minimal example:
```c++
#include <iostream>
#include <optional>

#include <glsl/glsl_source_processor.h>

int main() {
    FileSourceProvider sourceProvider(SillyFileProvider{}, SplitDirectories("shaders"));

    GLSLSourceProcessor processor(sourceProvider, "#version 450 core");

    processor.define("ALPHA_CUTOUT_THRESHOLD", 0.3f);
    processor.define("TEST_FLAG1");
    processor.define("TEST_FLAG2");
    processor.define("TEST_FLAG3");

    processor.undef("TEST_FLAG3");

    std::optional<std::string> source = processor.getShaderSource("example.glsl");

    if (source.has_value()) {
        std::cout << "After:\n" << source.value() << std::endl;
    }
}
```

###### Usage

You can find a small example on how to use it [here](main.cpp). Alternatively you can study the implementation