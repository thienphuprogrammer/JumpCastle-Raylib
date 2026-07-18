# Deletion-recovery test: remove a synced runtime atlas, rebuild only the
# jumpcastle_assets target, and require the file to return with a matching hash
# even though the jumpcastle executable itself is already up to date.
# Driven via `cmake -P` with BINARY_DIR, ASSET_DIR, SOURCE_ASSETS, CONFIG set.
set(target_file "${ASSET_DIR}/generated/castle.png")
set(source_file "${SOURCE_ASSETS}/generated/castle.png")

file(REMOVE "${target_file}")
if(EXISTS "${target_file}")
  message(FATAL_ERROR "could not delete ${target_file} before recovery")
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} --build "${BINARY_DIR}" --target jumpcastle_assets --config "${CONFIG}"
  RESULT_VARIABLE build_result
  OUTPUT_VARIABLE build_output
  ERROR_VARIABLE build_output)
if(NOT build_result EQUAL 0)
  message(FATAL_ERROR "jumpcastle_assets rebuild failed:\n${build_output}")
endif()

if(NOT EXISTS "${target_file}")
  message(FATAL_ERROR "castle.png was not restored by jumpcastle_assets")
endif()

file(SHA256 "${target_file}" restored_hash)
file(SHA256 "${source_file}" source_hash)
if(NOT restored_hash STREQUAL source_hash)
  message(FATAL_ERROR "restored castle.png hash ${restored_hash} != source ${source_hash}")
endif()

message(STATUS "asset recovery verified: castle.png restored with matching hash")
