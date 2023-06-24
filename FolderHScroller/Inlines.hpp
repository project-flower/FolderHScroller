#pragma once

template <typename TFunction, typename TVariable>
inline void ExecIfNotNull(TFunction function, TVariable variable)
{
    if (variable) function(variable);
}
