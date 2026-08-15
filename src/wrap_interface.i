//*************************************************************************
// Python wrapper definition file.
//*************************************************************************

namespace xbox_controller_api
{

#include <utils/wrap_adapters/GtsamAliases.h>
#include <wrapped_impl/CGamepadWrapper.h>

    class CGamepadWrapper
    {
        CGamepadWrapper();

        static bool isBackendAvailable();

        bool open();
        bool openIndex(int32_t i32JoystickIndex);
        void close();
        bool update();
        bool connected() const;
        string deviceName() const;
        string lastError() const;

        void setStickDeadzone(const double dStickDeadzone);
        double stickDeadzone() const;

        double leftStickX() const;
        double leftStickY() const;
        double rightStickX() const;
        double rightStickY() const;
        double leftTrigger() const;
        double rightTrigger() const;

        bool buttonA() const;
        bool buttonB() const;
        bool buttonX() const;
        bool buttonY() const;
        bool leftShoulder() const;
        bool rightShoulder() const;
        bool leftStickClick() const;
        bool rightStickClick() const;
        bool back() const;
        bool start() const;
        bool guide() const;
        bool dpadUp() const;
        bool dpadDown() const;
        bool dpadLeft() const;
        bool dpadRight() const;

        uint64_t sequenceId() const;

        int32_t buttonCount() const;
        string buttonName(int32_t i32ButtonIndex) const;
        bool buttonPressed(int32_t i32ButtonIndex) const;
    };

} // ACHTUNG: do not add semi-colon here!
