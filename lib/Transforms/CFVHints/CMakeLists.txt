# If we don't need RTTI or EH, there's no reason to export anything
# from the hello plugin.
if( NOT LLVM_REQUIRES_RTTI )
  if( NOT LLVM_REQUIRES_EH )
    set(LLVM_EXPORTED_SYMBOL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/CollectLoopHints.exports)
  endif()
endif()

if(WIN32 OR CYGWIN)
  set(LLVM_LINK_COMPONENTS Core Support)
endif()

# Add source files to the target
add_library( LLVMCollectCFVHints
  CollectLoopHints.cpp
  CollectCondBranchHints.cpp
  CollectICallHints.cpp
  CollectIBranchHints.cpp
)

# Specify dependencies using add_dependencies
add_dependencies(LLVMCollectCFVHints
  intrinsics_gen
  PLUGIN_TOOL
  opt
)
