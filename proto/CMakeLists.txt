include_directories(${Protobuf_INCLUDE_DIRS})
PROTOBUF_GENERATE_CPP(ProtoSources ProtoHeaders nes.proto)
add_library(proto ${ProtoSources} ${ProtoSources})
target_link_libraries(proto INTERFACE ${Protobuf_LIBRARIES})
