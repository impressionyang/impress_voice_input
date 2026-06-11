#pragma once
/**
 * @brief ONNX Runtime C API Shim
 *
 * 包装 onnxruntime_c_api.h 以解决 MinGW 交叉编译兼容性问题：
 * 1. SAL 注解（specstrings.h 中的宏与 MinGW 不兼容）
 * 2. _stdcall 调用约定导致函数声明语法错误
 *
 * 关键：必须在包含 onnxruntime_c_api.h 之前定义这些宏，
 * 因为 header 内部的 #define 会使用它们。
 */

#ifdef HAVE_ONNXRUNTIME
#ifndef ORT_API_SHIM_H
#define ORT_API_SHIM_H

#ifdef _WIN32
#define ORT_DLL_IMPORT

/* 在 specstrings.h 被包含之前，抢先定义 SAL 注解为空。
   onnxruntime_c_api.h 第 74 行 #include <specstrings.h>，
   如果 specstrings.h 用 #ifndef 保护，我们的定义就不会被覆盖。
   即使被覆盖，_stdcall 下面的定义也会生效。 */
#define _Success_(x)
#define _Check_return_
#define _Ret_maybenull_
#define _In_
#define _In_z_
#define _In_opt_
#define _In_opt_z_
#define _Out_
#define _Outptr_
#define _Out_opt_
#define _Inout_
#define _Inout_opt_
#define _Frees_ptr_opt_
#define _Ret_notnull_
#define _In_reads_(x)
#define _Inout_updates_(x)
#define _Out_writes_(x)
#define _Inout_updates_all_(x)
#define _Out_writes_bytes_all_(x)
#define _Out_writes_all_(x)
#define _Outptr_result_maybenull_(x)
#define _In_reads_opt_(x)
#define _Outptr_result_buffer_maybenull_(x)
#define _Return_type_success_(x)
#define _Out_writes_bytes_all_opt_(x)
#define _In_reads_bytes_(x)

/* 将 _stdcall 定义为空。
   onnxruntime_c_api.h 第 86 行 #define ORT_API_CALL _stdcall，
   所以当 ORT_API_CALL 展开为 _stdcall 后，_stdcall 再展开为空。
   这样最终的函数声明没有调用约定修饰，MinGW 可以正常解析。 */
#define _stdcall
#define __stdcall
#endif /* _WIN32 */

#include <onnxruntime_c_api.h>

#endif /* ORT_API_SHIM_H */
#endif /* HAVE_ONNXRUNTIME */
