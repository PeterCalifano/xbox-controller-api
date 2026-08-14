#include "CWrapperPlaceholder.h"

#include <iostream>
#include <utility>

namespace cpp_playground
{

    double CWrapperPlaceholder::getDataMember() const
    {
        return a_float_number_;
    }

    void CWrapperPlaceholder::setDataMember(double value)
    {
        a_float_number_ = value;
    }

    std::string CWrapperPlaceholder::getTextData() const
    {
        return charTextData_;
    }

    void CWrapperPlaceholder::setTextDataByConstRef(const std::string &charValue)
    {
        charTextData_ = charValue;
    }

    void CWrapperPlaceholder::setTextDataByValue(std::string charValue)
    {
        charTextData_ = std::move(charValue);
    }

    std::uint32_t CWrapperPlaceholder::echoFrameId(std::uint32_t ui32FrameId) const
    {
        return ui32FrameId;
    }

    void CWrapperPlaceholder::printToStdout() const
    {
        std::cout << "CWrapperPlaceholder text=" << charTextData_ << '\n';
    }

    double CWrapperPlaceholder::multiplyBy2(double value)
    {
        return value * 2.0;
    }

} // namespace cpp_playground
