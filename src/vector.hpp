#include "altgpu.h"
#include "managed_buffer.hpp"
#include "altrep_api.hpp"
#include "debug.hpp"

template<typename Datatype>
class AltGPUVector {
public:
    static R_altrep_class_t class_t;

private:
    R_xlen_t                   length_ = 0;
    std::unique_ptr<ManagedBuffer> buffer;
    
    AltGPUVector(R_xlen_t n) : length_(n), buffer(std::make_unique<ManagedBuffer>()) {
        debug_printf("[AltGPUVector::Constructor] length = %lld\n", (long long) n);
        buffer->allocateHost(static_cast<size_t>(n) * sizeof(Datatype));
        debug_printf("[AltGPUVector::Constructor] Host buffer allocated at %p\n", buffer->getHostPtr());
    }

public:
    AltGPUVector(const AltGPUVector&)            = delete;
    AltGPUVector& operator=(const AltGPUVector&) = delete;

    AltGPUVector(AltGPUVector&&)                 = default;
    AltGPUVector& operator=(AltGPUVector&&)      = default;

    ~AltGPUVector() = default;

    static void Destruct(SEXP ext) {
        debug_printf("[AltGPUVector::Destruct] Entering finalizer\n");
        AltGPUVector<Datatype>* ptr = static_cast<AltGPUVector<Datatype>*>(R_ExternalPtrAddr(ext));
        if (ptr) {
            debug_printf("[AltGPUVector::Destruct] Deleting instance at %p\n", ptr);
            delete ptr;
            R_ClearExternalPtr(ext);
            debug_printf("[AltGPUVector::Destruct] ExternalPtr cleared\n");
        } else {
            debug_printf("[AltGPUVector::Destruct] Pointer was already NULL\n");
        }
    }

    static SEXP Construct(R_xlen_t n) {
        debug_printf("[AltGPUVector::Construct] Requested length n = %lld\n", (long long) n);
        AltGPUVector<Datatype>* instance = new AltGPUVector<Datatype>(n);
        debug_printf("[AltGPUVector::Construct] New instance at %p\n", instance);

        SEXP xp = PROTECT(R_MakeExternalPtr(static_cast<void*>(instance), R_NilValue, R_NilValue));
        R_RegisterCFinalizerEx(xp, AltGPUVector<Datatype>::Destruct, TRUE);
        debug_printf("[AltGPUVector::Construct] ExternalPtr created and finalizer registered\n");

        SEXP res = R_new_altrep(class_t, xp, R_NilValue);
        UNPROTECT(1);
        debug_printf("[AltGPUVector::Construct] ALTREP object created: %p\n", res);
        return res;
    }

    static AltGPUVector<Datatype>* AltGPUVector_Unwrap(SEXP x) {
        debug_printf("[AltGPUVector::Unwrap] Unwrapping ALTREP %p\n", x);
        SEXP xp = R_altrep_data1(x);
        AltGPUVector<Datatype>* unwrapped = static_cast<AltGPUVector<Datatype>*>(R_ExternalPtrAddr(xp));
        if (!unwrapped) {
            debug_printf("[AltGPUVector::Unwrap] ERROR: pointer is NULL (already freed?)\n");
            Rf_error("AltGPUVector appears to have been freed");
        }
        debug_printf("[AltGPUVector::Unwrap] Unwrapped pointer: %p\n", unwrapped);
        return unwrapped;
    }

    static AltGPUVector<Datatype>& Get(SEXP x) {
        AltGPUVector<Datatype>* p = AltGPUVector_Unwrap(x);
        if (!p) {
            debug_printf("[AltGPUVector::Get] ERROR: encountered null pointer\n");
            Rcpp::stop("AltGPUVector::Get() encountered a null pointer!");
        }
        return *p;
    }

    static R_xlen_t Length(SEXP x) {
        AltGPUVector<Datatype>& obj = Get(x);
        debug_printf("[AltGPUVector::Length] ALTREP %p has length %lld\n", x, (long long) obj.length_);
        return obj.length_;
    }

    static Rboolean Inspect(
        SEXP x, int pre, int deep, int pvec,
        void (*inspect_subtree)(SEXP, int, int, int)
    ) {
        AltGPUVector<Datatype>& obj = Get(x);
        Rprintf("[AltGPUVector::Inspect] ALTREP %p\n", x);
        Rprintf("  AltGPUVector<%s> (len=%lld, host_ptr=%p, device_ptr=%p)\n",
                typeid(Datatype).name(),
                (long long) obj.length_,
                obj.buffer->getHostPtr(),
                obj.buffer->getDevicePtr());
        return TRUE;
    }

    static const void* Dataptr_or_null(SEXP x) {
        AltGPUVector<Datatype>& obj = Get(x);
        debug_printf("[AltGPUVector::Dataptr_or_null] Syncing buffer for ALTREP %p\n", x);
        obj.buffer->sync();
        const void* ptr = obj.buffer->getHostPtr();
        debug_printf("[AltGPUVector::Dataptr_or_null] Returning host_ptr = %p\n", ptr);
        return ptr;
    }

    static void* Dataptr(SEXP x, Rboolean writeable) {
        AltGPUVector<Datatype>& obj = Get(x);
        debug_printf("[AltGPUVector::Dataptr] ALTREP %p with writeable=%d\n", x, writeable);
        obj.buffer->sync();
        if (writeable) {
            debug_printf("[AltGPUVector::Dataptr] Marking buffer host as writable\n");
            obj.buffer->markHost();
        }
        void* ptr = obj.buffer->getHostPtr();
        debug_printf("[AltGPUVector::Dataptr] Returning host_ptr = %p\n", ptr);
        return ptr;
    }

    static Datatype elt(SEXP x, R_xlen_t i) {
        AltGPUVector<Datatype>& obj = Get(x);
        debug_printf("[AltGPUVector::elt] ALTREP %p, index i = %lld\n", x, (long long) i);
        obj.buffer->sync();
        Datatype* hptr = static_cast<Datatype*>(obj.buffer->getHostPtr());
        Datatype val = hptr[i];
        debug_printf("[AltGPUVector::elt] Retrieved value at [%lld] = %g\n", (long long) i, static_cast<double>(val));
        return val;
    }

    static R_xlen_t Get_region(
        SEXP   x,
        R_xlen_t start,
        R_xlen_t size,
        Datatype* out
    ) {
        AltGPUVector<Datatype>& obj = Get(x);
        debug_printf("[AltGPUVector::Get_region] ALTREP %p, start=%lld, size=%lld\n",
                x, (long long) start, (long long) size);
        obj.buffer->sync();

        Datatype* hptr = static_cast<Datatype*>(obj.buffer->getHostPtr());
        R_xlen_t remain = obj.length_ - start;
        R_xlen_t ncopy  = (remain < size ? remain : size);

        debug_printf("[AltGPUVector::Get_region] Copying %lld elements from host_ptr+%lld to out\n",
                (long long) ncopy, (long long) start);
        std::memcpy(out, hptr + start, static_cast<size_t>(ncopy) * sizeof(Datatype));
        debug_printf("[AltGPUVector::Get_region] Done copying %lld elements\n", (long long) ncopy);
        return ncopy;
    }

    static void Init(DllInfo* dll, const std::string& class_name) {
        std::string fullname = "altvec_" + class_name;
        debug_printf("[AltGPUVector::Init] Creating ALREP class '%s'\n", fullname.c_str());
        class_t = Altrep_C_API<Datatype>::MakeClass(fullname.c_str(), "altgpu", dll);

        debug_printf("[AltGPUVector::Init] Setting Length method\n");
        R_set_altrep_Length_method          (class_t, Length);

        debug_printf("[AltGPUVector::Init] Setting Inspect method\n");
        R_set_altrep_Inspect_method         (class_t, Inspect);

        debug_printf("[AltGPUVector::Init] Setting Dataptr_or_null method\n");
        R_set_altvec_Dataptr_or_null_method (class_t, Dataptr_or_null);

        debug_printf("[AltGPUVector::Init] Setting Dataptr method\n");
        R_set_altvec_Dataptr_method         (class_t, Dataptr);

        debug_printf("[AltGPUVector::Init] Setting elt method\n");
        Altrep_C_API<Datatype>::SetEltMethod      (class_t, elt);

        debug_printf("[AltGPUVector::Init] Setting get_region method\n");
        Altrep_C_API<Datatype>::SetGetRegionMethod(class_t, Get_region);

        debug_printf("[AltGPUVector::Init] Init complete for '%s'\n", fullname.c_str());
    }
};

template<> R_altrep_class_t AltGPUVector<double>::class_t = {NULL};
template<> R_altrep_class_t AltGPUVector<int>::class_t    = {NULL};

//' Create a new GPU‐backed integer ALTREP
//' @param n integer(1) Length of the new vector.
//' @return An altgpu_integer ALTREP object (backed by GPU buffer).
//' @export
// [[Rcpp::export(rng = false)]]
SEXP altgpuvector_int(R_xlen_t n) {
    debug_printf("[altgpuvector_int] Called with n = %lld\n", (long long) n);
    return AltGPUVector<int>::Construct(n);
}

//' Create a new GPU‐backed real (double) ALTREP
//' @param n integer(1) Length of the new vector.
//' @return An altgpu_double ALTREP object (backed by GPU buffer).
//' @export
// [[Rcpp::export(rng = false)]]
SEXP altgpuvector_real(R_xlen_t n) {
    debug_printf("[altgpuvector_real] Called with n = %lld\n", (long long) n);
    return AltGPUVector<double>::Construct(n);
}
