# Lexer-in-C
A lexer implemented in C

### Building
First, download the dependencies:
```bash
./download_dependencies.sh
```

Then, to build the static library, run the following commands from the terminal:
```bash
mkdir build ; cd build && cmake .. && make ; cd ..
```
This will build ```libLexer.a``` in ```./lib``` directory.

### Usage
See ```include/Lexer.h``` for information about functionality provided by this module
