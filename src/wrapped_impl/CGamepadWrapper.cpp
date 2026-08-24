/**
 * @file CGamepadWrapper.cpp
 * @brief Implements the flat binding-facing controller facade.
 * @details The facade stores one conditioned snapshot for its scalar accessors.
 */

#include <wrapped_impl/CGamepadWrapper.h>

#include <xbox_controller_api/GamepadControls.h>

#include <cstddef>
#include <span>
#include <string>

namespace xbox_controller_api
{
    namespace
    {
        /// @brief Translate a button index into a control identity.
        /// @return False when the index does not name a control.
        [[nodiscard]] bool TryResolveButton(std::int32_t i32ButtonIndex,
                                            EGamepadButton &enumResolvedButton) noexcept
        {
            const std::span<const EGamepadButton> spanButtons_ = AllGamepadButtons();

            if (i32ButtonIndex < 0 ||
                static_cast<std::size_t>(i32ButtonIndex) >= spanButtons_.size())
            {
                return false;
            }

            enumResolvedButton = spanButtons_[static_cast<std::size_t>(i32ButtonIndex)];

            return true;
        }
    } // namespace

    CGamepadWrapper::CGamepadWrapper() = default;

    CGamepadWrapper::~CGamepadWrapper() = default;

    void CGamepadWrapper::refreshConditionedState_()
    {
        strConditionedState_ = ApplyStickDeadzone(objSource_.state(), dStickDeadzone_);
    }

    bool CGamepadWrapper::isBackendAvailable()
    {
        return CSdlGamepadSource::isBackendAvailable();
    }

    bool CGamepadWrapper::open()
    {
        const bool bOpened_ = objSource_.open();
        refreshConditionedState_();

        return bOpened_;
    }

    bool CGamepadWrapper::openIndex(std::int32_t i32JoystickIndex)
    {
        const bool bOpened_ = objSource_.open(i32JoystickIndex);
        refreshConditionedState_();

        return bOpened_;
    }

    void CGamepadWrapper::close()
    {
        objSource_.close();
        refreshConditionedState_();
    }

    bool CGamepadWrapper::update()
    {
        const bool bUpdated_ = objSource_.update();
        refreshConditionedState_();

        return bUpdated_;
    }

    bool CGamepadWrapper::connected() const
    {
        return strConditionedState_.bConnected_;
    }

    std::string CGamepadWrapper::deviceName() const
    {
        return objSource_.deviceName();
    }

    std::string CGamepadWrapper::lastError() const
    {
        return objSource_.lastError();
    }

    void CGamepadWrapper::setStickDeadzone(double dStickDeadzone)
    {
        // Values outside [0, 1] disable or fully suppress stick input.
        dStickDeadzone_ = dStickDeadzone;

        // Apply the new deadzone to the current snapshot immediately.
        refreshConditionedState_();
    }

    double CGamepadWrapper::stickDeadzone() const
    {
        return dStickDeadzone_;
    }

    double CGamepadWrapper::leftStickX() const
    {
        return strConditionedState_.dLeftStickX_;
    }

    double CGamepadWrapper::leftStickY() const
    {
        return strConditionedState_.dLeftStickY_;
    }

    double CGamepadWrapper::rightStickX() const
    {
        return strConditionedState_.dRightStickX_;
    }

    double CGamepadWrapper::rightStickY() const
    {
        return strConditionedState_.dRightStickY_;
    }

    double CGamepadWrapper::leftTrigger() const
    {
        // Stick deadzones do not affect unidirectional triggers.
        return strConditionedState_.dLeftTrigger_;
    }

    double CGamepadWrapper::rightTrigger() const
    {
        return strConditionedState_.dRightTrigger_;
    }

    bool CGamepadWrapper::buttonA() const
    {
        return GetButton(strConditionedState_, EGamepadButton::A);
    }

    bool CGamepadWrapper::buttonB() const
    {
        return GetButton(strConditionedState_, EGamepadButton::B);
    }

    bool CGamepadWrapper::buttonX() const
    {
        return GetButton(strConditionedState_, EGamepadButton::X);
    }

    bool CGamepadWrapper::buttonY() const
    {
        return GetButton(strConditionedState_, EGamepadButton::Y);
    }

    bool CGamepadWrapper::leftShoulder() const
    {
        return GetButton(strConditionedState_, EGamepadButton::LeftShoulder);
    }

    bool CGamepadWrapper::rightShoulder() const
    {
        return GetButton(strConditionedState_, EGamepadButton::RightShoulder);
    }

    bool CGamepadWrapper::leftStickClick() const
    {
        return GetButton(strConditionedState_, EGamepadButton::LeftStickClick);
    }

    bool CGamepadWrapper::rightStickClick() const
    {
        return GetButton(strConditionedState_, EGamepadButton::RightStickClick);
    }

    bool CGamepadWrapper::back() const
    {
        return GetButton(strConditionedState_, EGamepadButton::Back);
    }

    bool CGamepadWrapper::start() const
    {
        return GetButton(strConditionedState_, EGamepadButton::Start);
    }

    bool CGamepadWrapper::guide() const
    {
        return GetButton(strConditionedState_, EGamepadButton::Guide);
    }

    bool CGamepadWrapper::dpadUp() const
    {
        return GetButton(strConditionedState_, EGamepadButton::DpadUp);
    }

    bool CGamepadWrapper::dpadDown() const
    {
        return GetButton(strConditionedState_, EGamepadButton::DpadDown);
    }

    bool CGamepadWrapper::dpadLeft() const
    {
        return GetButton(strConditionedState_, EGamepadButton::DpadLeft);
    }

    bool CGamepadWrapper::dpadRight() const
    {
        return GetButton(strConditionedState_, EGamepadButton::DpadRight);
    }

    std::uint64_t CGamepadWrapper::sequenceId() const
    {
        return strConditionedState_.ui64SequenceId_;
    }

    std::int32_t CGamepadWrapper::buttonCount() const
    {
        return static_cast<std::int32_t>(AllGamepadButtons().size());
    }

    std::string CGamepadWrapper::buttonName(std::int32_t i32ButtonIndex) const
    {
        EGamepadButton enumButton_ = EGamepadButton::A;
        if (!TryResolveButton(i32ButtonIndex, enumButton_))
        {
            return "Unknown";
        }

        return std::string(GetGamepadButtonName(enumButton_));
    }

    bool CGamepadWrapper::buttonPressed(std::int32_t i32ButtonIndex) const
    {
        EGamepadButton enumButton_ = EGamepadButton::A;
        if (!TryResolveButton(i32ButtonIndex, enumButton_))
        {
            return false;
        }

        return GetButton(strConditionedState_, enumButton_);
    }
} // namespace xbox_controller_api
