#pragma once

#ifndef ReleaseCom
#define ReleaseCom(x)                                                                                                  \
    {                                                                                                                  \
        if (x)                                                                                                         \
        {                                                                                                              \
            x->Release();                                                                                              \
            x = 0;                                                                                                     \
        }                                                                                                              \
    }
#endif
