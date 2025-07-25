; RUN: llc -mtriple=amdgcn < %s | FileCheck -check-prefix=FUNC %s
; RUN: llc -mtriple=amdgcn-amdhsa -mcpu=kaveri < %s | FileCheck -check-prefix=FUNC %s
; RUN: llc -mtriple=amdgcn -mcpu=tonga -mattr=-flat-for-global < %s | FileCheck -check-prefix=FUNC %s
; RUN: llc -mtriple=r600 -mcpu=redwood < %s | FileCheck --check-prefixes=EG,FUNC %s

; FIXME: This seems to not ever actually become an extload
; FUNC-LABEL: {{^}}global_anyext_load_i8:
; GCN: buffer_load_dword v{{[0-9]+}}
; GCN: buffer_store_dword v{{[0-9]+}}

; EG: MEM_RAT_CACHELESS STORE_RAW [[VAL:T[0-9]+.[XYZW]]],
; EG: VTX_READ_32 [[VAL]]
define amdgpu_kernel void @global_anyext_load_i8(ptr addrspace(1) nocapture noalias %out, ptr addrspace(1) nocapture noalias %src) nounwind {
  %load = load i32, ptr addrspace(1) %src
  %x = bitcast i32 %load to <4 x i8>
  store <4 x i8> %x, ptr addrspace(1) %out
  ret void
}

; FUNC-LABEL: {{^}}global_anyext_load_i16:
; GCN: buffer_load_dword v{{[0-9]+}}
; GCN: buffer_store_dword v{{[0-9]+}}

; EG: MEM_RAT_CACHELESS STORE_RAW [[VAL:T[0-9]+.[XYZW]]],
; EG: VTX_READ_32 [[VAL]]
define amdgpu_kernel void @global_anyext_load_i16(ptr addrspace(1) nocapture noalias %out, ptr addrspace(1) nocapture noalias %src) nounwind {
  %load = load i32, ptr addrspace(1) %src
  %x = bitcast i32 %load to <2 x i16>
  store <2 x i16> %x, ptr addrspace(1) %out
  ret void
}

; FUNC-LABEL: {{^}}local_anyext_load_i8:
; GCN: ds_read_b32 v{{[0-9]+}}
; GCN: ds_write_b32 v{{[0-9]+}}

; EG: LDS_READ_RET {{.*}}, [[VAL:T[0-9]+.[XYZW]]]
; EG: LDS_WRITE * [[VAL]]
define amdgpu_kernel void @local_anyext_load_i8(ptr addrspace(3) nocapture noalias %out, ptr addrspace(3) nocapture noalias %src) nounwind {
  %load = load i32, ptr addrspace(3) %src
  %x = bitcast i32 %load to <4 x i8>
  store <4 x i8> %x, ptr addrspace(3) %out
  ret void
}

; FUNC-LABEL: {{^}}local_anyext_load_i16:
; GCN: ds_read_b32 v{{[0-9]+}}
; GCN: ds_write_b32 v{{[0-9]+}}

; EG: LDS_READ_RET {{.*}}, [[VAL:T[0-9]+.[XYZW]]]
; EG: LDS_WRITE * [[VAL]]
define amdgpu_kernel void @local_anyext_load_i16(ptr addrspace(3) nocapture noalias %out, ptr addrspace(3) nocapture noalias %src) nounwind {
  %load = load i32, ptr addrspace(3) %src
  %x = bitcast i32 %load to <2 x i16>
  store <2 x i16> %x, ptr addrspace(3) %out
  ret void
}
