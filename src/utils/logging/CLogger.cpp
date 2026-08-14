#include "CLogger.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <mutex>

namespace template_project::logging
{
    namespace
    {
        constexpr std::string_view charColorReset_ = "\033[0m";         // Reset color and text intensity.
        constexpr std::string_view charCriticalColor_ = "\033[1;31m";  // Bold red foreground.
        constexpr std::string_view charErrorColor_ = "\033[31m";       // Red foreground.
        constexpr std::string_view charWarningColor_ = "\033[33m";     // Yellow foreground.
        constexpr std::string_view charInfoColor_ = "\033[34m";        // Blue foreground.
        constexpr std::string_view charDebugColor_ = "\033[36m";       // Cyan foreground.
        constexpr std::string_view charTraceColor_ = "\033[2m";        // Dimmed default foreground.

        std::mutex &GetOutputMutex_()
        {
            // One lock coordinates every CLogger instance, including instances that
            // independently target the same std::cout, std::clog, or capture stream.
            static std::mutex objOutputMutex_;
            return objOutputMutex_;
        }

        std::string_view TrimAsciiWhitespace_(std::string_view charText_)
        {
            while (!charText_.empty() &&
                   std::isspace(static_cast<unsigned char>(charText_.front())) != 0)
            {
                charText_.remove_prefix(1);
            }
            while (!charText_.empty() &&
                   std::isspace(static_cast<unsigned char>(charText_.back())) != 0)
            {
                charText_.remove_suffix(1);
            }
            return charText_;
        }
    } // namespace

    CLogger::CLogger(std::string charComponentName, const ELogLevel enumLevel,
                     const ELogColorMode enumColorMode, std::ostream &objOutputStream,
                     std::ostream &objDiagnosticStream)
        : charComponentName_(charComponentName.empty() ? "template_project"
                                                       : std::move(charComponentName)),
          enumLevel_(enumLevel), enumColorMode_(enumColorMode), objOutputStream_(objOutputStream),
          objDiagnosticStream_(objDiagnosticStream)
    {
    }

    void CLogger::setLevel(const ELogLevel enumLevel) noexcept
    {
        enumLevel_.store(enumLevel, std::memory_order_relaxed);
    }

    ELogLevel CLogger::getLevel() const noexcept
    {
        return enumLevel_.load(std::memory_order_relaxed);
    }

    bool CLogger::shouldLog(const ELogLevel enumSeverity) const noexcept
    {
        const ELogLevel enumConfiguredLevel_ = getLevel();
        const auto uiConfiguredLevel_ = static_cast<std::uint8_t>(enumConfiguredLevel_);
        const auto uiSeverity_ = static_cast<std::uint8_t>(enumSeverity);

        if (enumConfiguredLevel_ == ELogLevel::Quiet || enumSeverity == ELogLevel::Quiet)
        {
            return false;
        }

        // Reject values produced by an invalid enum cast instead of accidentally
        // treating them as a threshold more verbose than Trace.
        if (uiConfiguredLevel_ > static_cast<std::uint8_t>(ELogLevel::Trace) ||
            uiSeverity_ > static_cast<std::uint8_t>(ELogLevel::Trace))
        {
            return false;
        }

        return uiSeverity_ <= uiConfiguredLevel_;
    }

    std::optional<ELogLevel> CLogger::tryParseLevel(std::string_view charLevelText)
    {
        charLevelText = TrimAsciiWhitespace_(charLevelText);
        if (charLevelText.size() == 1 && charLevelText.front() >= '0' &&
            charLevelText.front() <= '6')
        {
            return static_cast<ELogLevel>(charLevelText.front() - '0');
        }

        std::string charNormalizedLevel_(charLevelText);
        std::transform(charNormalizedLevel_.begin(), charNormalizedLevel_.end(),
                       charNormalizedLevel_.begin(),
                       [](const unsigned char charValue_)
                       {
                           return static_cast<char>(std::tolower(charValue_));
                       });

        if (charNormalizedLevel_ == "quiet" || charNormalizedLevel_ == "off")
        {
            return ELogLevel::Quiet;
        }
        if (charNormalizedLevel_ == "critical" || charNormalizedLevel_ == "fatal")
        {
            return ELogLevel::Critical;
        }
        if (charNormalizedLevel_ == "error")
        {
            return ELogLevel::Error;
        }
        if (charNormalizedLevel_ == "warning" || charNormalizedLevel_ == "warn")
        {
            return ELogLevel::Warning;
        }
        if (charNormalizedLevel_ == "info")
        {
            return ELogLevel::Info;
        }
        if (charNormalizedLevel_ == "debug")
        {
            return ELogLevel::Debug;
        }
        if (charNormalizedLevel_ == "trace")
        {
            return ELogLevel::Trace;
        }
        return std::nullopt;
    }

    bool CLogger::setLevelFromEnvironment(const std::string_view charVariableName)
    {
        if (charVariableName.empty())
        {
            return false;
        }

        const std::string charVariableNameCopy_(charVariableName);
        const char *charEnvironmentValue_ = std::getenv(charVariableNameCopy_.c_str());
        if (charEnvironmentValue_ == nullptr)
        {
            return false;
        }

        const std::optional<ELogLevel> enumParsedLevel_ = tryParseLevel(charEnvironmentValue_);
        if (!enumParsedLevel_.has_value())
        {
            return false;
        }

        setLevel(*enumParsedLevel_);
        return true;
    }

    void CLogger::writeMessage_(const ELogLevel enumSeverity, const std::string_view charMessage)
    {
        std::ostream &objSelectedStream_ = selectStream_(enumSeverity);
        const bool bUseColor_ = enumColorMode_ == ELogColorMode::Enabled;

        // Build the complete line before taking the shared output lock. Only the
        // final stream operation is serialized across logger instances.
        std::ostringstream objFormattedLineStream_;
        if (bUseColor_)
        {
            objFormattedLineStream_ << getColorCode_(enumSeverity);
        }
        objFormattedLineStream_ << '[' << charComponentName_ << "][" << getLevelLabel_(enumSeverity)
                                << "] " << charMessage;
        if (bUseColor_)
        {
            objFormattedLineStream_ << charColorReset_;
        }
        objFormattedLineStream_ << '\n';

        const std::string charFormattedLine_ = objFormattedLineStream_.str();
        const std::scoped_lock objOutputLock_(GetOutputMutex_());
        objSelectedStream_ << charFormattedLine_;
    }

    std::ostream &CLogger::selectStream_(const ELogLevel enumSeverity) const noexcept
    {
        if (enumSeverity == ELogLevel::Critical || enumSeverity == ELogLevel::Error ||
            enumSeverity == ELogLevel::Warning)
        {
            return objDiagnosticStream_;
        }
        return objOutputStream_;
    }

    std::string_view CLogger::getLevelLabel_(const ELogLevel enumSeverity) noexcept
    {
        switch (enumSeverity)
        {
        case ELogLevel::Critical:
            return "CRITICAL";
        case ELogLevel::Error:
            return "ERROR";
        case ELogLevel::Warning:
            return "WARNING";
        case ELogLevel::Info:
            return "INFO";
        case ELogLevel::Debug:
            return "DEBUG";
        case ELogLevel::Trace:
            return "TRACE";
        case ELogLevel::Quiet:
        default:
            return "QUIET";
        }
    }

    std::string_view CLogger::getColorCode_(const ELogLevel enumSeverity) noexcept
    {
        switch (enumSeverity)
        {
        case ELogLevel::Critical:
            return charCriticalColor_;
        case ELogLevel::Error:
            return charErrorColor_;
        case ELogLevel::Warning:
            return charWarningColor_;
        case ELogLevel::Info:
            return charInfoColor_;
        case ELogLevel::Debug:
            return charDebugColor_;
        case ELogLevel::Trace:
            return charTraceColor_;
        case ELogLevel::Quiet:
        default:
            return {};
        }
    }
} // namespace template_project::logging
