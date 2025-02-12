# Build Cortex.cpp from source

Firstly, clone the Cortex.cpp repository [here](https://github.com/janhq/cortex.cpp) and initialize the submodules:

```bash
git clone https://github.com/janhq/cortex.cpp
cd cortex.cpp
git submodule update --init --recursive
```

You also need to install CMake. On Linux and MacOS, you can install CMake via your package manager

```bash
sudo apt install cmake  # Ubuntu
brew install cmake  # MacOS
```

On Windows, you can download CMake from https://cmake.org/download/.

#### Windows

1. Navigate to the `engine` folder.
2. Configure the vpkg:

```bash
cd vcpkg
./bootstrap-vcpkg.bat
./vcpkg install
```

3. Build the Cortex.cpp inside the `engine/build` folder (you can change `-DCMAKE_TOOLCHAIN_FILE` to use your own `vcpkg`):

```bash
mkdir build
cd build
cmake .. -DBUILD_SHARED_LIBS=OFF -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build . --config Release
```

4. Verify that Cortex.cpp is installed correctly by getting help information.

```sh
cortex -h
```

#### Linux and MacOS

1. Navigate to the `engine` folder.
2. Configure the vpkg:

```bash
cd vcpkg
./bootstrap-vcpkg.sh
./vcpkg install
```

3. Build the Cortex.cpp inside the `engine/build` folder (you can change `-DCMAKE_TOOLCHAIN_FILE` to use your own `vcpkg`):

```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
make -j4
```

4. Verify that Cortex.cpp is installed correctly by getting help information.

```sh
./cortex -h
```

#### Devcontainer / Codespaces

1. Open Cortex.cpp repository in Codespaces or local devcontainer

    [![Open in GitHub Codespaces](https://github.com/codespaces/badge.svg)](https://codespaces.new/janhq/cortex.cpp?quickstart=1)

    ```sh
    devcontainer up --workspace-folder .
    ```

2. Configure vpkg in `engine/vcpkg`:

```bash {"tag": "devcontainer"}
cd engine/vcpkg
export VCPKG_FORCE_SYSTEM_BINARIES="$([[ $(uname -m) == 'arm64' ]] && echo '1' || echo '0')"
./bootstrap-vcpkg.sh
```

3. Build the Cortex.cpp inside the `engine/build` folder:

```bash {"tag": "devcontainer"}
cd engine
mkdir -p build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$(realpath ..)/vcpkg/scripts/buildsystems/vcpkg.cmake
make -j$(grep -c ^processor /proc/cpuinfo)
```

4. Verify that Cortex.cpp is installed correctly by getting help information.

```sh {"tag": "devcontainer"}
cd engine/build
./cortex -h
```

5. Everytime a rebuild is needed, just run the commands above using oneliner

```sh
npx -y runme run --filename README.md -t devcontainer -y
```
