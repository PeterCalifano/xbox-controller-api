#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cuda_runtime.h>
#include <vector>

namespace
{
    bool HasGpu()
    {
        int numDevices = 0;
        const cudaError_t status = cudaGetDeviceCount(&numDevices);
        return (status == cudaSuccess) && (numDevices > 0);
    }
} // namespace

// Placeholder CUDA test demonstrating the FIXTURES_REQUIRED;Cuda scheme. Because
// the file name matches the test*.cpp glob, the shared add_tests() helper picks
// it up and (via tests/template_cuda/CMakeLists.txt) tags it with
// FIXTURES_REQUIRED;Cuda, so it depends on the "CUDA initialization gate":
//   - gate passes -> this test runs.
//   - gate fails  -> CTest reports this test as "Not Run".
//   - no GPU      -> the gate SKIPs and the HasGpu() guard below SKIPs this too.
// Replace this with real CUDA-dependent tests in an instantiated project.
TEST_CASE("CUDA placeholder: round-trips a buffer through device memory", "[cuda][placeholder]")
{
    if (!HasGpu())
    {
        SKIP("No CUDA-capable GPU detected; skipping CUDA placeholder test.");
    }

    constexpr int elementCount = 256;
    constexpr std::size_t byteCount = elementCount * sizeof(int);

    std::vector<int> hostInput(elementCount);
    for (int i = 0; i < elementCount; ++i)
    {
        hostInput[i] = i;
    }

    int *devicePtr = nullptr;
    REQUIRE(cudaMalloc(&devicePtr, byteCount) == cudaSuccess);
    REQUIRE(cudaMemcpy(devicePtr, hostInput.data(), byteCount, cudaMemcpyHostToDevice) == cudaSuccess);

    std::vector<int> hostOutput(elementCount, -1);
    REQUIRE(cudaMemcpy(hostOutput.data(), devicePtr, byteCount, cudaMemcpyDeviceToHost) == cudaSuccess);
    REQUIRE(cudaFree(devicePtr) == cudaSuccess);

    REQUIRE(hostOutput == hostInput);
}
