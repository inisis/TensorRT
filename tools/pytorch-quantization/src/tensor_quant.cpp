/*
 * SPDX-FileCopyrightText: Copyright (c) 1993-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <cuda.h>
#include <torch/extension.h>

void fake_tensor_quant_cuda_inplace(at::Tensor, at::Tensor, int, bool, bool);
at::Tensor fake_tensor_quant_cuda(at::Tensor, at::Tensor, int, bool, bool);
at::Tensor fake_tensor_quant_with_axis_cuda(at::Tensor, at::Tensor, int, int, bool, bool);
float bits_to_bound(int, int);
at::Tensor fake_e4m3fy_cuda(at::Tensor inputs);

void fake_tensor_quant_(at::Tensor inputs, at::Tensor amax, int num_bits = 8,
                        bool is_unsigned = false, bool narrow_range = true) {
  TORCH_CHECK(inputs.is_cuda());
  TORCH_CHECK(inputs.is_contiguous())  // in-place on non-contiguous tensor is more difficult
  TORCH_CHECK(amax.numel(), 1);
  fake_tensor_quant_cuda_inplace(inputs, amax, num_bits, is_unsigned, narrow_range);
}

at::Tensor fake_tensor_quant(at::Tensor inputs, at::Tensor amax, int num_bits = 8,
                             bool is_unsigned = false, bool narrow_range = true) {
  TORCH_CHECK(inputs.is_cuda());
  TORCH_CHECK(amax.numel(), 1);
  return fake_tensor_quant_cuda(inputs.contiguous(), amax.contiguous(), num_bits, is_unsigned, narrow_range);
}

at::Tensor fake_tensor_quant_with_axis(at::Tensor inputs, at::Tensor amax, int axis,
                                       int num_bits = 8, bool is_unsigned = false,
                                       bool narrow_range = true) {
  TORCH_CHECK(inputs.is_cuda());
  TORCH_CHECK(amax.numel(), inputs.size(axis));
  return fake_tensor_quant_with_axis_cuda(
      inputs.contiguous(), amax.contiguous(), axis, num_bits, is_unsigned, narrow_range);
}


#if CUDA_VERSION > 11070

  #include <cuda_fp8.h>
  #include <ATen/ATen.h>

  at::Tensor fake_e4m3fy_cuda(at::Tensor inputs);

  at::Tensor fake_e4m3fy(at::Tensor inputs) {
    if (inputs.is_cuda()) {
      return fake_e4m3fy_cuda(inputs.contiguous());
    } else {
      TORCH_CHECK(inputs.dtype() == at::ScalarType::Float);
      TORCH_CHECK(inputs.is_contiguous());
      auto out = at::zeros_like(inputs);
      for (int i = 0; i < inputs.numel(); ++i) {
        out.data_ptr<float>()[i] = static_cast<float>(static_cast<__nv_fp8_e4m3>(inputs.data_ptr<float>()[i]));
      }
      return out;
    }
  }

#else

  #include <stdexcept>

  at::Tensor fake_e4m3fy(at::Tensor inputs) {
    throw std::runtime_error("FP8 emulation is not supported on CUDA 11.7 and below");
  }

#endif

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m) {
  m.def("fake_tensor_quant_", &fake_tensor_quant_, "Fake Tensor Quant Inplace", py::arg("inputs"),
        py::arg("amax"), py::arg("num_bits") = 8, py::arg("unsigned") = false,
        py::arg("narrow_range") = true);
  m.def("fake_tensor_quant", &fake_tensor_quant, "Fake Tensor Quant", py::arg("inputs"),
        py::arg("amax"), py::arg("num_bits") = 8, py::arg("unsigned") = false,
        py::arg("narrow_range") = true);
  m.def("fake_tensor_quant_with_axis", &fake_tensor_quant_with_axis, "Fake Tensor Quant with axis",
        py::arg("inputs"), py::arg("amax"), py::arg("axis"), py::arg("num_bits") = 8,
        py::arg("unsigned") = false, py::arg("narrow_range") = true);

  m.def("fake_e4m3fy", &fake_e4m3fy, "Reduce precision to E4M3",
        py::arg("inputs"));
}
