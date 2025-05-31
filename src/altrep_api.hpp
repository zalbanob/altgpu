#include <R_ext/Altrep.h>
#include <R_ext/Rdynload.h>

#include <type_traits>

template<typename T>
struct AltrepTraits;

template<>
struct AltrepTraits<double> {
    using value_type = double;
    static constexpr auto make_class        = R_make_altreal_class;
    static constexpr auto set_elt_method    = R_set_altreal_Elt_method;
    static constexpr auto set_region_method = R_set_altreal_Get_region_method;
};

template<>
struct AltrepTraits<int> {
    using value_type = int;
    static constexpr auto make_class        = R_make_altinteger_class;
    static constexpr auto set_elt_method    = R_set_altinteger_Elt_method;
    static constexpr auto set_region_method = R_set_altinteger_Get_region_method;
};

template<typename T>
struct Altrep_C_API {
    using Traits = AltrepTraits<T>;
    
    static R_altrep_class_t MakeClass(const char* fullname, const char* pkg, DllInfo* dll) {
        return Traits::make_class(fullname, pkg, dll);
    }
    
    static void SetEltMethod(R_altrep_class_t cls, T (*fn)(SEXP, R_xlen_t)) {
        Traits::set_elt_method(cls, fn);
    }
    
    static void SetGetRegionMethod(R_altrep_class_t cls, R_xlen_t (*fn)(SEXP, R_xlen_t, R_xlen_t, T*)) {
        Traits::set_region_method(cls, fn);
    }
};
