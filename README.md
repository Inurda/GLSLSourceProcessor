# GLSL Preprocessor

A compile-time modular GLSL preprocessor that can handle injection of definitions and the inclusion of other files

By default, multiple implementations for file handling are provided and one implementation for logging to _stdout_, 
but since the processor is fully modular, so you can provide your own implementation at compile time, so no runtime 
overhead

## Setup

Requires C++20 or higher

Just copy the files [glsl_preprocessor.h](glsl_source_processor.h) and [glsl_preprocessor.inl](glsl_source_processor.inl)
into your codebase, and you are ready to go. _glsl_preprocessor.inl_ contains the implementation detail. 

###### CMake

You can clone this repository and add as a target in CMake, like this:

```cmake
add_subdirectory(path/to/this/repository)

target_link_libraries(your_target PRIVATE glsl_sp)
```

###### Usage

You can find a small example on how to use it [here](main.cpp). Alternatively you can study the implementation