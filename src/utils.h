#include <Rcpp.h>

#define ALTGPU_ALWAYS_INLINE __attribute__((always_inline))
#define ALTGPU_FLATTEN       __attribute__((flatten)) 

using namespace Rcpp;


ALTGPU_FLATTEN
bool _is_altrep(RObject x){
  return bool(ALTREP(x));
}

ALTGPU_ALWAYS_INLINE
Pairlist _all_attribs(RObject x){
  return ATTRIB(x);
}

ALTGPU_ALWAYS_INLINE
void _assert_altrep(RObject x){
  if (!_is_altrep(x)){
    stop("Not ALTREP!");
  }
}

ALTGPU_ALWAYS_INLINE
RawVector alt_class(RObject x){
  _assert_altrep(x);
  return ALTREP_CLASS(x);
}

ALTGPU_ALWAYS_INLINE
CharacterVector alt_classname(RObject x){
  _assert_altrep(x);
  return as<CharacterVector>(_all_attribs(alt_class(x))[0]);
}


bool _is_altvec_type(RObject x){
  if (!_is_altrep(x)) return FALSE;
  std::string s = Rcpp::as<std::string>(alt_classname(x)[0]);
  return s.find("altvec") != std::string::npos;
}