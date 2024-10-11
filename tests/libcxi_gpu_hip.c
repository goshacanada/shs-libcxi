/* SPDX-License-Identifier: GPL-2.0-only or BSD-2-Clause */
/* Copyright 2020 Hewlett Packard Enterprise Development LP */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "libcxi_test_common.h"

#ifdef HAVE_HIP_SUPPORT
#include <hip/hip_runtime_api.h>
#include <hip/driver_types.h>

void *libhip_handle;
static hipError_t (*hip_malloc)(void **devPtr, size_t size);
static hipError_t (*hip_host_alloc)(void **devPtr, size_t size, uint flags);
static hipError_t (*hip_free)(void *devPtr);
static hipError_t (*hip_host_free)(void *p);
static hipError_t (*hip_memset)(void *devPtr, int value, size_t count);
static hipError_t (*hip_memcpy)(void *dst, const void *src, size_t count,
				enum hipMemcpyKind kind);
static hipError_t (*hip_device_count)(int *count);
static hipError_t (*hip_device_sync)(void);

static int h_malloc(struct mem_window *win)
{
	hipError_t rc = hip_malloc((void **)&win->buffer, win->length);

	cr_assert_eq(rc, hipSuccess, "malloc() failed %d", rc);
	win->loc = on_device;
	win->is_device = true;

	return 0;
}

static int h_host_alloc(struct mem_window *win)
{
	hipError_t rc = hip_host_alloc((void **)&win->buffer, win->length,
				       hipHostMallocMapped);

	cr_assert_eq(rc, hipSuccess, "hip_host_alloc() failed %d", rc);
	win->loc = on_host;
	win->is_device = true;

	return 0;
}

static int h_free(void *devPtr)
{
	hipError_t rc = hip_free(devPtr);

	cr_assert_eq(rc, hipSuccess, "free() failed %d", rc);

	return 0;
}

static int h_host_free(void *p)
{
	hipError_t rc = hip_host_free(p);

	cr_assert_eq(rc, hipSuccess, "hip_host_free() failed %d", rc);

	return 0;
}

static int h_memset(void *devPtr, int value, size_t count)
{
	hipError_t rc = hip_memset(devPtr, value, count);

	cr_assert_eq(rc, hipSuccess, "memset() failed %d", rc);

	rc = hip_device_sync();
	cr_assert_eq(rc, hipSuccess, "hip_device_sync failed %d", rc);

	return 0;
}

static int h_memcpy(void *dst, const void *src, size_t count,
			  enum gpu_copy_dir dir)
{
	hipError_t rc = hip_memcpy(dst, src, count, (enum hipMemcpyKind)dir);

	cr_assert_eq(rc, hipSuccess, "memcpy() failed %d", rc);

	return 0;
}

int hip_lib_init(void)
{
	hipError_t ret;
	int count;

	if (libhip_handle)
		return 0;

	libhip_handle = dlopen("libamdhip64.so",
			       RTLD_LAZY | RTLD_GLOBAL | RTLD_NODELETE);
	if (!libhip_handle)
		return -1;

	hip_malloc = dlsym(libhip_handle, "hipMalloc");
	hip_host_alloc = dlsym(libhip_handle, "hipHostAlloc");
	hip_free = dlsym(libhip_handle, "hipFree");
	hip_host_free = dlsym(libhip_handle, "hipHostFree");
	hip_memset = dlsym(libhip_handle, "hipMemset");
	hip_memcpy = dlsym(libhip_handle, "hipMemcpy");
	hip_device_count = dlsym(libhip_handle, "hipGetDeviceCount");
	hip_device_sync = dlsym(libhip_handle, "hipDeviceSynchronize");

	if (!hip_malloc || !hip_free || !hip_memset || !hip_memcpy ||
	    !hip_device_count | !hip_device_sync || !hip_host_alloc ||
	    !hip_host_free) {
		printf("dlerror:%s\n", dlerror());
		dlclose(libhip_handle);
		return -1;
	}

	ret = hip_device_count(&count);
	if (ret != hipSuccess) {
		dlclose(libhip_handle);
		return -1;
	}

	gpu_malloc = h_malloc;
	gpu_host_alloc = h_host_alloc;
	gpu_free = h_free;
	gpu_host_free = h_host_free;
	gpu_memset = h_memset;
	gpu_memcpy = h_memcpy;

	printf("Found AMD GPU\n");

	return 0;
}

void hip_lib_fini(void)
{
	if (!libhip_handle)
		return;

	gpu_free = NULL;
	gpu_malloc = NULL;
	gpu_host_alloc = NULL;
	gpu_memset = NULL;
	gpu_memcpy = NULL;
	dlclose(libhip_handle);
	libhip_handle = NULL;
}
#endif /* HAVE_HIP_SUPPORT */
