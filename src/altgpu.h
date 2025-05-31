#ifndef ALTGPU_H
#define ALTGPU_H
#include <R.h>
#include <Rcpp.h>
#include <Rinternals.h>
#include <Rversion.h>



#include <R_ext/Rdynload.h>

#if R_VERSION < R_Version(3, 6, 0)
    #define class klass
    extern "C" {
        #include <R_ext/Altrep.h>
    }
    #undef class
#else
    #include <R_ext/Altrep.h>
#endif

#endif //ALTGPU_H
