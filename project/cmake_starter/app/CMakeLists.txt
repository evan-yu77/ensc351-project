# Build the app, using the HAL

include_directories(include)
file(GLOB MY_SOURCES "src/*.c")
add_executable(spiTestNew ${MY_SOURCES})

# Make use of the HAL library
target_link_libraries(spiTestNew LINK_PRIVATE hal)

# Copy executable to final location (change `spiTestNew` to project name as needed)
add_custom_command(TARGET spiTestNew POST_BUILD 
  COMMAND "${CMAKE_COMMAND}" -E copy 
     "$<TARGET_FILE:spiTestNew>"
     "~/ensc351/public/spiTestNew" 
  COMMENT "Copying ARM executable to public NFS directory")
