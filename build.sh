# Bash script to simplify the CMake build
# Debug build:   cmake -DCMAKE_BUILD_TYPE=Debug [options] ... CMakeLists.txt
# Release build: cmake -DCMAKE_BUILD_TYPE=Release [options] ... CMakeLists.txt

# Builds Falcon
printf "Building Falcon with CMake...\n"
cmake -DCMAKE_BUILD_TYPE=Debug CMakeLists.txt || exit 1
printf "\nRunning Make...\n"
make || exit 1

# Starts the Falcon's REPL
printf "\nStarting Falcon's REPL...\n"
./bin/falcon
