#include "ccv.h"
#include "ccv_internal.h"
#include "nnc/ccv_nnc.h"
#include "nnc/ccv_nnc_easy.h"
#include "nnc/ccv_nnc_internal.h"
#ifdef USE_OPENMP
#include <omp.h>
#endif
#ifdef USE_DISPATCH
#include <dispatch/dispatch.h>
#endif

// Shared methods.
#include "../_ccv_nnc_cpu_ref.h"

void _ccv_nnc_mul_forw_cpu_ref(const float p, ccv_nnc_tensor_view_t* const a, ccv_nnc_tensor_view_t* const b, ccv_nnc_tensor_view_t* const c)
{
	if (b == 0)
	{
		if (p == 1)
		{
			_ccv_nnc_tensor_transfer_cpu_ref_f32(a, c);
			return;
		} else if (p == 0) {
			ccv_nnc_tensor_zero(c);
			return;
		}
		// Assuming this is float 32.
		int dim[CCV_NNC_MAX_DIM_ALLOC];
		int astride[CCV_NNC_MAX_DIM_ALLOC];
		int cstride[CCV_NNC_MAX_DIM_ALLOC];
		assert(ccv_nnc_tensor_nd(a->info.dim) <= CCV_NNC_MAX_DIM + 2);
		assert(ccv_nnc_tensor_nd(c->info.dim) <= CCV_NNC_MAX_DIM + 2);
		ccv_nnc_tensor_view_get_dim(a, dim);
		assert(ccv_nnc_tensor_view_check_dim(c, dim));
		int x;
		if (!CCV_IS_TENSOR_VIEW(a) && !CCV_IS_TENSOR_VIEW(c))
		{
			// Super optimal case, just do one for-loop for sum.
			const int tensor_count = ccv_nnc_tensor_count(a->info);
			for (x = 0; x < tensor_count; x++)
				c->data.f32[x] = p * a->data.f32[x];
			return;
		}
		assert(CCV_NNC_MAX_DIM == 2); // Need to change this logic for CCV_NNC_MAX_DIM == other number.
		ccv_nnc_tensor_view_get_stride(a, astride);
		ccv_nnc_tensor_view_get_stride(c, cstride);
		int i[CCV_NNC_MAX_DIM + 2];
		float* const ap = a->data.f32;
		float* const cp = c->data.f32;
		const int count = dim[2] * dim[3];
		if (astride[2] == dim[3] && cstride[2] == dim[3])
		{
			// Special casing if the ainc[3] is the same as dim[3]
			for (i[0] = 0; i[0] < dim[0]; i[0]++)
			{
				float* ap0 = ap + i[0] * astride[0];
				float* cp0 = cp + i[0] * cstride[0];
				for (i[1] = 0; i[1] < dim[1]; i[1]++)
				{
					for (x = 0; x < count; x++)
						cp0[x] = p * ap0[x];
					ap0 += astride[1];
					cp0 += cstride[1];
				}
			}
			return;
		}
		// Non-optimal case, need to do skip copy.
		for (i[0] = 0; i[0] < dim[0]; i[0]++)
		{
			float* const ap0 = ap + i[0] * astride[0];
			float* const cp0 = cp + i[0] * cstride[0];
			for (i[1] = 0; i[1] < dim[1]; i[1]++)
			{
				float* ap1 = ap0 + i[1] * astride[1];
				float* cp1 = cp0 + i[1] * cstride[1];
				for (i[2] = 0; i[2] < dim[2]; i[2]++)
				{
					for (x = 0; x < dim[3]; x++)
						cp1[x] = p * ap1[x];
					ap1 += astride[2];
					cp1 += cstride[2];
				}
			}
		}
		return;
	}
	int cdim[CCV_NNC_MAX_DIM_ALLOC];
	assert(ccv_nnc_tensor_nd(a->info.dim) <= CCV_NNC_MAX_DIM + 2);
	assert(ccv_nnc_tensor_nd(b->info.dim) <= CCV_NNC_MAX_DIM + 2);
	ccv_nnc_tensor_view_get_dim(a, cdim); // Fill in cdim first.
	ccv_nnc_tensor_view_get_broadcast_dim(b, cdim);
	assert(ccv_nnc_tensor_view_check_broadcast_dim(a, cdim));
	assert(ccv_nnc_tensor_view_check_broadcast_dim(b, cdim));
	const int a_check_dim = ccv_nnc_tensor_view_check_dim(a, cdim);
	const int b_check_dim = ccv_nnc_tensor_view_check_dim(b, cdim);
	if (p == 1 && a_check_dim && b_check_dim)
	{
		_ccv_nnc_ewprod_forw_cpu_ref((ccv_nnc_tensor_view_t*[]){
			a, b
		}, 2, &c, 1);
		return;
	} else if (p == 0) {
		ccv_nnc_tensor_zero(c);
		return;
	}
	// Assuming this is float 32.
	int adim[CCV_NNC_MAX_DIM_ALLOC];
	int bdim[CCV_NNC_MAX_DIM_ALLOC];
	ccv_nnc_tensor_view_get_dim(a, adim);
	ccv_nnc_tensor_view_get_dim(b, bdim);
	int astride[CCV_NNC_MAX_DIM_ALLOC];
	int bstride[CCV_NNC_MAX_DIM_ALLOC];
	int cstride[CCV_NNC_MAX_DIM_ALLOC];
	assert(ccv_nnc_tensor_nd(c->info.dim) <= CCV_NNC_MAX_DIM + 2);
	assert(ccv_nnc_tensor_view_check_dim(c, cdim));
	int x;
	if (!CCV_IS_TENSOR_VIEW(a) && !CCV_IS_TENSOR_VIEW(b) && !CCV_IS_TENSOR_VIEW(c) && a_check_dim && b_check_dim)
	{
		const int tensor_count = ccv_nnc_tensor_count(a->info);
		// Super optimal case, just do one for-loop for sum.
		for (x = 0; x < tensor_count; x++)
			c->data.f32[x] = p * a->data.f32[x] * b->data.f32[x];
		return;
	}
	assert(CCV_NNC_MAX_DIM == 2); // Need to change this logic for CCV_NNC_MAX_DIM == other number.
	ccv_nnc_tensor_view_get_stride(a, astride);
	ccv_nnc_tensor_view_get_stride(b, bstride);
	ccv_nnc_tensor_view_get_stride(c, cstride);
	int i[CCV_NNC_MAX_DIM + 2];
	float* const ap = a->data.f32;
	float* const bp = b->data.f32;
	float* const cp = c->data.f32;
	const int count = cdim[2] * cdim[3];
	if (astride[2] == cdim[3] && bstride[2] == cdim[3] && cstride[2] == cdim[3] && adim[2] == cdim[2] && bdim[2] == cdim[2])
	{
		// Special casing if the ainc[3] is the same as dim[3]
		for (i[0] = 0; i[0] < cdim[0]; i[0]++)
		{
			float* const ap0 = adim[0] == 1 ? ap : ap + i[0] * astride[0];
			float* const bp0 = bdim[0] == 1 ? bp : bp + i[0] * bstride[0];
			float* cp0 = cp + i[0] * cstride[0];
			for (i[1] = 0; i[1] < cdim[1]; i[1]++)
			{
				float* const ap1 = adim[1] == 1 ? ap0 : ap0 + i[1] * astride[1];
				float* const bp1 = bdim[1] == 1 ? bp0 : bp0 + i[1] * bstride[1];
				for (x = 0; x < count; x++)
					cp0[x] = p * ap1[x] * bp1[x];
				cp0 += cstride[1];
			}
		}
		return;
	}
	// Non-optimal case, need to do skip copy and handle broadcasting.
	for (i[0] = 0; i[0] < cdim[0]; i[0]++)
	{
		float* const ap0 = adim[0] == 1 ? ap : ap + i[0] * astride[0];
		float* const bp0 = bdim[0] == 1 ? bp : bp + i[0] * bstride[0];
		float* const cp0 = cp + i[0] * cstride[0];
		for (i[1] = 0; i[1] < cdim[1]; i[1]++)
		{
			float* const ap1 = adim[1] == 1 ? ap0 : ap0 + i[1] * astride[1];
			float* const bp1 = bdim[1] == 1 ? bp0 : bp0 + i[1] * bstride[1];
			float* cp1 = cp0 + i[1] * cstride[1];
			for (i[2] = 0; i[2] < cdim[2]; i[2]++)
			{
				float* const ap2 = adim[2] == 1 ? ap1 : ap1 + i[2] * astride[2];
				float* const bp2 = bdim[2] == 1 ? bp1 : bp1 + i[2] * bstride[2];
				if (adim[3] == 1)
					for (x = 0; x < cdim[3]; x++)
						cp1[x] = p * ap2[0] * bp2[x];
				else if (bdim[3] == 1)
					for (x = 0; x < cdim[3]; x++)
						cp1[x] = p * ap2[x] * bp2[0];
				else
					for (x = 0; x < cdim[3]; x++)
						cp1[x] = p * ap2[x] * bp2[x];
				cp1 += cstride[2];
			}
		}
	}
}

static int _ccv_nnc_mul_forw(const ccv_nnc_cmd_t cmd, const ccv_nnc_hint_t hint, const int flags, ccv_nnc_tensor_t* const* const inputs, const int input_size, ccv_nnc_tensor_t* const* const outputs, const int output_size, ccv_nnc_stream_context_t* const stream_context)
{
	assert(input_size == 2);
	_ccv_nnc_mul_forw_cpu_ref(cmd.info.blas.a[0], (ccv_nnc_tensor_view_t*)inputs[0], (ccv_nnc_tensor_view_t*)inputs[1], (ccv_nnc_tensor_view_t*)outputs[0]);
	return CCV_NNC_EXEC_SUCCESS;
}

static int _ccv_nnc_mul_back(const ccv_nnc_cmd_t cmd, const ccv_nnc_hint_t hint, const int flags, ccv_nnc_tensor_t* const* const inputs, const int input_size, ccv_nnc_tensor_t* const* const outputs, const int output_size, ccv_nnc_stream_context_t* const stream_context)
{
	int gdim[CCV_NNC_MAX_DIM_ALLOC];
	int no_broadcasting = 1;
	if (outputs[0])
	{
		assert(input_size >= 3 && inputs[2]);
		ccv_nnc_tensor_view_get_dim((ccv_nnc_tensor_view_t*)outputs[0], gdim);
		ccv_nnc_tensor_view_get_broadcast_dim((ccv_nnc_tensor_view_t*)inputs[2], gdim);
		no_broadcasting = no_broadcasting && (ccv_nnc_tensor_view_check_dim((ccv_nnc_tensor_view_t*)outputs[0], gdim) && ccv_nnc_tensor_view_check_dim((ccv_nnc_tensor_view_t*)inputs[2], gdim));
	}
	if (no_broadcasting && output_size > 1 && outputs[1])
	{
		assert(inputs[1]);
		ccv_nnc_tensor_view_get_dim((ccv_nnc_tensor_view_t*)inputs[1], gdim);
		ccv_nnc_tensor_view_get_broadcast_dim((ccv_nnc_tensor_view_t*)outputs[1], gdim);
		no_broadcasting = no_broadcasting && (ccv_nnc_tensor_view_check_dim((ccv_nnc_tensor_view_t*)inputs[1], gdim) && ccv_nnc_tensor_view_check_dim((ccv_nnc_tensor_view_t*)outputs[1], gdim));
	}
	if (no_broadcasting)
	{
		if (outputs[0])
		{
			if (inputs[0] == 0)
				_ccv_nnc_mul_forw_cpu_ref(cmd.info.blas.a[0], (ccv_nnc_tensor_view_t*)inputs[2], 0, (ccv_nnc_tensor_view_t*)outputs[0]);
			else
				_ccv_nnc_mul_forw_cpu_ref(cmd.info.blas.a[0], (ccv_nnc_tensor_view_t*)inputs[0], (ccv_nnc_tensor_view_t*)inputs[2], (ccv_nnc_tensor_view_t*)outputs[0]);
		}
		if (output_size > 1 && outputs[1])
		{
			if (inputs[0] == 0)
				_ccv_nnc_mul_forw_cpu_ref(cmd.info.blas.a[0], (ccv_nnc_tensor_view_t*)inputs[1], 0, (ccv_nnc_tensor_view_t*)outputs[1]);
			else
				_ccv_nnc_mul_forw_cpu_ref(cmd.info.blas.a[0], (ccv_nnc_tensor_view_t*)inputs[0], (ccv_nnc_tensor_view_t*)inputs[1], (ccv_nnc_tensor_view_t*)outputs[1]);
		}
		return CCV_NNC_EXEC_SUCCESS;
	}
	int adim[CCV_NNC_MAX_DIM_ALLOC];
	int bdim[CCV_NNC_MAX_DIM_ALLOC];
	int astride[CCV_NNC_MAX_DIM_ALLOC];
	int bstride[CCV_NNC_MAX_DIM_ALLOC];
	int i[CCV_NNC_MAX_DIM + 2];
	int x;
	const float p = cmd.info.blas.a[0];
	// Now the case we need broadcasting.
	if (inputs[0] == 0)
	{
		if (outputs[0])
		{
			ccv_nnc_tensor_view_t* const a = (ccv_nnc_tensor_view_t*)outputs[0];
			ccv_nnc_tensor_view_t* const b = (ccv_nnc_tensor_view_t*)inputs[2];
			ccv_nnc_tensor_view_get_dim(a, adim);
			ccv_nnc_tensor_view_get_dim(b, bdim);
			ccv_nnc_tensor_view_get_stride(a, astride);
			ccv_nnc_tensor_view_get_stride(b, bstride);
			ccv_nnc_tensor_zero(a);
			float* const ap = a->data.f32;
			float* const bp = b->data.f32;
			for (i[0] = 0; i[0] < gdim[0]; i[0]++)
			{
				float* const ap0 = adim[0] == 1 ? ap : ap + i[0] * astride[0];
				float* const bp0 = bdim[0] == 1 ? bp : bp + i[0] * bstride[0];
				for (i[1] = 0; i[1] < gdim[1]; i[1]++)
				{
					float* const ap1 = adim[1] == 1 ? ap0 : ap0 + i[1] * astride[1];
					float* const bp1 = bdim[1] == 1 ? bp0 : bp0 + i[1] * bstride[1];
					for (i[2] = 0; i[2] < gdim[2]; i[2]++)
					{
						float* const ap2 = adim[2] == 1 ? ap1 : ap1 + i[2] * astride[2];
						float* const bp2 = bdim[2] == 1 ? bp1 : bp1 + i[2] * bstride[2];
						if (adim[3] == 1)
							for (x = 0; x < gdim[3]; x++)
								ap2[0] += p * bp2[x];
						else if (bdim[3] == 1)
							for (x = 0; x < gdim[3]; x++)
								ap2[x] += p * bp2[0];
						else
							for (x = 0; x < gdim[3]; x++)
								ap2[x] += p * bp2[x];
					}
				}
			}
		}
		if (output_size > 1 && outputs[1])
		{
			ccv_nnc_tensor_view_t* const a = (ccv_nnc_tensor_view_t*)outputs[1];
			ccv_nnc_tensor_view_t* const b = (ccv_nnc_tensor_view_t*)inputs[1];
			ccv_nnc_tensor_view_get_dim(a, adim);
			ccv_nnc_tensor_view_get_dim(b, bdim);
			ccv_nnc_tensor_view_get_stride(a, astride);
			ccv_nnc_tensor_view_get_stride(b, bstride);
			ccv_nnc_tensor_zero(a);
			float* const ap = a->data.f32;
			float* const bp = b->data.f32;
			for (i[0] = 0; i[0] < gdim[0]; i[0]++)
			{
				float* const ap0 = adim[0] == 1 ? ap : ap + i[0] * astride[0];
				float* const bp0 = bdim[0] == 1 ? bp : bp + i[0] * bstride[0];
				for (i[1] = 0; i[1] < gdim[1]; i[1]++)
				{
					float* const ap1 = adim[1] == 1 ? ap0 : ap0 + i[1] * astride[1];
					float* const bp1 = bdim[1] == 1 ? bp0 : bp0 + i[1] * bstride[1];
					for (i[2] = 0; i[2] < gdim[2]; i[2]++)
					{
						float* const ap2 = adim[2] == 1 ? ap1 : ap1 + i[2] * astride[2];
						float* const bp2 = bdim[2] == 1 ? bp1 : bp1 + i[2] * bstride[2];
						if (adim[3] == 1)
							for (x = 0; x < gdim[3]; x++)
								ap2[0] += p * bp2[x];
						else if (bdim[3] == 1)
							for (x = 0; x < gdim[3]; x++)
								ap2[x] += p * bp2[0];
						else
							for (x = 0; x < gdim[3]; x++)
								ap2[x] += p * bp2[x];
					}
				}
			}
		}
		return CCV_NNC_EXEC_SUCCESS;
	}
	int gstride[CCV_NNC_MAX_DIM_ALLOC];
	ccv_nnc_tensor_view_t* const g = (ccv_nnc_tensor_view_t*)inputs[0];
	ccv_nnc_tensor_view_get_dim(g, gdim);
	ccv_nnc_tensor_view_get_stride(g, gstride);
	if (outputs[0])
	{
		ccv_nnc_tensor_view_t* const a = (ccv_nnc_tensor_view_t*)outputs[0];
		ccv_nnc_tensor_view_t* const b = (ccv_nnc_tensor_view_t*)inputs[2];
		ccv_nnc_tensor_view_get_dim(a, adim);
		ccv_nnc_tensor_view_get_dim(b, bdim);
		ccv_nnc_tensor_view_get_stride(a, astride);
		ccv_nnc_tensor_view_get_stride(b, bstride);
		ccv_nnc_tensor_zero(a);
		float* const ap = a->data.f32;
		float* const bp = b->data.f32;
		float* const gp = g->data.f32;
		for (i[0] = 0; i[0] < gdim[0]; i[0]++)
		{
			float* const ap0 = adim[0] == 1 ? ap : ap + i[0] * astride[0];
			float* const bp0 = bdim[0] == 1 ? bp : bp + i[0] * bstride[0];
			float* const gp0 = gp + i[0] * gstride[0];
			for (i[1] = 0; i[1] < gdim[1]; i[1]++)
			{
				float* const ap1 = adim[1] == 1 ? ap0 : ap0 + i[1] * astride[1];
				float* const bp1 = bdim[1] == 1 ? bp0 : bp0 + i[1] * bstride[1];
				float* gp1 = gp0 + i[1] * gstride[1];
				for (i[2] = 0; i[2] < gdim[2]; i[2]++)
				{
					float* const ap2 = adim[2] == 1 ? ap1 : ap1 + i[2] * astride[2];
					float* const bp2 = bdim[2] == 1 ? bp1 : bp1 + i[2] * bstride[2];
					if (adim[3] == 1)
						for (x = 0; x < gdim[3]; x++)
							ap2[0] += p * gp1[x] * bp2[x];
					else if (bdim[3] == 1)
						for (x = 0; x < gdim[3]; x++)
							ap2[x] += p * gp1[x] * bp2[0];
					else
						for (x = 0; x < gdim[3]; x++)
							ap2[x] += p * gp1[x] * bp2[x];
					gp1 += gstride[2];
				}
			}
		}
	}
	if (output_size > 1 && outputs[1])
	{
		ccv_nnc_tensor_view_t* const a = (ccv_nnc_tensor_view_t*)outputs[1];
		ccv_nnc_tensor_view_t* const b = (ccv_nnc_tensor_view_t*)inputs[1];
		ccv_nnc_tensor_view_get_dim(a, adim);
		ccv_nnc_tensor_view_get_dim(b, bdim);
		ccv_nnc_tensor_view_get_stride(a, astride);
		ccv_nnc_tensor_view_get_stride(b, bstride);
		ccv_nnc_tensor_zero(a);
		float* const ap = a->data.f32;
		float* const bp = b->data.f32;
		float* const gp = g->data.f32;
		for (i[0] = 0; i[0] < gdim[0]; i[0]++)
		{
			float* const ap0 = adim[0] == 1 ? ap : ap + i[0] * astride[0];
			float* const bp0 = bdim[0] == 1 ? bp : bp + i[0] * bstride[0];
			float* const gp0 = gp + i[0] * gstride[0];
			for (i[1] = 0; i[1] < gdim[1]; i[1]++)
			{
				float* const ap1 = adim[1] == 1 ? ap0 : ap0 + i[1] * astride[1];
				float* const bp1 = bdim[1] == 1 ? bp0 : bp0 + i[1] * bstride[1];
				float* gp1 = gp0 + i[1] * gstride[1];
				for (i[2] = 0; i[2] < gdim[2]; i[2]++)
				{
					float* const ap2 = adim[2] == 1 ? ap1 : ap1 + i[2] * astride[2];
					float* const bp2 = bdim[2] == 1 ? bp1 : bp1 + i[2] * bstride[2];
					if (adim[3] == 1)
						for (x = 0; x < gdim[3]; x++)
							ap2[0] += p * gp1[x] * bp2[x];
					else if (bdim[3] == 1)
						for (x = 0; x < gdim[3]; x++)
							ap2[x] += p * gp1[x] * bp2[0];
					else
						for (x = 0; x < gdim[3]; x++)
							ap2[x] += p * gp1[x] * bp2[x];
					gp1 += gstride[2];
				}
			}
		}
	}
	return CCV_NNC_EXEC_SUCCESS;
}

REGISTER_COMMAND_BACKEND(CCV_NNC_MUL_FORWARD, CCV_NNC_BACKEND_CPU_REF)(ccv_nnc_cmd_backend_registry_t* const registry)
{
	registry->tensor_formats = CCV_TENSOR_FORMAT_NHWC | CCV_TENSOR_FORMAT_NCHW | CCV_TENSOR_FORMAT_CHWN;
	registry->tensor_datatypes = CCV_32F;
	registry->tensor_memory = CCV_TENSOR_CPU_MEMORY;
	registry->algorithms = 1;
	registry->exec = _ccv_nnc_mul_forw;
}

REGISTER_COMMAND_BACKEND(CCV_NNC_MUL_BACKWARD, CCV_NNC_BACKEND_CPU_REF)(ccv_nnc_cmd_backend_registry_t* const registry)
{
	registry->tensor_formats = CCV_TENSOR_FORMAT_NHWC | CCV_TENSOR_FORMAT_NCHW | CCV_TENSOR_FORMAT_CHWN;
	registry->tensor_datatypes = CCV_32F;
	registry->tensor_memory = CCV_TENSOR_CPU_MEMORY;
	registry->algorithms = 1;
	registry->exec = _ccv_nnc_mul_back;
}

static int _ccv_nnc_scalar_mul_forw(const ccv_nnc_cmd_t cmd, const ccv_nnc_hint_t hint, const int flags, ccv_nnc_tensor_t* const* const inputs, const int input_size, ccv_nnc_tensor_t* const* const outputs, const int output_size, ccv_nnc_stream_context_t* const stream_context)
{
	_ccv_nnc_mul_forw_cpu_ref(cmd.info.blas.a[0], (ccv_nnc_tensor_view_t*)inputs[0], 0, (ccv_nnc_tensor_view_t*)outputs[0]);
	return CCV_NNC_EXEC_SUCCESS;
}
static int _ccv_nnc_scalar_mul_back(const ccv_nnc_cmd_t cmd, const ccv_nnc_hint_t hint, const int flags, ccv_nnc_tensor_t* const* const inputs, const int input_size, ccv_nnc_tensor_t* const* const outputs, const int output_size, ccv_nnc_stream_context_t* const stream_context)
{
	if (inputs[0])
		_ccv_nnc_mul_forw_cpu_ref(cmd.info.blas.a[0], (ccv_nnc_tensor_view_t*)inputs[0], 0, (ccv_nnc_tensor_view_t*)outputs[0]);
	else
		_ccv_nnc_tensor_set_cpu_ref_f32((ccv_nnc_tensor_view_t*)outputs[0], cmd.info.blas.a[0]);
	return CCV_NNC_EXEC_SUCCESS;
}

REGISTER_COMMAND_BACKEND(CCV_NNC_SCALAR_MUL_FORWARD, CCV_NNC_BACKEND_CPU_REF)(ccv_nnc_cmd_backend_registry_t* const registry)
{
	registry->tensor_formats = CCV_TENSOR_FORMAT_NHWC | CCV_TENSOR_FORMAT_NCHW | CCV_TENSOR_FORMAT_CHWN;
	registry->tensor_datatypes = CCV_32F;
	registry->tensor_memory = CCV_TENSOR_CPU_MEMORY;
	registry->algorithms = 1;
	registry->exec = _ccv_nnc_scalar_mul_forw;
}

REGISTER_COMMAND_BACKEND(CCV_NNC_SCALAR_MUL_BACKWARD, CCV_NNC_BACKEND_CPU_REF)(ccv_nnc_cmd_backend_registry_t* const registry)
{
	registry->tensor_formats = CCV_TENSOR_FORMAT_NHWC | CCV_TENSOR_FORMAT_NCHW | CCV_TENSOR_FORMAT_CHWN;
	registry->tensor_datatypes = CCV_32F;
	registry->tensor_memory = CCV_TENSOR_CPU_MEMORY;
	registry->algorithms = 1;
	registry->exec = _ccv_nnc_scalar_mul_back;
}
