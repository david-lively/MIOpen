#ifndef GUARD_MLOPEN_OCL_KERNEL_HPP_
#define GUARD_MLOPEN_OCL_KERNEL_HPP_

#include <sstream>
#include <array>
#include <utility>
#include <vector>
#include <mlopen.h>
#include <cassert>
#include <functional>
#include <array>
#include <memory>

#include <mlopen/errors.hpp>
#include <mlopen/each_args.hpp>
#include <mlopen/clhelper.hpp>

enum class kernarg_list_types {
	generic,
	winograd,
};

namespace mlopen {

using SharedKernelPtr = std::shared_ptr<typename std::remove_pointer<cl_kernel>::type>;
using SharedProgramPtr = std::shared_ptr<typename std::remove_pointer<cl_program>::type>;

struct LocalMemArg 
{
	LocalMemArg(size_t _size) : size(_size) {}
	size_t GetSize() const { return size; }
	
	private:
	size_t size;
};

struct OCLSetKernelArg
{
	template<class I, class T>
	void operator()(cl_kernel kernel, I i, const T& x) const
	{
		cl_int status = clSetKernelArg(kernel, i, sizeof(T), reinterpret_cast<const void*>(&x));
		if (status != CL_SUCCESS) {
			MLOPEN_THROW(
			"Error setting argument #" + std::to_string(i) + " to kernel (size = " + std::to_string(sizeof(T)) + "): "
			+ std::to_string(status));
		}
	}

	template<class I, class T>
	void operator()(cl_kernel kernel, I i, const LocalMemArg& lmem) const
	{
		cl_int status = clSetKernelArg(kernel, i, lmem.GetSize(), NULL);
		if (status != CL_SUCCESS) {
			MLOPEN_THROW(
				"Error setting argument #" + std::to_string(i) + " to kernel: "
				+ std::to_string(status));
		}
	}
};

struct OCLKernelInvoke
{
	cl_command_queue queue;
	SharedKernelPtr kernel;
	size_t work_dim;
	std::array<size_t, 3> global_work_offset;
	std::array<size_t, 3> global_work_dim;
	std::array<size_t, 3> local_work_dim;
	std::function<void(cl_event&)> callback;
	kernarg_list_types kernarg_list_type;

	template<class... Ts>
	void operator()(const Ts&... xs) const
	{
		each_args_i(std::bind(OCLSetKernelArg{}, kernel.get(), std::placeholders::_1, std::placeholders::_2), xs...);
		run();
	}

	void run() const;
};

class OCLKernel {

public:
	OCLKernel() {}
	OCLKernel(ClKernelPtr k) : kernel(std::move(k)) {}
	OCLKernel(ClKernelPtr k, 
			std::vector<size_t> local_dims,
			std::vector<size_t> global_dims, SharedProgramPtr p=nullptr) 
	: program(p), kernel(std::move(k)), ldims(std::move(local_dims)), gdims(std::move(global_dims))
	{
		assert(!gdims.empty() && gdims.size() <= 3);

		/* We sometimes launch more than 256 threads in a workgroup.
		 * The way to do that is to set local_size == NULL in the clEnqueueNDRangeKernel 
		 * interface and to pass the acual work-group size through the kernel attribute 
		 * __attribute__((reqd_work_group_size(MLO_GRP_SZ0, MLO_GRP_SZ1, MLO_GRP_SZ2)))
		 *
		 * To make the above possible we send an indication to the Invoke (run)
		 * routine with ldims.size() == 0.
		 *
		 * This is why assert on ldims is not done.
		 */
	}

	OCLKernelInvoke Invoke(cl_command_queue q, std::function<void(cl_event&)> callback=nullptr);

	cl_kernel GetKernel() { return kernel.get(); } 

	std::string GetName() const;

	inline const std::vector<size_t>& GetLocalDims() const { return ldims; }
	inline const std::vector<size_t>& GetGlobalDims() const { return gdims; }
	inline void SetKernArgListType(kernarg_list_types type) { kernarg_list_type = type; }

private:
	SharedProgramPtr program;
	SharedKernelPtr kernel;
	std::vector<size_t> ldims;
	std::vector<size_t> gdims;
	kernarg_list_types kernarg_list_type = kernarg_list_types::generic;
};

}  // namespace mlopen

#endif // GUARD_MLOPEN_OCL_KERNEL_HPP_
