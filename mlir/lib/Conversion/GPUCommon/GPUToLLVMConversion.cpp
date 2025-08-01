//===- ConvertLaunchFuncToGpuRuntimeCalls.cpp - MLIR GPU lowering passes --===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a pass to convert gpu.launch_func op into a sequence of
// GPU runtime calls. As most of GPU runtimes does not have a stable published
// ABI, this pass uses a slim runtime layer that builds on top of the public
// API from GPU runtime headers.
//
//===----------------------------------------------------------------------===//

#include "mlir/Conversion/GPUCommon/GPUCommonPass.h"

#include "mlir/Conversion/ArithToLLVM/ArithToLLVM.h"
#include "mlir/Conversion/AsyncToLLVM/AsyncToLLVM.h"
#include "mlir/Conversion/ControlFlowToLLVM/ControlFlowToLLVM.h"
#include "mlir/Conversion/ConvertToLLVM/ToLLVMInterface.h"
#include "mlir/Conversion/ConvertToLLVM/ToLLVMPass.h"
#include "mlir/Conversion/FuncToLLVM/ConvertFuncToLLVM.h"
#include "mlir/Conversion/GPUCommon/GPUToLLVM.h"
#include "mlir/Conversion/LLVMCommon/Pattern.h"
#include "mlir/Conversion/MemRefToLLVM/MemRefToLLVM.h"
#include "mlir/Conversion/VectorToLLVM/ConvertVectorToLLVM.h"
#include "mlir/Dialect/Async/IR/Async.h"
#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/GPU/Transforms/Passes.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/Vector/Transforms/LoweringPatterns.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"

#include "llvm/ADT/STLExtras.h"

#define DEBUG_TYPE "gpu-to-llvm"

namespace mlir {
#define GEN_PASS_DEF_GPUTOLLVMCONVERSIONPASS
#include "mlir/Conversion/Passes.h.inc"
} // namespace mlir

using namespace mlir;

namespace {
class GpuToLLVMConversionPass
    : public impl::GpuToLLVMConversionPassBase<GpuToLLVMConversionPass> {
public:
  using Base::Base;
  void getDependentDialects(DialectRegistry &registry) const final {
    Base::getDependentDialects(registry);
    registerConvertToLLVMDependentDialectLoading(registry);
  }
  // Run the dialect converter on the module.
  void runOnOperation() override;
};

template <typename OpTy>
class ConvertOpToGpuRuntimeCallPattern : public ConvertOpToLLVMPattern<OpTy> {
public:
  explicit ConvertOpToGpuRuntimeCallPattern(
      const LLVMTypeConverter &typeConverter)
      : ConvertOpToLLVMPattern<OpTy>(typeConverter) {}

protected:
  Value getNumElements(ConversionPatternRewriter &rewriter, Location loc,
                       MemRefType type, MemRefDescriptor desc) const {
    Type indexType = ConvertToLLVMPattern::getIndexType();
    if (type.hasStaticShape())
      return ConvertToLLVMPattern::createIndexAttrConstant(
          rewriter, loc, indexType, type.getNumElements());
    // Compute the number of elements by multiplying all the dim sizes.
    uint64_t rank = type.getRank();
    Value numElements = desc.size(rewriter, loc, /*pos=*/0);
    for (unsigned i = 1; i < rank; i++)
      numElements = LLVM::MulOp::create(rewriter, loc, numElements,
                                        desc.size(rewriter, loc, /*pos=*/i));
    return numElements;
  }

  MLIRContext *context = &this->getTypeConverter()->getContext();

  Type llvmVoidType = LLVM::LLVMVoidType::get(context);
  LLVM::LLVMPointerType llvmPointerType = LLVM::LLVMPointerType::get(context);
  Type llvmInt8Type = IntegerType::get(context, 8);
  Type llvmInt16Type = IntegerType::get(context, 16);
  Type llvmInt32Type = IntegerType::get(context, 32);
  Type llvmInt64Type = IntegerType::get(context, 64);
  Type llvmFloat32Type = Float32Type::get(context);
  Type llvmIntPtrType = IntegerType::get(
      context, this->getTypeConverter()->getPointerBitwidth(0));

  FunctionCallBuilder streamCreateCallBuilder = {
      "mgpuStreamCreate", llvmPointerType /* void *stream */, {}};
  FunctionCallBuilder streamDestroyCallBuilder = {
      "mgpuStreamDestroy", llvmVoidType, {llvmPointerType /* void *stream */}};
  FunctionCallBuilder streamSynchronizeCallBuilder = {
      "mgpuStreamSynchronize",
      llvmVoidType,
      {llvmPointerType /* void *stream */}};
  FunctionCallBuilder streamWaitEventCallBuilder = {
      "mgpuStreamWaitEvent",
      llvmVoidType,
      {llvmPointerType /* void *stream */, llvmPointerType /* void *event */}};
  FunctionCallBuilder eventCreateCallBuilder = {
      "mgpuEventCreate", llvmPointerType /* void *event */, {}};
  FunctionCallBuilder eventDestroyCallBuilder = {
      "mgpuEventDestroy", llvmVoidType, {llvmPointerType /* void *event */}};
  FunctionCallBuilder eventSynchronizeCallBuilder = {
      "mgpuEventSynchronize",
      llvmVoidType,
      {llvmPointerType /* void *event */}};
  FunctionCallBuilder eventRecordCallBuilder = {
      "mgpuEventRecord",
      llvmVoidType,
      {llvmPointerType /* void *event */, llvmPointerType /* void *stream */}};
  FunctionCallBuilder hostRegisterCallBuilder = {
      "mgpuMemHostRegisterMemRef",
      llvmVoidType,
      {llvmIntPtrType /* intptr_t rank */,
       llvmPointerType /* void *memrefDesc */,
       llvmIntPtrType /* intptr_t elementSizeBytes */}};
  FunctionCallBuilder hostUnregisterCallBuilder = {
      "mgpuMemHostUnregisterMemRef",
      llvmVoidType,
      {llvmIntPtrType /* intptr_t rank */,
       llvmPointerType /* void *memrefDesc */,
       llvmIntPtrType /* intptr_t elementSizeBytes */}};
  FunctionCallBuilder allocCallBuilder = {
      "mgpuMemAlloc",
      llvmPointerType /* void * */,
      {llvmIntPtrType /* intptr_t sizeBytes */,
       llvmPointerType /* void *stream */,
       llvmInt8Type /* bool isHostShared */}};
  FunctionCallBuilder deallocCallBuilder = {
      "mgpuMemFree",
      llvmVoidType,
      {llvmPointerType /* void *ptr */, llvmPointerType /* void *stream */}};
  FunctionCallBuilder memcpyCallBuilder = {
      "mgpuMemcpy",
      llvmVoidType,
      {llvmPointerType /* void *dst */, llvmPointerType /* void *src */,
       llvmIntPtrType /* intptr_t sizeBytes */,
       llvmPointerType /* void *stream */}};
  FunctionCallBuilder memset16CallBuilder = {
      "mgpuMemset16",
      llvmVoidType,
      {llvmPointerType /* void *dst */,
       llvmInt16Type /* unsigned short value */,
       llvmIntPtrType /* intptr_t sizeBytes */,
       llvmPointerType /* void *stream */}};
  FunctionCallBuilder memset32CallBuilder = {
      "mgpuMemset32",
      llvmVoidType,
      {llvmPointerType /* void *dst */, llvmInt32Type /* unsigned int value */,
       llvmIntPtrType /* intptr_t sizeBytes */,
       llvmPointerType /* void *stream */}};
  FunctionCallBuilder setDefaultDeviceCallBuilder = {
      "mgpuSetDefaultDevice",
      llvmVoidType,
      {llvmInt32Type /* uint32_t devIndex */}};
  FunctionCallBuilder createDnVecCallBuilder = {
      "mgpuCreateDnVec",
      llvmPointerType,
      {llvmIntPtrType, llvmPointerType, llvmInt32Type,
       llvmPointerType /* void *stream */}};
  FunctionCallBuilder destroyDnVecCallBuilder = {
      "mgpuDestroyDnVec",
      llvmVoidType,
      {llvmPointerType, llvmPointerType /* void *stream */}};
  FunctionCallBuilder createDnMatCallBuilder = {
      "mgpuCreateDnMat",
      llvmPointerType,
      {llvmIntPtrType, llvmIntPtrType, llvmPointerType, llvmInt32Type,
       llvmPointerType /* void *stream */}};
  FunctionCallBuilder destroyDnMatCallBuilder = {
      "mgpuDestroyDnMat",
      llvmVoidType,
      {llvmPointerType, llvmPointerType /* void *stream */}};
  FunctionCallBuilder createCooCallBuilder = {
      "mgpuCreateCoo",
      llvmPointerType,
      {llvmIntPtrType, llvmIntPtrType, llvmIntPtrType, llvmPointerType,
       llvmPointerType, llvmPointerType, llvmInt32Type, llvmInt32Type,
       llvmPointerType /* void *stream */}};
  FunctionCallBuilder createCooAoSCallBuilder = {
      "mgpuCreateCooAoS", // deprecated in cuSPARSE 11.2
      llvmPointerType,
      {llvmIntPtrType, llvmIntPtrType, llvmIntPtrType, llvmPointerType,
       llvmPointerType, llvmInt32Type, llvmInt32Type,
       llvmPointerType /* void *stream */}};
  FunctionCallBuilder createCsrCallBuilder = {
      "mgpuCreateCsr",
      llvmPointerType,
      {llvmIntPtrType, llvmIntPtrType, llvmIntPtrType, llvmPointerType,
       llvmPointerType, llvmPointerType, llvmInt32Type, llvmInt32Type,
       llvmInt32Type, llvmPointerType /* void *stream */}};
  FunctionCallBuilder createCscCallBuilder = {
      "mgpuCreateCsc",
      llvmPointerType,
      {llvmIntPtrType, llvmIntPtrType, llvmIntPtrType, llvmPointerType,
       llvmPointerType, llvmPointerType, llvmInt32Type, llvmInt32Type,
       llvmInt32Type, llvmPointerType /* void *stream */}};
  FunctionCallBuilder createBsrCallBuilder = {
      "mgpuCreateBsr",
      llvmPointerType,
      {llvmIntPtrType, llvmIntPtrType, llvmIntPtrType, llvmIntPtrType,
       llvmIntPtrType, llvmPointerType, llvmPointerType, llvmPointerType,
       llvmInt32Type, llvmInt32Type, llvmInt32Type,
       llvmPointerType /* void *stream */}};
  FunctionCallBuilder destroySpMatCallBuilder = {
      "mgpuDestroySpMat",
      llvmVoidType,
      {llvmPointerType, llvmPointerType /* void *stream */}};
  FunctionCallBuilder spMVBufferSizeCallBuilder = {
      "mgpuSpMVBufferSize",
      llvmIntPtrType,
      {llvmInt32Type, llvmPointerType, llvmPointerType, llvmPointerType,
       llvmInt32Type, llvmPointerType /* void *stream */}};
  FunctionCallBuilder spMVCallBuilder = {
      "mgpuSpMV",
      llvmVoidType,
      {llvmInt32Type, llvmPointerType, llvmPointerType, llvmPointerType,
       llvmInt32Type, llvmPointerType, llvmPointerType /* void *stream */}};
  FunctionCallBuilder createSpMMBufferSizeCallBuilder = {
      "mgpuSpMMBufferSize",
      llvmIntPtrType,
      {llvmInt32Type, llvmInt32Type, llvmPointerType, llvmPointerType,
       llvmPointerType, llvmInt32Type, llvmPointerType /* void *stream */}};
  FunctionCallBuilder createSpMMCallBuilder = {
      "mgpuSpMM",
      llvmVoidType,
      {llvmInt32Type, llvmInt32Type, llvmPointerType, llvmPointerType,
       llvmPointerType, llvmInt32Type, llvmPointerType,
       llvmPointerType /* void *stream */}};
  FunctionCallBuilder createSDDMMBufferSizeCallBuilder = {
      "mgpuSDDMMBufferSize",
      llvmIntPtrType,
      {llvmInt32Type, llvmInt32Type, llvmPointerType, llvmPointerType,
       llvmPointerType, llvmInt32Type, llvmPointerType /* void *stream */}};
  FunctionCallBuilder createSDDMMCallBuilder = {
      "mgpuSDDMM",
      llvmVoidType,
      {llvmInt32Type, llvmInt32Type, llvmPointerType, llvmPointerType,
       llvmPointerType, llvmInt32Type, llvmPointerType,
       llvmPointerType /* void *stream */}};
  FunctionCallBuilder createLtDnMatCallBuilder = {
      "mgpuCreateCuSparseLtDnMat",
      llvmVoidType,
      {llvmPointerType, llvmIntPtrType, llvmIntPtrType, llvmPointerType,
       llvmInt32Type, llvmPointerType /* void *stream */}};
  FunctionCallBuilder destroyCuSparseLtSpMatBuilder = {
      "mgpuDestroyCuSparseLtSpMat",
      llvmVoidType,
      {llvmPointerType, llvmPointerType /* void *stream */}};
  FunctionCallBuilder destroyCuSparseLtDnMatBuilder = {
      "mgpuDestroyCuSparseLtDnMat",
      llvmVoidType,
      {llvmPointerType, llvmPointerType /* void *stream */}};
  FunctionCallBuilder create2To4SpMatCallBuilder = {
      "mgpuCusparseLtCreate2To4SpMat",
      llvmVoidType,
      {llvmPointerType, llvmIntPtrType, llvmIntPtrType, llvmPointerType,
       llvmInt32Type, llvmPointerType /* void *stream */}};
  FunctionCallBuilder createCuSparseLtSpMMBufferSizeBuilder = {
      "mgpuCuSparseLtSpMMBufferSize",
      llvmVoidType,
      {llvmPointerType, llvmInt32Type, llvmInt32Type, llvmPointerType,
       llvmPointerType, llvmPointerType, llvmInt32Type, llvmInt32Type,
       llvmPointerType /*void *stream*/}};
  FunctionCallBuilder createCuSparseLtSpMMBuilder = {
      "mgpuCuSparseLtSpMM",
      llvmVoidType,
      {llvmPointerType, llvmPointerType, llvmPointerType, llvmPointerType,
       llvmPointerType, llvmPointerType, llvmPointerType /*void *stream*/}};
  FunctionCallBuilder createSpGEMMCreateDescrBuilder = {
      "mgpuSpGEMMCreateDescr",
      llvmPointerType,
      {llvmPointerType /*void *stream*/}};
  FunctionCallBuilder createSpGEMMDestroyDescrBuilder = {
      "mgpuSpGEMMDestroyDescr",
      llvmVoidType,
      {llvmPointerType /*s*/, llvmPointerType /*void *stream*/}};
  FunctionCallBuilder createSpGEMMWorkEstimationBuilder = {
      "mgpuSpGEMMWorkEstimation",
      llvmIntPtrType,
      {llvmPointerType /*s*/, llvmInt32Type /*ma*/, llvmInt32Type /*mb*/,
       llvmPointerType /*a*/, llvmPointerType /*b*/, llvmPointerType /*c*/,
       llvmInt32Type /*ctp*/, llvmIntPtrType /*bs*/, llvmPointerType /*buf*/,
       llvmPointerType /*void *stream*/}};
  FunctionCallBuilder createSpGEMMComputeBuilder = {
      "mgpuSpGEMMCompute",
      llvmIntPtrType,
      {llvmPointerType /*s*/, llvmInt32Type /*ma*/, llvmInt32Type /*mb*/,
       llvmPointerType /*a*/, llvmPointerType /*b*/, llvmPointerType /*c*/,
       llvmInt32Type /*ctp*/, llvmIntPtrType /*bs*/, llvmPointerType /*buf*/,
       llvmPointerType /*void *stream*/}};
  FunctionCallBuilder createSpGEMMCopyBuilder = {
      "mgpuSpGEMMCopy",
      llvmVoidType,
      {llvmPointerType /*s*/, llvmInt32Type /*ma*/, llvmInt32Type /*mb*/,
       llvmPointerType /*a*/, llvmPointerType /*b*/, llvmPointerType /*c*/,
       llvmInt32Type /*ctp*/, llvmPointerType /*void *stream*/}};
  FunctionCallBuilder createSpMatGetSizeBuilder = {
      "mgpuSpMatGetSize",
      llvmVoidType,
      {llvmPointerType /*mc*/, llvmPointerType /*rc*/, llvmPointerType /*cc*/,
       llvmPointerType /*nc*/, llvmPointerType /*void *stream*/}};
  FunctionCallBuilder createSetCsrPointersBuilder = {
      "mgpuSetCsrPointers",
      llvmVoidType,
      {llvmPointerType /*spmat*/, llvmPointerType /*pos*/,
       llvmPointerType /*crd*/, llvmPointerType /*val*/,
       llvmPointerType /*void *stream*/}};
};

/// A rewrite pattern to convert gpu.host_register operations into a GPU runtime
/// call. Currently it supports CUDA and ROCm (HIP).
class ConvertHostRegisterOpToGpuRuntimeCallPattern
    : public ConvertOpToGpuRuntimeCallPattern<gpu::HostRegisterOp> {
public:
  ConvertHostRegisterOpToGpuRuntimeCallPattern(
      const LLVMTypeConverter &typeConverter)
      : ConvertOpToGpuRuntimeCallPattern<gpu::HostRegisterOp>(typeConverter) {}

private:
  LogicalResult
  matchAndRewrite(gpu::HostRegisterOp hostRegisterOp, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override;
};

class ConvertHostUnregisterOpToGpuRuntimeCallPattern
    : public ConvertOpToGpuRuntimeCallPattern<gpu::HostUnregisterOp> {
public:
  ConvertHostUnregisterOpToGpuRuntimeCallPattern(
      const LLVMTypeConverter &typeConverter)
      : ConvertOpToGpuRuntimeCallPattern<gpu::HostUnregisterOp>(typeConverter) {
  }

private:
  LogicalResult
  matchAndRewrite(gpu::HostUnregisterOp hostUnregisterOp, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override;
};

/// A rewrite pattern to convert gpu.alloc operations into a GPU runtime
/// call. Currently it supports CUDA and ROCm (HIP).
class ConvertAllocOpToGpuRuntimeCallPattern
    : public ConvertOpToGpuRuntimeCallPattern<gpu::AllocOp> {
public:
  ConvertAllocOpToGpuRuntimeCallPattern(const LLVMTypeConverter &typeConverter)
      : ConvertOpToGpuRuntimeCallPattern<gpu::AllocOp>(typeConverter) {}

private:
  LogicalResult
  matchAndRewrite(gpu::AllocOp allocOp, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override;
};

/// A rewrite pattern to convert gpu.dealloc operations into a GPU runtime
/// call. Currently it supports CUDA and ROCm (HIP).
class ConvertDeallocOpToGpuRuntimeCallPattern
    : public ConvertOpToGpuRuntimeCallPattern<gpu::DeallocOp> {
public:
  ConvertDeallocOpToGpuRuntimeCallPattern(
      const LLVMTypeConverter &typeConverter)
      : ConvertOpToGpuRuntimeCallPattern<gpu::DeallocOp>(typeConverter) {}

private:
  LogicalResult
  matchAndRewrite(gpu::DeallocOp deallocOp, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override;
};

class ConvertAsyncYieldToGpuRuntimeCallPattern
    : public ConvertOpToGpuRuntimeCallPattern<async::YieldOp> {
public:
  ConvertAsyncYieldToGpuRuntimeCallPattern(
      const LLVMTypeConverter &typeConverter)
      : ConvertOpToGpuRuntimeCallPattern<async::YieldOp>(typeConverter) {}

private:
  LogicalResult
  matchAndRewrite(async::YieldOp yieldOp, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override;
};

/// A rewrite pattern to convert gpu.wait operations into a GPU runtime
/// call. Currently it supports CUDA and ROCm (HIP).
class ConvertWaitOpToGpuRuntimeCallPattern
    : public ConvertOpToGpuRuntimeCallPattern<gpu::WaitOp> {
public:
  ConvertWaitOpToGpuRuntimeCallPattern(const LLVMTypeConverter &typeConverter)
      : ConvertOpToGpuRuntimeCallPattern<gpu::WaitOp>(typeConverter) {}

private:
  LogicalResult
  matchAndRewrite(gpu::WaitOp waitOp, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override;
};

/// A rewrite pattern to convert gpu.wait async operations into a GPU runtime
/// call. Currently it supports CUDA and ROCm (HIP).
class ConvertWaitAsyncOpToGpuRuntimeCallPattern
    : public ConvertOpToGpuRuntimeCallPattern<gpu::WaitOp> {
public:
  ConvertWaitAsyncOpToGpuRuntimeCallPattern(
      const LLVMTypeConverter &typeConverter)
      : ConvertOpToGpuRuntimeCallPattern<gpu::WaitOp>(typeConverter) {}

private:
  LogicalResult
  matchAndRewrite(gpu::WaitOp waitOp, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override;
};

/// A rewrite patter to legalize gpu.launch_func with LLVM types.
class LegalizeLaunchFuncOpPattern
    : public ConvertOpToGpuRuntimeCallPattern<gpu::LaunchFuncOp> {
public:
  LegalizeLaunchFuncOpPattern(const LLVMTypeConverter &typeConverter,
                              bool kernelBarePtrCallConv,
                              bool kernelIntersperseSizeCallConv)
      : ConvertOpToGpuRuntimeCallPattern<gpu::LaunchFuncOp>(typeConverter),
        kernelBarePtrCallConv(kernelBarePtrCallConv),
        kernelIntersperseSizeCallConv(kernelIntersperseSizeCallConv) {}

private:
  LogicalResult
  matchAndRewrite(gpu::LaunchFuncOp launchOp, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override;

  bool kernelBarePtrCallConv;
  bool kernelIntersperseSizeCallConv;
};

/// A rewrite pattern to convert gpu.memcpy operations into a GPU runtime
/// call. Currently it supports CUDA and ROCm (HIP).
class ConvertMemcpyOpToGpuRuntimeCallPattern
    : public ConvertOpToGpuRuntimeCallPattern<gpu::MemcpyOp> {
public:
  ConvertMemcpyOpToGpuRuntimeCallPattern(const LLVMTypeConverter &typeConverter)
      : ConvertOpToGpuRuntimeCallPattern<gpu::MemcpyOp>(typeConverter) {}

private:
  LogicalResult
  matchAndRewrite(gpu::MemcpyOp memcpyOp, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override;
};

/// A rewrite pattern to convert gpu.memset operations into a GPU runtime
/// call. Currently it supports CUDA and ROCm (HIP).
class ConvertMemsetOpToGpuRuntimeCallPattern
    : public ConvertOpToGpuRuntimeCallPattern<gpu::MemsetOp> {
public:
  ConvertMemsetOpToGpuRuntimeCallPattern(const LLVMTypeConverter &typeConverter)
      : ConvertOpToGpuRuntimeCallPattern<gpu::MemsetOp>(typeConverter) {}

private:
  LogicalResult
  matchAndRewrite(gpu::MemsetOp memsetOp, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override;
};

/// A rewrite pattern to convert gpu.set_default_device to a GPU runtime call.
/// Currently supports CUDA and ROCm (HIP)
class ConvertSetDefaultDeviceOpToGpuRuntimeCallPattern
    : public ConvertOpToGpuRuntimeCallPattern<gpu::SetDefaultDeviceOp> {
public:
  ConvertSetDefaultDeviceOpToGpuRuntimeCallPattern(
      const LLVMTypeConverter &typeConverter)
      : ConvertOpToGpuRuntimeCallPattern<gpu::SetDefaultDeviceOp>(
            typeConverter) {}

  LogicalResult
  matchAndRewrite(gpu::SetDefaultDeviceOp op, OpAdaptor adaptor,
                  ConversionPatternRewriter &rewriter) const override;
};

/// Generic rewriting rule for operation on sparse matrices.
/// Currently supports CUDA (by means of cuSparse and cuSparseLt).
#define DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(op_name)                \
  class Convert##op_name##ToGpuRuntimeCallPattern                              \
      : public ConvertOpToGpuRuntimeCallPattern<gpu::op_name> {                \
  public:                                                                      \
    Convert##op_name##ToGpuRuntimeCallPattern(                                 \
        const LLVMTypeConverter &typeConverter)                                \
        : ConvertOpToGpuRuntimeCallPattern<gpu::op_name>(typeConverter) {}     \
                                                                               \
  private:                                                                     \
    LogicalResult                                                              \
    matchAndRewrite(gpu::op_name op, OpAdaptor adaptor,                        \
                    ConversionPatternRewriter &rewriter) const override;       \
  };

DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(CreateDnTensorOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(DestroyDnTensorOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(CreateCooOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(CreateCooAoSOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(CreateCsrOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(CreateCscOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(CreateBsrOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(Create2To4SpMatOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(DestroySpMatOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(SpMVBufferSizeOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(SpMVOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(SpMMBufferSizeOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(SDDMMBufferSizeOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(SpMMOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(SDDMMOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(SpGEMMCreateDescrOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(SpGEMMDestroyDescrOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(SpGEMMWorkEstimationOrComputeOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(SpGEMMCopyOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(SpMatGetSizeOp)
DECLARE_CONVERT_OP_TO_GPU_RUNTIME_CALL_PATTERN(SetCsrPointersOp)

} // namespace

void GpuToLLVMConversionPass::runOnOperation() {
  MLIRContext *context = &getContext();

  // Perform progressive lowering of vector transfer operations.
  {
    RewritePatternSet patterns(&getContext());
    // Vector transfer ops with rank > 1 should be lowered with VectorToSCF.
    vector::populateVectorTransferLoweringPatterns(patterns,
                                                   /*maxTransferRank=*/1);
    if (failed(applyPatternsGreedily(getOperation(), std::move(patterns))))
      return signalPassFailure();
  }

  LowerToLLVMOptions options(context);
  options.useBarePtrCallConv = hostBarePtrCallConv;
  RewritePatternSet patterns(context);
  ConversionTarget target(*context);
  target.addLegalDialect<LLVM::LLVMDialect>();
  LLVMTypeConverter converter(context, options);

  // Populate all patterns from all dialects that implement the
  // `ConvertToLLVMPatternInterface` interface.
  for (Dialect *dialect : context->getLoadedDialects()) {
    auto iface = dyn_cast<ConvertToLLVMPatternInterface>(dialect);
    if (!iface)
      continue;
    iface->populateConvertToLLVMConversionPatterns(target, converter, patterns);
  }

  // Preserve GPU modules and binaries. Modules are preserved as they can be
  // converted later by `gpu-module-to-binary`.
  target.addLegalOp<gpu::GPUModuleOp, gpu::BinaryOp>();
  // Accept as legal LaunchFuncOps if the operands have been lowered.
  target.addDynamicallyLegalOp<gpu::LaunchFuncOp>(
      [&](gpu::LaunchFuncOp op) -> bool { return converter.isLegal(op); });

  // These aren't covered by the ConvertToLLVMPatternInterface right now.
  populateVectorToLLVMConversionPatterns(converter, patterns);
  populateFinalizeMemRefToLLVMConversionPatterns(converter, patterns);
  populateAsyncStructuralTypeConversionsAndLegality(converter, patterns,
                                                    target);
  populateGpuToLLVMConversionPatterns(converter, patterns,
                                      kernelBarePtrCallConv,
                                      kernelIntersperseSizeCallConv);

  if (failed(
          applyPartialConversion(getOperation(), target, std::move(patterns))))
    signalPassFailure();
}

LLVM::CallOp FunctionCallBuilder::create(Location loc, OpBuilder &builder,
                                         ArrayRef<Value> arguments) const {
  auto module = builder.getBlock()->getParent()->getParentOfType<ModuleOp>();
  auto function = [&] {
    if (auto function = module.lookupSymbol<LLVM::LLVMFuncOp>(functionName))
      return function;
    return OpBuilder::atBlockEnd(module.getBody())
        .create<LLVM::LLVMFuncOp>(loc, functionName, functionType);
  }();
  return LLVM::CallOp::create(builder, loc, function, arguments);
}

// Corresponding to cusparseIndexType_t defined in cusparse.h.
static int32_t getCuSparseIndexTypeFrom(Type type) {
  if (type.isInteger(16))
    return 1; // CUSPARSE_INDEX_16U
  if (type.isInteger(32))
    return 2; // CUSPARSE_INDEX_32I
  return 3;   // CUSPARSE_INDEX_64I
}

static int32_t getCuSparseLtDataTypeFrom(Type type) {
  if (type.isF16())
    return 0; // CUSPARSE_COMPUTE_16F,
  if (type.isInteger(32))
    return 1; // CUSPARSE_COMPUTE_32I
  llvm_unreachable("unsupported type");
  // TODO: add support to TF32
}

// Corresponding to cudaDataType_t defined in CUDA library_types.h.
static int32_t getCuSparseDataTypeFrom(Type type) {
  if (llvm::isa<ComplexType>(type)) {
    // get the element type
    auto elementType = cast<ComplexType>(type).getElementType();
    if (elementType.isBF16())
      return 15; // CUDA_C_16BF
    if (elementType.isF16())
      return 6; // CUDA_C_16F
    if (elementType.isF32())
      return 4; // CUDA_C_32F
    if (elementType.isF64())
      return 5; // CUDA_C_64F
    if (elementType.isInteger(8))
      return 7; // CUDA_C_8I
    if (elementType.isInteger(16))
      return 21; // CUDA_C_16I
    if (elementType.isInteger(32))
      return 11; // CUDA_C_32I
  }
  if (type.isBF16())
    return 14; // CUDA_R_16BF
  if (type.isF16())
    return 2; // CUDA_R_16F
  if (type.isF32())
    return 0; // CUDA_R_32F
  if (type.isF64())
    return 1; // CUDA_R_64F
  if (type.isInteger(8))
    return 3; // CUDA_R_8I
  if (type.isInteger(16))
    return 20; // CUDA_R_16I
  if (type.isInteger(32))
    return 10; // CUDA_R_32I

  llvm_unreachable("unsupported element type");
}

static gpu::Prune2To4SpMatFlag get2To4PruneFlag(Value spMat) {
  return spMat.getDefiningOp<gpu::Create2To4SpMatOp>().getPruneFlag();
}

// TODO:  We may want a run-time (of the mlir compiler) disablement/warning:
// cusparseLt currently won't work for cuda architecture <8.0 and will trigger a
// runtime (of the CUDA program) error , but it might be great if we could at
// least output a warning when we found the target architecture is <8.0 and the
// user still wants to use cusparseLt. to make sure when lowering gpu sparse
// dialect to llvm calls, the cusparselt calls are disabled for cuda
// architecture <8.0
static bool is2To4Sparsity(Value spMat) {
  if (auto op = spMat.getDefiningOp<gpu::Create2To4SpMatOp>())
    return true;
  if (auto op = spMat.getDefiningOp<gpu::CreateCooOp>())
    return false;
  if (auto op = spMat.getDefiningOp<gpu::CreateCooAoSOp>())
    return false;
  if (auto op = spMat.getDefiningOp<gpu::CreateCsrOp>())
    return false;
  if (auto op = spMat.getDefiningOp<gpu::CreateCscOp>())
    return false;
  if (auto op = spMat.getDefiningOp<gpu::CreateBsrOp>())
    return false;
  // Print the spMat defining op
  spMat.getDefiningOp()->print(llvm::errs());
  llvm_unreachable("cannot find spmat def");
}

static bool isSpMMCusparseLtOp(Value op) {
  for (Operation *user : op.getUsers()) {
    auto spmmOp = dyn_cast<gpu::SpMMOp>(user);
    // If the other operator is 50% sparsity then we should use cusparseLt
    if (!spmmOp)
      continue;
    if (is2To4Sparsity(spmmOp.getSpmatA()))
      return true;
  }
  return false;
}

// Returns whether all operands are of LLVM type.
static LogicalResult areAllLLVMTypes(Operation *op, ValueRange operands,
                                     ConversionPatternRewriter &rewriter) {
  if (!llvm::all_of(operands, [](Value value) {
        return LLVM::isCompatibleType(value.getType());
      }))
    return rewriter.notifyMatchFailure(
        op, "Cannot convert if operands aren't of LLVM type.");
  return success();
}

static LogicalResult
isAsyncWithOneDependency(ConversionPatternRewriter &rewriter,
                         gpu::AsyncOpInterface op) {
  if (op.getAsyncDependencies().size() != 1)
    return rewriter.notifyMatchFailure(
        op, "Can only convert with exactly one async dependency.");

  if (!op.getAsyncToken())
    return rewriter.notifyMatchFailure(op, "Can convert only async version.");

  return success();
}

LogicalResult ConvertHostRegisterOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::HostRegisterOp hostRegisterOp, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  auto *op = hostRegisterOp.getOperation();
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)))
    return failure();

  Location loc = op->getLoc();

  auto memRefType = hostRegisterOp.getValue().getType();
  auto elementType = cast<UnrankedMemRefType>(memRefType).getElementType();
  auto elementSize = getSizeInBytes(loc, elementType, rewriter);

  auto arguments = getTypeConverter()->promoteOperands(
      loc, op->getOperands(), adaptor.getOperands(), rewriter);
  arguments.push_back(elementSize);
  hostRegisterCallBuilder.create(loc, rewriter, arguments);

  rewriter.eraseOp(op);
  return success();
}

LogicalResult ConvertHostUnregisterOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::HostUnregisterOp hostUnregisterOp, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  Operation *op = hostUnregisterOp.getOperation();
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)))
    return failure();

  Location loc = op->getLoc();

  auto memRefType = hostUnregisterOp.getValue().getType();
  auto elementType = cast<UnrankedMemRefType>(memRefType).getElementType();
  auto elementSize = getSizeInBytes(loc, elementType, rewriter);

  auto arguments = getTypeConverter()->promoteOperands(
      loc, op->getOperands(), adaptor.getOperands(), rewriter);
  arguments.push_back(elementSize);
  hostUnregisterCallBuilder.create(loc, rewriter, arguments);

  rewriter.eraseOp(op);
  return success();
}

LogicalResult ConvertAllocOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::AllocOp allocOp, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {

  MemRefType memRefType = allocOp.getType();

  if (failed(areAllLLVMTypes(allocOp, adaptor.getOperands(), rewriter)) ||
      !isConvertibleAndHasIdentityMaps(memRefType))
    return failure();

  auto loc = allocOp.getLoc();

  bool isShared = allocOp.getHostShared();

  if (isShared && allocOp.getAsyncToken())
    return rewriter.notifyMatchFailure(
        allocOp, "Host Shared allocation cannot be done async");
  if (!isShared && failed(isAsyncWithOneDependency(rewriter, allocOp)))
    return failure();

  // Get shape of the memref as values: static sizes are constant
  // values and dynamic sizes are passed to 'alloc' as operands.
  SmallVector<Value, 4> shape;
  SmallVector<Value, 4> strides;
  Value sizeBytes;
  getMemRefDescriptorSizes(loc, memRefType, adaptor.getDynamicSizes(), rewriter,
                           shape, strides, sizeBytes);

  // Allocate the underlying buffer and store a pointer to it in the MemRef
  // descriptor.
  auto nullPtr = mlir::LLVM::ZeroOp::create(rewriter, loc, llvmPointerType);
  Value stream = adaptor.getAsyncDependencies().empty()
                     ? nullPtr
                     : adaptor.getAsyncDependencies().front();

  auto isHostShared = mlir::LLVM::ConstantOp::create(
      rewriter, loc, llvmInt8Type, rewriter.getI8IntegerAttr(isShared));

  Value allocatedPtr =
      allocCallBuilder.create(loc, rewriter, {sizeBytes, stream, isHostShared})
          .getResult();

  // No alignment.
  Value alignedPtr = allocatedPtr;

  // Create the MemRef descriptor.
  auto memRefDescriptor = this->createMemRefDescriptor(
      loc, memRefType, allocatedPtr, alignedPtr, shape, strides, rewriter);

  if (allocOp.getAsyncToken()) {
    // Async alloc: make dependent ops use the same stream.
    rewriter.replaceOp(allocOp, {memRefDescriptor, stream});
  } else {
    rewriter.replaceOp(allocOp, {memRefDescriptor});
  }

  return success();
}

LogicalResult ConvertDeallocOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::DeallocOp deallocOp, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(deallocOp, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, deallocOp)))
    return failure();

  Location loc = deallocOp.getLoc();

  Value pointer =
      MemRefDescriptor(adaptor.getMemref()).allocatedPtr(rewriter, loc);
  Value stream = adaptor.getAsyncDependencies().front();
  deallocCallBuilder.create(loc, rewriter, {pointer, stream});

  rewriter.replaceOp(deallocOp, {stream});
  return success();
}

static bool isGpuAsyncTokenType(Value value) {
  return isa<gpu::AsyncTokenType>(value.getType());
}

// Converts !gpu.async.token operands of `async.yield` to runtime calls. The
// !gpu.async.token are lowered to stream within the async.execute region, but
// are passed as events between them. For each !gpu.async.token operand, we
// create an event and record it on the stream.
LogicalResult ConvertAsyncYieldToGpuRuntimeCallPattern::matchAndRewrite(
    async::YieldOp yieldOp, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (llvm::none_of(yieldOp.getOperands(), isGpuAsyncTokenType))
    return rewriter.notifyMatchFailure(yieldOp, "no gpu async token operand");

  Location loc = yieldOp.getLoc();
  SmallVector<Value, 4> newOperands(adaptor.getOperands());
  llvm::SmallDenseSet<Value> streams;
  for (auto &operand : yieldOp->getOpOperands()) {
    if (!isGpuAsyncTokenType(operand.get()))
      continue;
    auto idx = operand.getOperandNumber();
    auto stream = adaptor.getOperands()[idx];
    auto event = eventCreateCallBuilder.create(loc, rewriter, {}).getResult();
    eventRecordCallBuilder.create(loc, rewriter, {event, stream});
    newOperands[idx] = event;
    streams.insert(stream);
  }
  for (auto stream : streams)
    streamDestroyCallBuilder.create(loc, rewriter, {stream});

  rewriter.modifyOpInPlace(yieldOp, [&] { yieldOp->setOperands(newOperands); });
  return success();
}

// Returns whether `value` is the result of an LLVM::CallOp to `functionName`.
static bool isDefinedByCallTo(Value value, StringRef functionName) {
  assert(isa<LLVM::LLVMPointerType>(value.getType()));
  if (auto defOp = value.getDefiningOp<LLVM::CallOp>())
    return *defOp.getCallee() == functionName;
  return false;
}

// Converts `gpu.wait` to runtime calls. The converted op synchronizes the host
// with the stream/event operands. The operands are destroyed. That is, it
// assumes that it is not used afterwards or elsewhere. Otherwise we will get a
// runtime error. Eventually, we should guarantee this property.
LogicalResult ConvertWaitOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::WaitOp waitOp, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (waitOp.getAsyncToken())
    return rewriter.notifyMatchFailure(waitOp, "Cannot convert async op.");

  Location loc = waitOp.getLoc();

  for (auto operand : adaptor.getOperands()) {
    if (isDefinedByCallTo(operand, streamCreateCallBuilder.functionName)) {
      // The converted operand's definition created a stream.
      streamSynchronizeCallBuilder.create(loc, rewriter, {operand});
      streamDestroyCallBuilder.create(loc, rewriter, {operand});
    } else {
      // Otherwise the converted operand is an event. This assumes that we use
      // events in control flow code as well.
      eventSynchronizeCallBuilder.create(loc, rewriter, {operand});
      eventDestroyCallBuilder.create(loc, rewriter, {operand});
    }
  }

  rewriter.eraseOp(waitOp);
  return success();
}

// Converts `gpu.wait async` to runtime calls. The converted op creates a new
// stream that is synchronized with stream/event operands. The operands are
// destroyed. That is, it assumes that it is not used afterwards or elsewhere.
// Otherwise we will get a runtime error. Eventually, we should guarantee this
// property.
LogicalResult ConvertWaitAsyncOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::WaitOp waitOp, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (!waitOp.getAsyncToken())
    return rewriter.notifyMatchFailure(waitOp, "Can only convert async op.");

  Location loc = waitOp.getLoc();

  auto insertionPoint = rewriter.saveInsertionPoint();
  SmallVector<Value, 1> events;
  for (auto pair :
       llvm::zip(waitOp.getAsyncDependencies(), adaptor.getOperands())) {
    auto operand = std::get<1>(pair);
    if (isDefinedByCallTo(operand, streamCreateCallBuilder.functionName)) {
      // The converted operand's definition created a stream. Insert an event
      // into the stream just after the last use of the original token operand.
      auto *defOp = std::get<0>(pair).getDefiningOp();
      rewriter.setInsertionPointAfter(defOp);
      auto event = eventCreateCallBuilder.create(loc, rewriter, {}).getResult();
      eventRecordCallBuilder.create(loc, rewriter, {event, operand});
      events.push_back(event);
    } else {
      // Otherwise the converted operand is an event. This assumes that we use
      // events in control flow code as well.
      events.push_back(operand);
    }
  }
  rewriter.restoreInsertionPoint(insertionPoint);
  auto stream = streamCreateCallBuilder.create(loc, rewriter, {}).getResult();
  for (auto event : events)
    streamWaitEventCallBuilder.create(loc, rewriter, {stream, event});
  for (auto event : events)
    eventDestroyCallBuilder.create(loc, rewriter, {event});
  rewriter.replaceOp(waitOp, {stream});

  return success();
}

// Legalize the op's operands.
LogicalResult LegalizeLaunchFuncOpPattern::matchAndRewrite(
    gpu::LaunchFuncOp launchOp, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(launchOp, adaptor.getOperands(), rewriter)))
    return failure();

  if (launchOp.getAsyncDependencies().size() > 1)
    return rewriter.notifyMatchFailure(
        launchOp, "Cannot convert with more than one async dependency.");

  // Fail when the synchronous version of the op has async dependencies. The
  // lowering destroys the stream, and we do not want to check that there is no
  // use of the stream after this op.
  if (!launchOp.getAsyncToken() && !launchOp.getAsyncDependencies().empty())
    return rewriter.notifyMatchFailure(
        launchOp, "Cannot convert non-async op with async dependencies.");

  Location loc = launchOp.getLoc();

  Value stream = Value();
  if (!adaptor.getAsyncDependencies().empty())
    stream = adaptor.getAsyncDependencies().front();
  // If the async keyword is present and there are no dependencies, then a
  // stream must be created to pass to subsequent operations.
  else if (launchOp.getAsyncToken())
    stream = streamCreateCallBuilder.create(loc, rewriter, {}).getResult();

  // Lower the kernel operands to match kernel parameters.
  // Note: If `useBarePtrCallConv` is set in the type converter's options,
  // the value of `kernelBarePtrCallConv` will be ignored.
  OperandRange origArguments = launchOp.getKernelOperands();
  SmallVector<Value, 8> llvmArguments = getTypeConverter()->promoteOperands(
      loc, origArguments, adaptor.getKernelOperands(), rewriter,
      /*useBarePtrCallConv=*/kernelBarePtrCallConv);
  SmallVector<Value, 8> llvmArgumentsWithSizes;

  // Intersperse size information if requested.
  if (kernelIntersperseSizeCallConv) {
    if (origArguments.size() != llvmArguments.size()) {
      // This shouldn't happen if the bare-pointer calling convention is used.
      return rewriter.notifyMatchFailure(
          launchOp,
          "Cannot add sizes to arguments with one-to-many LLVM IR expansion.");
    }

    llvmArgumentsWithSizes.reserve(llvmArguments.size() * 2);
    for (auto [llvmArg, origArg] : zip_equal(llvmArguments, origArguments)) {
      auto memrefTy = dyn_cast<MemRefType>(origArg.getType());
      if (!memrefTy) {
        return rewriter.notifyMatchFailure(
            launchOp, "Operand to launch op is not a memref.");
      }

      if (!memrefTy.hasStaticShape() ||
          !memrefTy.getElementType().isIntOrFloat()) {
        return rewriter.notifyMatchFailure(
            launchOp, "Operand to launch op is not a memref with a static "
                      "shape and an integer or float element type.");
      }

      unsigned bitwidth = memrefTy.getElementTypeBitWidth();
      if (bitwidth % 8 != 0) {
        return rewriter.notifyMatchFailure(
            launchOp, "Operand to launch op is not a memref with a "
                      "byte-aligned element type.");
      }

      uint64_t staticSize = static_cast<uint64_t>(bitwidth / 8) *
                            static_cast<uint64_t>(memrefTy.getNumElements());

      Value sizeArg = LLVM::ConstantOp::create(
          rewriter, loc, getIndexType(), rewriter.getIndexAttr(staticSize));
      llvmArgumentsWithSizes.push_back(llvmArg); // Presumably a bare pointer.
      llvmArgumentsWithSizes.push_back(sizeArg);
    }
  }

  std::optional<gpu::KernelDim3> clusterSize = std::nullopt;
  if (launchOp.hasClusterSize()) {
    clusterSize =
        gpu::KernelDim3{adaptor.getClusterSizeX(), adaptor.getClusterSizeY(),
                        adaptor.getClusterSizeZ()};
  }
  gpu::LaunchFuncOp::create(
      rewriter, launchOp.getLoc(), launchOp.getKernelAttr(),
      gpu::KernelDim3{adaptor.getGridSizeX(), adaptor.getGridSizeY(),
                      adaptor.getGridSizeZ()},
      gpu::KernelDim3{adaptor.getBlockSizeX(), adaptor.getBlockSizeY(),
                      adaptor.getBlockSizeZ()},
      adaptor.getDynamicSharedMemorySize(),
      llvmArgumentsWithSizes.empty() ? llvmArguments : llvmArgumentsWithSizes,
      stream, clusterSize);
  if (launchOp.getAsyncToken())
    rewriter.replaceOp(launchOp, {stream});
  else
    rewriter.eraseOp(launchOp);
  return success();
}

static Value bitAndAddrspaceCast(Location loc,
                                 ConversionPatternRewriter &rewriter,
                                 LLVM::LLVMPointerType destinationType,
                                 Value sourcePtr,
                                 const LLVMTypeConverter &typeConverter) {
  auto sourceTy = cast<LLVM::LLVMPointerType>(sourcePtr.getType());
  if (destinationType.getAddressSpace() != sourceTy.getAddressSpace())
    sourcePtr = LLVM::AddrSpaceCastOp::create(
        rewriter, loc,
        LLVM::LLVMPointerType::get(rewriter.getContext(),
                                   destinationType.getAddressSpace()),
        sourcePtr);
  return sourcePtr;
}

LogicalResult ConvertMemcpyOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::MemcpyOp memcpyOp, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  auto memRefType = cast<MemRefType>(memcpyOp.getSrc().getType());

  if (failed(areAllLLVMTypes(memcpyOp, adaptor.getOperands(), rewriter)) ||
      !isConvertibleAndHasIdentityMaps(memRefType) ||
      failed(isAsyncWithOneDependency(rewriter, memcpyOp)))
    return failure();

  auto loc = memcpyOp.getLoc();

  MemRefDescriptor srcDesc(adaptor.getSrc());
  Value numElements = getNumElements(rewriter, loc, memRefType, srcDesc);

  Type elementPtrType = getElementPtrType(memRefType);
  Value nullPtr = LLVM::ZeroOp::create(rewriter, loc, elementPtrType);
  Value gepPtr = LLVM::GEPOp::create(
      rewriter, loc, elementPtrType,
      typeConverter->convertType(memRefType.getElementType()), nullPtr,
      numElements);
  auto sizeBytes =
      LLVM::PtrToIntOp::create(rewriter, loc, getIndexType(), gepPtr);

  auto src = bitAndAddrspaceCast(loc, rewriter, llvmPointerType,
                                 srcDesc.alignedPtr(rewriter, loc),
                                 *getTypeConverter());
  auto dst = bitAndAddrspaceCast(
      loc, rewriter, llvmPointerType,
      MemRefDescriptor(adaptor.getDst()).alignedPtr(rewriter, loc),
      *getTypeConverter());

  auto stream = adaptor.getAsyncDependencies().front();
  memcpyCallBuilder.create(loc, rewriter, {dst, src, sizeBytes, stream});

  rewriter.replaceOp(memcpyOp, {stream});

  return success();
}

LogicalResult ConvertMemsetOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::MemsetOp memsetOp, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  auto memRefType = cast<MemRefType>(memsetOp.getDst().getType());

  if (failed(areAllLLVMTypes(memsetOp, adaptor.getOperands(), rewriter)) ||
      !isConvertibleAndHasIdentityMaps(memRefType) ||
      failed(isAsyncWithOneDependency(rewriter, memsetOp)))
    return failure();

  auto loc = memsetOp.getLoc();

  Type valueType = adaptor.getValue().getType();
  unsigned bitWidth = valueType.getIntOrFloatBitWidth();
  // Ints and floats of 16 or 32 bit width are allowed.
  if (!valueType.isIntOrFloat() || (bitWidth != 16 && bitWidth != 32)) {
    return rewriter.notifyMatchFailure(
        memsetOp, "value must be a 16 or 32 bit int or float");
  }

  unsigned valueTypeWidth = valueType.getIntOrFloatBitWidth();
  Type bitCastType = valueTypeWidth == 32 ? llvmInt32Type : llvmInt16Type;

  MemRefDescriptor dstDesc(adaptor.getDst());
  Value numElements = getNumElements(rewriter, loc, memRefType, dstDesc);

  auto value =
      LLVM::BitcastOp::create(rewriter, loc, bitCastType, adaptor.getValue());
  auto dst = bitAndAddrspaceCast(loc, rewriter, llvmPointerType,
                                 dstDesc.alignedPtr(rewriter, loc),
                                 *getTypeConverter());

  auto stream = adaptor.getAsyncDependencies().front();
  FunctionCallBuilder builder =
      valueTypeWidth == 32 ? memset32CallBuilder : memset16CallBuilder;
  builder.create(loc, rewriter, {dst, value, numElements, stream});

  rewriter.replaceOp(memsetOp, {stream});
  return success();
}

LogicalResult ConvertSetDefaultDeviceOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::SetDefaultDeviceOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  Location loc = op.getLoc();
  auto call = setDefaultDeviceCallBuilder.create(loc, rewriter,
                                                 {adaptor.getDevIndex()});
  rewriter.replaceOp(op, call);
  return success();
}

template <typename T>
static Value genConstInt32From(OpBuilder &builder, Location loc, T tValue) {
  Type llvmInt32Type = builder.getIntegerType(32);
  return LLVM::ConstantOp::create(builder, loc, llvmInt32Type,
                                  static_cast<int32_t>(tValue));
}

template <typename T>
static Value genConstFloat32From(OpBuilder &builder, Location loc, T tValue) {
  Type llvmFloat32Type = builder.getF32Type();
  return LLVM::ConstantOp::create(
      builder, loc, llvmFloat32Type,
      builder.getF32FloatAttr(static_cast<float>(tValue)));
}

LogicalResult ConvertCreateDnTensorOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::CreateDnTensorOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto stream = adaptor.getAsyncDependencies().front();
  Value pTensor =
      MemRefDescriptor(adaptor.getMemref()).allocatedPtr(rewriter, loc);
  Type dType = op.getMemref().getType().getElementType();
  auto dtp = genConstInt32From(rewriter, loc, getCuSparseDataTypeFrom(dType));

  SmallVector<Value, 4> dims;
  for (Value dim : adaptor.getDims()) {
    dims.push_back(dim);
  }

  Value handle;
  // TODO: For now, we track the use of the handle and lower it to cusparse /
  // cusparseLt accordingly. If in a block, both cusparse and cusparseLt are
  // used, we require two separate Creation ops to be the correct logic. In
  // future, we may add support to using one handle in sparse tensor / GPU
  // dialect in both cusparse and cusparseLt. use the cusparseLt create call if
  // the dnmat is used with spmat with 2:4 sparsity
  if (dims.size() == 2) {
    if (isSpMMCusparseLtOp(op.getDnTensor())) {
      auto handleSz = LLVM::ConstantOp::create(rewriter, loc, getIndexType(),
                                               rewriter.getIndexAttr(11032));
      handle = LLVM::AllocaOp::create(rewriter, loc, llvmPointerType,
                                      llvmInt8Type, handleSz, /*alignment=*/16);
      handle = LLVM::BitcastOp::create(rewriter, loc, llvmPointerType, handle);

      createLtDnMatCallBuilder
          .create(loc, rewriter,
                  {handle, dims[0], dims[1], pTensor, dtp, stream})
          .getResult();
    } else {
      handle =
          createDnMatCallBuilder
              .create(loc, rewriter, {dims[0], dims[1], pTensor, dtp, stream})
              .getResult();
    }
  } else {
    assert(dims.size() == 1 && "Only 1D and 2D tensors are supported");
    handle = createDnVecCallBuilder
                 .create(loc, rewriter, {dims[0], pTensor, dtp, stream})
                 .getResult();
  }
  rewriter.replaceOp(op, {handle, stream});
  return success();
}

LogicalResult ConvertDestroyDnTensorOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::DestroyDnTensorOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto stream = adaptor.getAsyncDependencies().front();
  auto definingOp = op.getDnTensor().getDefiningOp<gpu::CreateDnTensorOp>();
  SmallVector<Value, 4> dims;
  for (Value dim : definingOp.getDims()) {
    dims.push_back(dim);
  }
  if (dims.size() == 2) {
    // Use the cusparseLt destroy call if the dnmat is used with spmat with
    // 2:4 sparsity
    if (isSpMMCusparseLtOp(op.getDnTensor())) {
      destroyCuSparseLtDnMatBuilder.create(loc, rewriter,
                                           {adaptor.getDnTensor(), stream});
    } else {
      destroyDnMatCallBuilder.create(loc, rewriter,
                                     {adaptor.getDnTensor(), stream});
    }
  } else {
    assert(dims.size() == 1 && "Only 1D and 2D tensors are supported");
    destroyDnVecCallBuilder.create(loc, rewriter,
                                   {adaptor.getDnTensor(), stream});
  }
  rewriter.replaceOp(op, {stream});
  return success();
}

LogicalResult ConvertCreateCooOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::CreateCooOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto stream = adaptor.getAsyncDependencies().front();
  Value pRowIdxs =
      MemRefDescriptor(adaptor.getRowIdxs()).allocatedPtr(rewriter, loc);
  Value pColIdxs =
      MemRefDescriptor(adaptor.getColIdxs()).allocatedPtr(rewriter, loc);
  Value pValues =
      MemRefDescriptor(adaptor.getValues()).allocatedPtr(rewriter, loc);
  Type iType =
      llvm::cast<MemRefType>(op.getColIdxs().getType()).getElementType();
  Type dType =
      llvm::cast<MemRefType>(op.getValues().getType()).getElementType();
  auto itp = genConstInt32From(rewriter, loc, getCuSparseIndexTypeFrom(iType));
  auto dtp = genConstInt32From(rewriter, loc, getCuSparseDataTypeFrom(dType));
  auto handle =
      createCooCallBuilder
          .create(loc, rewriter,
                  {adaptor.getRows(), adaptor.getCols(), adaptor.getNnz(),
                   pRowIdxs, pColIdxs, pValues, itp, dtp, stream})
          .getResult();
  rewriter.replaceOp(op, {handle, stream});
  return success();
}

LogicalResult ConvertCreateCooAoSOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::CreateCooAoSOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto stream = adaptor.getAsyncDependencies().front();
  Value pIdxs = MemRefDescriptor(adaptor.getIdxs()).allocatedPtr(rewriter, loc);
  Value pValues =
      MemRefDescriptor(adaptor.getValues()).allocatedPtr(rewriter, loc);
  Type iType = llvm::cast<MemRefType>(op.getIdxs().getType()).getElementType();
  Type dType =
      llvm::cast<MemRefType>(op.getValues().getType()).getElementType();
  auto itp = genConstInt32From(rewriter, loc, getCuSparseIndexTypeFrom(iType));
  auto dtp = genConstInt32From(rewriter, loc, getCuSparseDataTypeFrom(dType));
  auto handle =
      createCooAoSCallBuilder
          .create(loc, rewriter,
                  {adaptor.getRows(), adaptor.getCols(), adaptor.getNnz(),
                   pIdxs, pValues, itp, dtp, stream})
          .getResult();
  rewriter.replaceOp(op, {handle, stream});
  return success();
}

LogicalResult ConvertCreateCsrOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::CreateCsrOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto stream = adaptor.getAsyncDependencies().front();
  Value pRowPos =
      MemRefDescriptor(adaptor.getRowPos()).allocatedPtr(rewriter, loc);
  Value pColIdxs =
      MemRefDescriptor(adaptor.getColIdxs()).allocatedPtr(rewriter, loc);
  Value pValues =
      MemRefDescriptor(adaptor.getValues()).allocatedPtr(rewriter, loc);
  Type pType =
      llvm::cast<MemRefType>(op.getRowPos().getType()).getElementType();
  Type iType =
      llvm::cast<MemRefType>(op.getColIdxs().getType()).getElementType();
  Type dType =
      llvm::cast<MemRefType>(op.getValues().getType()).getElementType();
  auto ptp = genConstInt32From(rewriter, loc, getCuSparseIndexTypeFrom(pType));
  auto itp = genConstInt32From(rewriter, loc, getCuSparseIndexTypeFrom(iType));
  auto dtp = genConstInt32From(rewriter, loc, getCuSparseDataTypeFrom(dType));
  auto handle =
      createCsrCallBuilder
          .create(loc, rewriter,
                  {adaptor.getRows(), adaptor.getCols(), adaptor.getNnz(),
                   pRowPos, pColIdxs, pValues, ptp, itp, dtp, stream})
          .getResult();
  rewriter.replaceOp(op, {handle, stream});
  return success();
}

LogicalResult ConvertCreate2To4SpMatOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::Create2To4SpMatOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto stream = adaptor.getAsyncDependencies().front();
  Value pMat =
      MemRefDescriptor(adaptor.getMemref()).allocatedPtr(rewriter, loc);
  Type dType =
      llvm::cast<MemRefType>(op.getMemref().getType()).getElementType();
  auto dtp = genConstInt32From(rewriter, loc, getCuSparseDataTypeFrom(dType));

  // CUDA runner asserts the size is 44104 bytes.
  auto handleSz = LLVM::ConstantOp::create(rewriter, loc, getIndexType(),
                                           rewriter.getIndexAttr(44104));
  Value handle = LLVM::AllocaOp::create(
      rewriter, loc, llvmPointerType, llvmInt8Type, handleSz, /*alignment=*/16);
  handle = LLVM::BitcastOp::create(rewriter, loc, llvmPointerType, handle);

  create2To4SpMatCallBuilder
      .create(loc, rewriter,
              {handle, adaptor.getRows(), adaptor.getCols(), pMat, dtp, stream})
      .getResult();
  rewriter.replaceOp(op, {handle, stream});
  return success();
}

LogicalResult ConvertDestroySpMatOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::DestroySpMatOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto stream = adaptor.getAsyncDependencies().front();
  // Use the cusparseLt destroy call if the spmat is 2:4 sparsity
  if (is2To4Sparsity(op.getSpmat())) {
    destroyCuSparseLtSpMatBuilder.create(loc, rewriter,
                                         {adaptor.getSpmat(), stream});

  } else {
    destroySpMatCallBuilder.create(loc, rewriter, {adaptor.getSpmat(), stream});
  }
  rewriter.replaceOp(op, {stream});
  return success();
}

LogicalResult ConvertSpMVBufferSizeOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::SpMVBufferSizeOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto modeA = genConstInt32From(rewriter, loc, op.getModeA());
  auto computeType = genConstInt32From(
      rewriter, loc, getCuSparseDataTypeFrom(adaptor.getComputeType()));
  auto stream = adaptor.getAsyncDependencies().front();
  auto bufferSize = spMVBufferSizeCallBuilder
                        .create(loc, rewriter,
                                {modeA, adaptor.getSpmatA(), adaptor.getDnX(),
                                 adaptor.getDnY(), computeType, stream})
                        .getResult();
  rewriter.replaceOp(op, {bufferSize, stream});
  return success();
}

LogicalResult ConvertSpMVOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::SpMVOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto modeA = genConstInt32From(rewriter, loc, adaptor.getModeA());
  auto computeType = genConstInt32From(
      rewriter, loc, getCuSparseDataTypeFrom(adaptor.getComputeType()));
  auto stream = adaptor.getAsyncDependencies().front();
  Value pBuf =
      MemRefDescriptor(adaptor.getBuffer()).allocatedPtr(rewriter, loc);
  spMVCallBuilder.create(loc, rewriter,
                         {modeA, adaptor.getSpmatA(), adaptor.getDnX(),
                          adaptor.getDnY(), computeType, pBuf, stream});
  rewriter.replaceOp(op, {stream});
  return success();
}

LogicalResult ConvertSpMMBufferSizeOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::SpMMBufferSizeOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto modeA = genConstInt32From(rewriter, loc, adaptor.getModeA());
  auto modeB = genConstInt32From(rewriter, loc, adaptor.getModeB());
  auto stream = adaptor.getAsyncDependencies().front();
  Value bufferSize;
  if (is2To4Sparsity(op.getSpmatA())) {
    auto pruneFlag =
        genConstInt32From(rewriter, loc, get2To4PruneFlag(op.getSpmatA()));
    auto computeType = genConstInt32From(
        rewriter, loc, getCuSparseLtDataTypeFrom(adaptor.getComputeType()));
    auto three = LLVM::ConstantOp::create(rewriter, loc, getIndexType(),
                                          rewriter.getIndexAttr(3));
    auto bufferSize =
        LLVM::AllocaOp::create(rewriter, loc, llvmPointerType, llvmPointerType,
                               three, /*alignment=*/16);
    createCuSparseLtSpMMBufferSizeBuilder
        .create(loc, rewriter,
                {bufferSize, modeA, modeB, adaptor.getSpmatA(),
                 adaptor.getDnmatB(), adaptor.getDnmatC(), computeType,
                 pruneFlag, stream})
        .getResult();

    auto bufferSizePtr1 = LLVM::GEPOp::create(
        rewriter, loc, llvmPointerType, llvmPointerType, bufferSize,
        ValueRange{LLVM::ConstantOp::create(rewriter, loc, getIndexType(),
                                            rewriter.getIndexAttr(1))});
    auto bufferSizePtr2 = LLVM::GEPOp::create(
        rewriter, loc, llvmPointerType, llvmPointerType, bufferSize,
        ValueRange{LLVM::ConstantOp::create(rewriter, loc, getIndexType(),
                                            rewriter.getIndexAttr(2))});
    auto bufferSize0 =
        LLVM::LoadOp::create(rewriter, loc, llvmInt64Type, bufferSize);
    auto bufferSize1 =
        LLVM::LoadOp::create(rewriter, loc, llvmInt64Type, bufferSizePtr1);
    auto bufferSize2 =
        LLVM::LoadOp::create(rewriter, loc, llvmInt64Type, bufferSizePtr2);

    rewriter.replaceOp(op, {bufferSize0, bufferSize1, bufferSize2, stream});
  } else {
    auto computeType = genConstInt32From(
        rewriter, loc, getCuSparseDataTypeFrom(adaptor.getComputeType()));
    bufferSize =
        createSpMMBufferSizeCallBuilder
            .create(loc, rewriter,
                    {modeA, modeB, adaptor.getSpmatA(), adaptor.getDnmatB(),
                     adaptor.getDnmatC(), computeType, stream})
            .getResult();
    rewriter.replaceOp(op, {bufferSize, stream});
  }
  return success();
}

LogicalResult ConvertSDDMMBufferSizeOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::SDDMMBufferSizeOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto modeA = genConstInt32From(rewriter, loc, adaptor.getModeA());
  auto modeB = genConstInt32From(rewriter, loc, adaptor.getModeB());
  auto computeType = genConstInt32From(
      rewriter, loc, getCuSparseDataTypeFrom(adaptor.getComputeType()));
  auto stream = adaptor.getAsyncDependencies().front();
  auto bufferSize =
      createSDDMMBufferSizeCallBuilder
          .create(loc, rewriter,
                  {modeA, modeB, adaptor.getDnmatA(), adaptor.getDnmatB(),
                   adaptor.getSpmatC(), computeType, stream})
          .getResult();
  rewriter.replaceOp(op, {bufferSize, stream});
  return success();
}

LogicalResult ConvertSpMMOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::SpMMOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto modeA = genConstInt32From(rewriter, loc, adaptor.getModeA());
  auto modeB = genConstInt32From(rewriter, loc, adaptor.getModeB());
  auto computeType = genConstInt32From(
      rewriter, loc, getCuSparseDataTypeFrom(adaptor.getComputeType()));

  auto stream = adaptor.getAsyncDependencies().front();

  // Lower to cusparseLt if applicable
  if (is2To4Sparsity(op.getSpmatA())) {
    SmallVector<Value> pBufs;
    for (Value buffer : adaptor.getBuffers()) {
      Value pBuf = MemRefDescriptor(buffer).allocatedPtr(rewriter, loc);
      pBufs.push_back(pBuf);
    }
    createCuSparseLtSpMMBuilder.create(
        loc, rewriter,
        {adaptor.getSpmatA(), adaptor.getDnmatB(), adaptor.getDnmatC(),
         pBufs[0], pBufs[1], pBufs[2], stream});
  } else {
    Value pBuf = MemRefDescriptor(adaptor.getBuffers().front())
                     .allocatedPtr(rewriter, loc);
    createSpMMCallBuilder.create(loc, rewriter,
                                 {modeA, modeB, adaptor.getSpmatA(),
                                  adaptor.getDnmatB(), adaptor.getDnmatC(),
                                  computeType, pBuf, stream});
  }
  rewriter.replaceOp(op, {stream});
  return success();
}

template <typename T>
static void addOpaquePointerConversion(LLVMTypeConverter &converter) {
  converter.addConversion([&converter](T) -> Type {
    return LLVM::LLVMPointerType::get(&converter.getContext());
  });
}

LogicalResult ConvertSDDMMOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::SDDMMOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto computeType = genConstInt32From(
      rewriter, loc, getCuSparseDataTypeFrom(adaptor.getComputeType()));
  auto modeA = genConstInt32From(rewriter, loc, adaptor.getModeA());
  auto modeB = genConstInt32From(rewriter, loc, adaptor.getModeB());
  auto stream = adaptor.getAsyncDependencies().front();
  Value pBuf =
      MemRefDescriptor(adaptor.getBuffer()).allocatedPtr(rewriter, loc);
  createSDDMMCallBuilder.create(loc, rewriter,
                                {modeA, modeB, adaptor.getDnmatA(),
                                 adaptor.getDnmatB(), adaptor.getSpmatC(),
                                 computeType, pBuf, stream});
  rewriter.replaceOp(op, {stream});
  return success();
}

LogicalResult
ConvertSpGEMMCreateDescrOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::SpGEMMCreateDescrOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto stream = adaptor.getAsyncDependencies().front();
  Value descr = createSpGEMMCreateDescrBuilder.create(loc, rewriter, {stream})
                    .getResult();
  rewriter.replaceOp(op, {descr, stream});
  return success();
}

LogicalResult
ConvertSpGEMMDestroyDescrOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::SpGEMMDestroyDescrOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto stream = adaptor.getAsyncDependencies().front();
  createSpGEMMDestroyDescrBuilder.create(loc, rewriter,
                                         {adaptor.getDesc(), stream});
  rewriter.replaceOp(op, {stream});
  return success();
}

LogicalResult
ConvertSpGEMMWorkEstimationOrComputeOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::SpGEMMWorkEstimationOrComputeOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto computeType = genConstInt32From(
      rewriter, loc, getCuSparseDataTypeFrom(adaptor.getComputeType()));
  auto modeA = genConstInt32From(rewriter, loc, adaptor.getModeA());
  auto modeB = genConstInt32From(rewriter, loc, adaptor.getModeB());
  auto stream = adaptor.getAsyncDependencies().front();

  Value pBuf =
      MemRefDescriptor(adaptor.getBuffer()).allocatedPtr(rewriter, loc);
  Value bufferSizeNew;

  if (adaptor.getKind() ==
      gpu::SpGEMMWorkEstimationOrComputeKind::WORK_ESTIMATION) {
    bufferSizeNew =
        createSpGEMMWorkEstimationBuilder
            .create(loc, rewriter,
                    {adaptor.getDesc(), modeA, modeB, adaptor.getSpmatA(),
                     adaptor.getSpmatB(), adaptor.getSpmatC(), computeType,
                     adaptor.getBufferSz(), pBuf, stream})
            .getResult();
  } else {
    bufferSizeNew =
        createSpGEMMComputeBuilder
            .create(loc, rewriter,
                    {adaptor.getDesc(), modeA, modeB, adaptor.getSpmatA(),
                     adaptor.getSpmatB(), adaptor.getSpmatC(), computeType,
                     adaptor.getBufferSz(), pBuf, stream})
            .getResult();
  }
  rewriter.replaceOp(op, {bufferSizeNew, stream});
  return success();
}

LogicalResult ConvertSpGEMMCopyOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::SpGEMMCopyOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto computeType = genConstInt32From(
      rewriter, loc, getCuSparseDataTypeFrom(adaptor.getComputeType()));
  auto modeA = genConstInt32From(rewriter, loc, adaptor.getModeA());
  auto modeB = genConstInt32From(rewriter, loc, adaptor.getModeB());
  auto stream = adaptor.getAsyncDependencies().front();
  createSpGEMMCopyBuilder.create(loc, rewriter,
                                 {adaptor.getDesc(), modeA, modeB,
                                  adaptor.getSpmatA(), adaptor.getSpmatB(),
                                  adaptor.getSpmatC(), computeType, stream});
  rewriter.replaceOp(op, {stream});
  return success();
}

LogicalResult ConvertSpMatGetSizeOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::SpMatGetSizeOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto stream = adaptor.getAsyncDependencies().front();

  auto three = LLVM::ConstantOp::create(rewriter, loc, getIndexType(),
                                        rewriter.getIndexAttr(3));
  auto buffer = LLVM::AllocaOp::create(rewriter, loc, llvmPointerType,
                                       llvmInt64Type, three, /*alignment=*/16);

  auto rowsPtr = LLVM::GEPOp::create(
      rewriter, loc, llvmPointerType, llvmPointerType, buffer,
      ValueRange{LLVM::ConstantOp::create(rewriter, loc, getIndexType(),
                                          rewriter.getIndexAttr(0))});
  auto colsPtr = LLVM::GEPOp::create(
      rewriter, loc, llvmPointerType, llvmPointerType, buffer,
      ValueRange{LLVM::ConstantOp::create(rewriter, loc, getIndexType(),
                                          rewriter.getIndexAttr(1))});
  auto nnzsPtr = LLVM::GEPOp::create(
      rewriter, loc, llvmPointerType, llvmPointerType, buffer,
      ValueRange{LLVM::ConstantOp::create(rewriter, loc, getIndexType(),
                                          rewriter.getIndexAttr(2))});
  createSpMatGetSizeBuilder.create(
      loc, rewriter, {adaptor.getSpmat(), rowsPtr, colsPtr, nnzsPtr, stream});
  auto rows = LLVM::LoadOp::create(rewriter, loc, llvmInt64Type, rowsPtr);
  auto cols = LLVM::LoadOp::create(rewriter, loc, llvmInt64Type, colsPtr);
  auto nnzs = LLVM::LoadOp::create(rewriter, loc, llvmInt64Type, nnzsPtr);

  rewriter.replaceOp(op, {rows, cols, nnzs, stream});
  return success();
}

LogicalResult ConvertSetCsrPointersOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::SetCsrPointersOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto stream = adaptor.getAsyncDependencies().front();
  Value pPos =
      MemRefDescriptor(adaptor.getPositions()).allocatedPtr(rewriter, loc);
  Value pCrd =
      MemRefDescriptor(adaptor.getCoordinates()).allocatedPtr(rewriter, loc);
  Value pVal =
      MemRefDescriptor(adaptor.getValues()).allocatedPtr(rewriter, loc);
  createSetCsrPointersBuilder.create(
      loc, rewriter, {adaptor.getSpmat(), pPos, pCrd, pVal, stream});
  rewriter.replaceOp(op, {stream});
  return success();
}

LogicalResult ConvertCreateCscOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::CreateCscOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto stream = adaptor.getAsyncDependencies().front();
  Value pColPos =
      MemRefDescriptor(adaptor.getColPos()).allocatedPtr(rewriter, loc);
  Value pRowIdxs =
      MemRefDescriptor(adaptor.getRowIdxs()).allocatedPtr(rewriter, loc);
  Value pValues =
      MemRefDescriptor(adaptor.getValues()).allocatedPtr(rewriter, loc);
  Type pType =
      llvm::cast<MemRefType>(op.getColPos().getType()).getElementType();
  Type iType =
      llvm::cast<MemRefType>(op.getRowIdxs().getType()).getElementType();
  Type dType =
      llvm::cast<MemRefType>(op.getValues().getType()).getElementType();
  auto ptp = genConstInt32From(rewriter, loc, getCuSparseIndexTypeFrom(pType));
  auto itp = genConstInt32From(rewriter, loc, getCuSparseIndexTypeFrom(iType));
  auto dtp = genConstInt32From(rewriter, loc, getCuSparseDataTypeFrom(dType));
  auto handle =
      createCscCallBuilder
          .create(loc, rewriter,
                  {adaptor.getRows(), adaptor.getCols(), adaptor.getNnz(),
                   pColPos, pRowIdxs, pValues, ptp, itp, dtp, stream})
          .getResult();
  rewriter.replaceOp(op, {handle, stream});
  return success();
}

LogicalResult ConvertCreateBsrOpToGpuRuntimeCallPattern::matchAndRewrite(
    gpu::CreateBsrOp op, OpAdaptor adaptor,
    ConversionPatternRewriter &rewriter) const {
  if (failed(areAllLLVMTypes(op, adaptor.getOperands(), rewriter)) ||
      failed(isAsyncWithOneDependency(rewriter, op)))
    return failure();
  Location loc = op.getLoc();
  auto stream = adaptor.getAsyncDependencies().front();
  Value pRowPos =
      MemRefDescriptor(adaptor.getBRowPos()).allocatedPtr(rewriter, loc);
  Value pColIdxs =
      MemRefDescriptor(adaptor.getBColIdxs()).allocatedPtr(rewriter, loc);
  Value pValues =
      MemRefDescriptor(adaptor.getValues()).allocatedPtr(rewriter, loc);
  Type pType =
      llvm::cast<MemRefType>(op.getBRowPos().getType()).getElementType();
  Type iType =
      llvm::cast<MemRefType>(op.getBColIdxs().getType()).getElementType();
  Type dType =
      llvm::cast<MemRefType>(op.getValues().getType()).getElementType();
  auto ptp = genConstInt32From(rewriter, loc, getCuSparseIndexTypeFrom(pType));
  auto itp = genConstInt32From(rewriter, loc, getCuSparseIndexTypeFrom(iType));
  auto dtp = genConstInt32From(rewriter, loc, getCuSparseDataTypeFrom(dType));
  auto handle =
      createBsrCallBuilder
          .create(loc, rewriter,
                  {adaptor.getBrows(), adaptor.getBcols(), adaptor.getBnnz(),
                   adaptor.getRBlockSize(), adaptor.getCBlockSize(), pRowPos,
                   pColIdxs, pValues, ptp, itp, dtp, stream})
          .getResult();
  rewriter.replaceOp(op, {handle, stream});
  return success();
}

void mlir::populateGpuToLLVMConversionPatterns(
    LLVMTypeConverter &converter, RewritePatternSet &patterns,
    bool kernelBarePtrCallConv, bool kernelIntersperseSizeCallConv) {
  addOpaquePointerConversion<gpu::AsyncTokenType>(converter);
  addOpaquePointerConversion<gpu::SparseDnTensorHandleType>(converter);
  addOpaquePointerConversion<gpu::SparseSpMatHandleType>(converter);
  addOpaquePointerConversion<gpu::SparseSpGEMMOpHandleType>(converter);

  patterns.add<ConvertAllocOpToGpuRuntimeCallPattern,
               ConvertDeallocOpToGpuRuntimeCallPattern,
               ConvertHostRegisterOpToGpuRuntimeCallPattern,
               ConvertHostUnregisterOpToGpuRuntimeCallPattern,
               ConvertMemcpyOpToGpuRuntimeCallPattern,
               ConvertMemsetOpToGpuRuntimeCallPattern,
               ConvertSetDefaultDeviceOpToGpuRuntimeCallPattern,
               ConvertWaitAsyncOpToGpuRuntimeCallPattern,
               ConvertWaitOpToGpuRuntimeCallPattern,
               ConvertAsyncYieldToGpuRuntimeCallPattern,
               ConvertCreateDnTensorOpToGpuRuntimeCallPattern,
               ConvertDestroyDnTensorOpToGpuRuntimeCallPattern,
               ConvertCreateCooOpToGpuRuntimeCallPattern,
               ConvertCreateCooAoSOpToGpuRuntimeCallPattern,
               ConvertCreateCsrOpToGpuRuntimeCallPattern,
               ConvertCreateCscOpToGpuRuntimeCallPattern,
               ConvertCreateBsrOpToGpuRuntimeCallPattern,
               ConvertCreate2To4SpMatOpToGpuRuntimeCallPattern,
               ConvertDestroySpMatOpToGpuRuntimeCallPattern,
               ConvertSpMVBufferSizeOpToGpuRuntimeCallPattern,
               ConvertSpMVOpToGpuRuntimeCallPattern,
               ConvertSpMMBufferSizeOpToGpuRuntimeCallPattern,
               ConvertSDDMMBufferSizeOpToGpuRuntimeCallPattern,
               ConvertSpMMOpToGpuRuntimeCallPattern,
               ConvertSDDMMOpToGpuRuntimeCallPattern,
               ConvertSpGEMMCreateDescrOpToGpuRuntimeCallPattern,
               ConvertSpGEMMDestroyDescrOpToGpuRuntimeCallPattern,
               ConvertSpGEMMWorkEstimationOrComputeOpToGpuRuntimeCallPattern,
               ConvertSpGEMMCopyOpToGpuRuntimeCallPattern,
               ConvertSpMatGetSizeOpToGpuRuntimeCallPattern,
               ConvertSetCsrPointersOpToGpuRuntimeCallPattern>(converter);
  patterns.add<LegalizeLaunchFuncOpPattern>(converter, kernelBarePtrCallConv,
                                            kernelIntersperseSizeCallConv);
}

//===----------------------------------------------------------------------===//
// GPUModuleOp convert to LLVM op interface
//===----------------------------------------------------------------------===//

namespace {
struct GPUModuleOpConvertToLLVMInterface
    : public ConvertToLLVMOpInterface::ExternalModel<
          GPUModuleOpConvertToLLVMInterface, gpu::GPUModuleOp> {
  /// Get the conversion patterns from the target attribute.
  void getConvertToLLVMConversionAttrs(
      Operation *op, SmallVectorImpl<ConvertToLLVMAttrInterface> &attrs) const;
};
} // namespace

void GPUModuleOpConvertToLLVMInterface::getConvertToLLVMConversionAttrs(
    Operation *op, SmallVectorImpl<ConvertToLLVMAttrInterface> &attrs) const {
  auto module = cast<gpu::GPUModuleOp>(op);
  ArrayAttr targetsAttr = module.getTargetsAttr();
  // Fail if there are no target attributes or there is more than one target.
  if (!targetsAttr || targetsAttr.size() != 1)
    return;
  if (auto patternAttr = dyn_cast<ConvertToLLVMAttrInterface>(targetsAttr[0]))
    attrs.push_back(patternAttr);
}

void mlir::gpu::registerConvertGpuToLLVMInterface(DialectRegistry &registry) {
  registry.addExtension(+[](MLIRContext *ctx, gpu::GPUDialect *dialect) {
    gpu::GPUModuleOp::attachInterface<GPUModuleOpConvertToLLVMInterface>(*ctx);
  });
}
