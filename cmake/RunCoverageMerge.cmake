file(GLOB ZEP_PROFRAW_FILES "${ZEP_COVERAGE_DIR}/*.profraw")

if(NOT ZEP_PROFRAW_FILES)
    message(FATAL_ERROR "no coverage profiles found in ${ZEP_COVERAGE_DIR}")
endif()

execute_process(
    COMMAND "${ZEP_LLVM_PROFDATA}" merge -sparse ${ZEP_PROFRAW_FILES} -o "${ZEP_PROFDATA}"
    RESULT_VARIABLE ZEP_PROFDATA_RESULT
)

if(NOT ZEP_PROFDATA_RESULT EQUAL 0)
    message(FATAL_ERROR "llvm-profdata merge failed")
endif()
