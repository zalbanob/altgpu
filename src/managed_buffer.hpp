#include <Rcpp.h>
#include <iostream>
#include <cuda_runtime.h>

#include "altgpu.h"



class ManagedBuffer {
    private:
        void*    cpu_ptr      = nullptr;
        void*    gpu_ptr      = nullptr;
        size_t   size         = 0;
        bool     host_dirty   = false;
        bool     device_dirty = false;

        void createOnDevice() {
            if (gpu_ptr == nullptr) {
                cudaError_t err = cudaMalloc(&gpu_ptr, size);
                if (err != cudaSuccess) {
                    std::cerr << "cudaMalloc failed: " << cudaGetErrorString(err) << std::endl;
                    std::abort();
                }
            }
        }

        void createOnHost() {
            if (cpu_ptr == nullptr) {
                cpu_ptr = std::malloc(size);
                if (!cpu_ptr) {
                    std::cerr << "malloc failed" << std::endl;
                    std::abort();
                }
            }
        }

public:
    ManagedBuffer() = default;

    // Destructor: free both sides if allocated
    ~ManagedBuffer() {freeAll();}

    // Disable copy (to avoid double‐free issues)
    ManagedBuffer(const ManagedBuffer&) = delete;
    ManagedBuffer& operator=(const ManagedBuffer&) = delete;
    ManagedBuffer(ManagedBuffer&& other) noexcept { // Allow move (transfers ownership of pointers)
        cpu_ptr      = other.cpu_ptr;
        gpu_ptr      = other.gpu_ptr;
        size         = other.size;
        host_dirty   = other.host_dirty;
        device_dirty = other.device_dirty;

        other.cpu_ptr      = nullptr;
        other.gpu_ptr      = nullptr;
        other.size         = 0;
        other.host_dirty   = false;
        other.device_dirty = false;
    }

    ManagedBuffer& operator=(ManagedBuffer&& other) noexcept {
        if (this != &other) {
            freeAll();

            cpu_ptr      = other.cpu_ptr;
            gpu_ptr      = other.gpu_ptr;
            size         = other.size;
            host_dirty   = other.host_dirty;
            device_dirty = other.device_dirty;

            other.cpu_ptr      = nullptr;
            other.gpu_ptr      = nullptr;
            other.size         = 0;
            other.host_dirty   = false;
            other.device_dirty = false;
        }
        return *this;
    }

    
    void allocateHost(size_t bytes) {
        if (size != 0) {
            std::cerr << "Buffer already allocated" << std::endl;
            std::abort();
        }
        size = bytes;
        createOnHost();
        host_dirty   = true;
        device_dirty = false;
    }

    void allocateDevice(size_t bytes) {
        if (size != 0 && size != bytes) {
            std::cerr << "Size mismatch" << std::endl;
            std::abort();
        }
        if (size == 0) size = bytes;
        createOnDevice();
        device_dirty = true;
        host_dirty   = false;
    }

    void freeAll() {
        if (cpu_ptr) { std::free(cpu_ptr); cpu_ptr = nullptr; }
        if (gpu_ptr) { cudaFree(gpu_ptr) ; gpu_ptr = nullptr; }
        size         = 0;
        host_dirty   = false;
        device_dirty = false;
    }

    void markHost() {
        if (size == 0) {
            std::cerr << "Cannot mark host dirty: buffer is empty" << std::endl;
            std::abort();
        }
        host_dirty   = true;
        device_dirty = false;
    }

    void markDevice() {
        if (size == 0) {
            std::cerr << "Cannot mark device dirty: buffer is empty" << std::endl;
            std::abort();
        }
        device_dirty = true;
        host_dirty   = false;
    }

    bool hasHost  () const {return (cpu_ptr != nullptr);}
    bool hasDevice() const {return (gpu_ptr != nullptr);}
    bool isSynced () const { return (!host_dirty && !device_dirty);}

    bool hostNewer  () const { return (host_dirty    && !device_dirty);}
    bool deviceNewer() const {return (device_dirty && !host_dirty);}

    // The sync() function:
    // - If host is newer, allocate device (if needed) and cudaMemcpy to device.
    // - If device is newer, allocate host (if needed) and cudaMemcpy to host.
    // - Afterwards, both dirty flags are cleared.
    // - If neither is dirty (already synced) or size==0, does nothing.
    void sync() {
        if (size == 0) return; 
        if (hostNewer()) {  // Host to Device
            createOnDevice();
            cudaError_t cpyErr = cudaMemcpy(gpu_ptr, cpu_ptr, size, cudaMemcpyHostToDevice);
            if (cpyErr != cudaSuccess) {
                std::cerr << "cudaMemcpy H2D failed: " << cudaGetErrorString(cpyErr) << std::endl;
                std::abort();
            }
            host_dirty   = false;
            device_dirty = false;
        }
        else if (deviceNewer()) { // Device to Host
            createOnHost();
            cudaError_t cpyErr = cudaMemcpy(cpu_ptr, gpu_ptr, size, cudaMemcpyDeviceToHost);
            if (cpyErr != cudaSuccess) {
                std::cerr << "cudaMemcpy D2H failed: " << cudaGetErrorString(cpyErr) << std::endl;
                std::abort();
            }
            host_dirty   = false;
            device_dirty = false;
        }
        // else: already synced, do nothing
    }


    void*  getHostPtr()   const { return cpu_ptr;}
    void*  getDevicePtr() const { return gpu_ptr;}
    size_t getSize()      const {return size;}
};
