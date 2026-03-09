#/bin/bash
if [[ "$OSTYPE" == "darwin"* ]]; then
        mkdir -p build
        cd build
        cmake .. -DCMAKE_PREFIX_PATH="$(brew --prefix llvm)"
        cmake --build .
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        mkdir -p build
        cd build
        cmake ..
        cmake --build .
else
        echo "Unsupported OS: $OSTYPE"
        exit 1
fi