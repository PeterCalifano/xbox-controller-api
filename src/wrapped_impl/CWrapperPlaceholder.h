#pragma once

#include <cstdint>
#include <string>

namespace cpp_playground
{
    class CWrapperPlaceholder
    {
      public:
        CWrapperPlaceholder() = default;

        double getDataMember() const;
        void setDataMember(double value);
        std::string getTextData() const;
        void setTextDataByConstRef(const std::string &charValue);
        void setTextDataByValue(std::string charValue);
        std::uint32_t echoFrameId(std::uint32_t ui32FrameId) const;
        void printToStdout() const;

        static double multiplyBy2(double value);

      private:
        double a_float_number_{0.0};
        std::string charTextData_{"initial"};
    };

} // namespace cpp_playground
