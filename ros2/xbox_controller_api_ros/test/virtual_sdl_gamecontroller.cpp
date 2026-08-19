/**
 * @file virtual_sdl_gamecontroller.cpp
 * @brief Attaches a test-only SDL virtual controller through LD_PRELOAD.
 * @details Interposes SDL_InitSubSystem so an enabled test process receives a
 *          virtual GameController before the production SDL source opens it.
 *          The joystick handle intentionally survives for process lifetime.
 */

#include <SDL.h>

#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <mutex>

namespace
{

    /// @brief Test-only opt-in environment variable for virtual-controller setup.
    constexpr char kVirtualControllerSentinel[] = "XBOX_CONTROLLER_API_TEST_VIRTUAL_CONTROLLER";

    /// @brief Stable device name used only by the launch-test preload fixture.
    constexpr char kVirtualControllerName[] = "xbox_controller_api CI virtual controller";

    /// @brief SDL's dynamically resolved implementation signature.
    using FSdlInitSubSystem = int(SDLCALL *)(Uint32);

    /// @brief Keep the virtual joystick open so its seeded state remains available.
    SDL_Joystick *pVirtualJoystick_ = nullptr;

    /// @brief Restrict attachment to processes started by the virtual launch test.
    [[nodiscard]] bool IsVirtualControllerEnabled() noexcept
    {
        const char *const pSentinelValue_ = std::getenv(kVirtualControllerSentinel);

        return pSentinelValue_ != nullptr && std::strcmp(pSentinelValue_, "1") == 0;
    }

    /// @brief Seed the deterministic controller state used by later active tests.
    /// @param pVirtualJoystick Open virtual joystick that owns the mutable state.
    void SeedVirtualControllerState(SDL_Joystick *pVirtualJoystick) noexcept
    {
        // Provide the library's documented stick convention after its Y-axis inversion.
        (void)SDL_JoystickSetVirtualAxis(
            pVirtualJoystick, SDL_CONTROLLER_AXIS_LEFTX, SDL_JOYSTICK_AXIS_MAX);
        (void)SDL_JoystickSetVirtualAxis(
            pVirtualJoystick, SDL_CONTROLLER_AXIS_LEFTY, SDL_JOYSTICK_AXIS_MIN);
        (void)SDL_JoystickSetVirtualAxis(
            pVirtualJoystick, SDL_CONTROLLER_AXIS_RIGHTX, SDL_JOYSTICK_AXIS_MIN);
        (void)SDL_JoystickSetVirtualAxis(
            pVirtualJoystick, SDL_CONTROLLER_AXIS_RIGHTY, SDL_JOYSTICK_AXIS_MAX);
        (void)SDL_JoystickSetVirtualAxis(
            pVirtualJoystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_JOYSTICK_AXIS_MIN);
        (void)SDL_JoystickSetVirtualAxis(
            pVirtualJoystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, SDL_JOYSTICK_AXIS_MIN);

        // Reset every advertised button before setting the two positive test inputs.
        for (int i32ButtonIndex_ = 0; i32ButtonIndex_ < SDL_CONTROLLER_BUTTON_MAX;
             ++i32ButtonIndex_)
        {
            (void)SDL_JoystickSetVirtualButton(pVirtualJoystick, i32ButtonIndex_, SDL_RELEASED);
        }
        (void)SDL_JoystickSetVirtualButton(pVirtualJoystick, SDL_CONTROLLER_BUTTON_A, SDL_PRESSED);
        (void)SDL_JoystickSetVirtualButton(
            pVirtualJoystick, SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_PRESSED);
    }

    /// @brief Attach and seed one virtual GameController after SDL has initialized.
    void AttachVirtualController() noexcept
    {
        SDL_VirtualJoystickDesc strDescriptor_{};
        strDescriptor_.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
        strDescriptor_.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
        strDescriptor_.naxes = static_cast<Uint16>(SDL_CONTROLLER_AXIS_MAX);
        strDescriptor_.nbuttons = static_cast<Uint16>(SDL_CONTROLLER_BUTTON_MAX);
        strDescriptor_.axis_mask = (1U << SDL_CONTROLLER_AXIS_MAX) - 1U;
        strDescriptor_.button_mask = (1U << SDL_CONTROLLER_BUTTON_MAX) - 1U;
        strDescriptor_.vendor_id = 0x045EU;
        strDescriptor_.product_id = 0x0B13U;
        strDescriptor_.name = kVirtualControllerName;

        const int i32DeviceIndex_ = SDL_JoystickAttachVirtualEx(&strDescriptor_);
        if (i32DeviceIndex_ < 0)
        {
            return;
        }

        pVirtualJoystick_ = SDL_JoystickOpen(i32DeviceIndex_);
        if (pVirtualJoystick_ == nullptr)
        {
            return;
        }

        SeedVirtualControllerState(pVirtualJoystick_);
    }

} // namespace

/// @brief Forward SDL initialization and attach the enabled virtual controller.
/// @param ui32Flags SDL subsystem flags requested by the production caller.
/// @return Result returned by SDL's next implementation in the loader chain.
extern "C" int SDLCALL SDL_InitSubSystem(Uint32 ui32Flags)
{
    static const auto pRealSdlInitSubSystem_ =
        reinterpret_cast<FSdlInitSubSystem>(dlsym(RTLD_NEXT, "SDL_InitSubSystem"));
    if (pRealSdlInitSubSystem_ == nullptr)
    {
        return -1;
    }

    const int i32Result_ = pRealSdlInitSubSystem_(ui32Flags);
    if (i32Result_ != 0 || (ui32Flags & SDL_INIT_GAMECONTROLLER) == 0U ||
        !IsVirtualControllerEnabled())
    {
        return i32Result_;
    }

    // Multiple game-controller initializations share one deterministic device.
    static std::once_flag objAttachOnce_;
    std::call_once(objAttachOnce_, AttachVirtualController);

    return i32Result_;
}
