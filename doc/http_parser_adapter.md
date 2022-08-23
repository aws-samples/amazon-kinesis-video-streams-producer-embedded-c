# HTTP parser adapter

This document addresses how to switch and use a proprietary HTTP parser.

## Current solution: llhttp

By default, we use *llhttp* as our HTTP parser. And it's configured by the CMake option `USE_LLHTTP`. With this option enabled, it will use the source `src/source/http_parser_adapter_llhttp.c` as the HTTP parser implementation.

*llhttp* is a good HTTP parser library that has good performance and is well tested. However, *llhttp* may not work properly on some platforms for a specific reason. In this situation, we might need to replace the llhttp solution.

## How to change solution

We provide a default HTTP parser solution that **IS NOT BUG-FREE**. This default solution's only purpose is to demonstrate how to replace the HTTP parser with your own solution.

By disabling the CMake option `USE_LLHTTP`, it will use the source `src/source/http_parser_adapter_default.c` as the HTTP parser implementation. And you can refer to this source code to write your own code.
