/// @file example_project.h
/// @brief Declares the dependencies used by the installed-package example.
/// @details Every include here resolves against the installed header tree, so
///          this file doubles as a check that the exported include layout is
///          usable from outside the source checkout.

#pragma once

#include <utils/logging/CLogger.h>
#include <xbox_controller_api/CScriptedGamepadSource.h>
#include <xbox_controller_api/GamepadFilters.h>
#include <xbox_controller_api/SGamepadState.h>
