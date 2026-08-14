#include <catch2/catch_test_macros.hpp>
#include <cuda_runtime.h>
#include <sstream>
#include <string>

namespace
{
    bool HasGpu()
    {
        int numDevices = 0;
        const cudaError_t status = cudaGetDeviceCount(&numDevices);
        return (status == cudaSuccess) && (numDevices > 0);
    }

    // Concise, generic guidance shown when the CUDA runtime cannot initialize on
    // a host that does report a GPU. (Downstream GPU projects can extend this
    // with API-specific hints, e.g. an OptiX SDK/driver compatibility note.)
    std::string cudaInitFailureHint(const cudaError_t status)
    {
        std::ostringstream hint;
        hint << "\n\nCUDA runtime failed to initialize: " << cudaGetErrorName(status)
             << " - " << cudaGetErrorString(status) << '\n'
             << "  Common causes:\n"
             << "    - NVIDIA driver older than the CUDA toolkit this binary was built\n"
             << "      against (a CUDA 12.x runtime needs a recent R5xx driver).\n"
             << "    - GPU not exposed to the container. For Podman/Docker pass it via the\n"
             << "      NVIDIA Container Toolkit CDI device: --device nvidia.com/gpu=all.\n"
             << "    - No usable libcudart / driver libraries on the host.\n"
             << "  Run `nvidia-smi` to confirm the driver sees the GPU, then re-check the\n"
             << "  CUDA toolkit/driver compatibility.";
        return hint.str();
    }
} // namespace

// CTest FIXTURES_SETUP gate (wired in tests/template_cuda/CMakeLists.txt). When a
// GPU is present but the CUDA runtime cannot initialize, this test fails and
// CTest reports every FIXTURES_REQUIRED;Cuda test as "Not Run" instead of
// producing N identical CUDA-init failures across the suite. With no GPU it
// skips (exit 0), and the dependent tests fall back to their own HasGpu() guards.
//
// The file name intentionally does NOT match the test*.cpp glob, so the shared
// add_tests() helper ignores it; it is registered by hand as the fixture setup.
TEST_CASE("CUDA initialization gate", "[cuda][setup]")
{
    if (!HasGpu())
    {
        SKIP("No CUDA-capable GPU detected; skipping CUDA initialization gate.");
    }

    // cudaFree(nullptr) is a no-op that still forces lazy CUDA context creation,
    // surfacing driver/runtime incompatibilities here rather than in every test.
    const cudaError_t status = cudaFree(nullptr);
    if (status != cudaSuccess)
    {
        std::ostringstream msg;
        msg << "cudaFree(nullptr) failed: " << cudaGetErrorName(status)
            << " [code=" << static_cast<int>(status) << "] " << cudaGetErrorString(status)
            << cudaInitFailureHint(status);
        FAIL(msg.str());
    }

    SUCCEED("CUDA runtime initialized successfully.");
}
