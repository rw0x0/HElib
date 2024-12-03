#pragma once

#define HRESULT long
#define C_DECOR extern "C"
#define C_FUNC C_DECOR HRESULT

#define _HRESULT_TYPEDEF_(hr) ((HRESULT)hr)

#define E_POINTER _HRESULT_TYPEDEF_(0x80004003L)

#define S_OK _HRESULT_TYPEDEF_(0L)
#define S_FALSE _HRESULT_TYPEDEF_(1L)

#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)

#define IfNullRet(expr, ret)   \
    {                          \
        if ((expr) == nullptr) \
        {                      \
            return ret;        \
        }                      \
    }

#define IfFailRet(expr)          \
    {                            \
        HRESULT __hr__ = (expr); \
        if (FAILED(__hr__))      \
        {                        \
            return __hr__;       \
        }                        \
    }


template <class T>
inline T *FromVoid(void *voidptr)
{
    T *result = reinterpret_cast<T *>(voidptr);
    return result;
}
