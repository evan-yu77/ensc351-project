# Build the app, using the HAL

include_directories(include)
file(GLOB MY_SOURCES "src/*.c")
add_executable(proj351 ${MY_SOURCES})

# Make use of the HAL library
target_link_libraries(proj351 LINK_PRIVATE hal)

# Copy executable to final location (change `proj351` to project name as needed)
add_custom_command(TARGET proj351 POST_BUILD 
  COMMAND "${CMAKE_COMMAND}" -E copy 
     "$<TARGET_FILE:proj351>"
     "~/ensc351/public/proj351" 
  COMMENT "Copying ARM executable to public NFS directory")
