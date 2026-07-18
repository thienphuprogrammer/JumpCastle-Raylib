# Copy an explicit list of runtime assets into the executable-adjacent tree,
# touching only files whose contents differ. Invoked via `cmake -P` by the
# jumpcastle_assets target with SOURCE_ROOT, DESTINATION_ROOT and FILES set.
foreach(relative_path IN LISTS FILES)
  get_filename_component(relative_dir "${relative_path}" DIRECTORY)
  file(MAKE_DIRECTORY "${DESTINATION_ROOT}/${relative_dir}")
  file(COPY_FILE
       "${SOURCE_ROOT}/${relative_path}"
       "${DESTINATION_ROOT}/${relative_path}"
       ONLY_IF_DIFFERENT)
endforeach()
