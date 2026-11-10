#! enable_language works only at file scope, so the app includes this file in its top-level CMakeLists.txt.
include_guard(GLOBAL)

if(WIN32)
    enable_language(RC)
else()
    enable_language(ASM)
endif()

#! RmlKit's resource reader includes the index, so it compiles only in an app that embeds its resources.
function(rmlkit_add_resources target)
    set(paths "")
    foreach(argument IN LISTS ARGN)
        if(argument STREQUAL "ROOT")
            set(expect_root TRUE)
        elseif(expect_root)
            set(root "${argument}")
            set(expect_root FALSE)
        else()
            if(NOT DEFINED root)
                message(FATAL_ERROR "rmlkit_add_resources: ${argument} comes before the first ROOT")
            endif()
            file(RELATIVE_PATH path "${root}" "${argument}")
            if(argument MATCHES "[\"\\\\]")
                message(FATAL_ERROR "rmlkit_add_resources: ${argument} contains a quote or backslash")
            endif()
            if(DEFINED file_of_${path})
                message(FATAL_ERROR "rmlkit_add_resources: ${path} comes from ${file_of_${path}} and ${argument}")
            endif()
            set(file_of_${path} "${argument}")
            list(APPEND paths "${path}")
        endif()
    endforeach()
    #! The index must stay sorted: EmbeddedResources finds a file with a binary search.
    list(SORT paths)

    set(index "")
    set(table "")
    set(data "")
    set(source_files "")
    set(id 0)
    foreach(path IN LISTS paths)
        set(file "${file_of_${path}}")
        list(APPEND source_files "${file}")
        string(APPEND index "{\"${path}\", ${id}},\n")
        if(WIN32)
            math(EXPR resource_id "${id} + 1")
            string(APPEND data "${resource_id} RCDATA \"${file}\"\n")
        else()
            string(APPEND table "    .quad resource_${id}, resource_${id}_end\n")
            string(APPEND data "    .balign 16\nresource_${id}:\n    .incbin \"${file}\"\nresource_${id}_end:\n")
        endif()
        math(EXPR id "${id} + 1")
    endforeach()

    set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}")
    file(CONFIGURE OUTPUT "${output_dir}/embedded_resource_index.inc" CONTENT "${index}" @ONLY)
    if(WIN32)
        set(data_source "${output_dir}/embedded_resources.rc")
        file(CONFIGURE OUTPUT "${data_source}" CONTENT "${data}" @ONLY)
    else()
        set(data_source "${output_dir}/embedded_resources.S")
        file(CONFIGURE OUTPUT "${data_source}" CONTENT "\
    .section .data.rel.ro.resources,\"aw\"
    .balign 8
    .globl resource_table
    .hidden resource_table
resource_table:
${table}
    .section .rodata.resources,\"a\"
${data}
    .section .note.GNU-stack,\"\",@progbits
" @ONLY)
    endif()

    add_library(${target} OBJECT "${data_source}")
    set_source_files_properties("${data_source}" PROPERTIES OBJECT_DEPENDS "${source_files}")
    target_include_directories(${target} INTERFACE "${output_dir}")
    target_sources(RmlKit PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../src/RmlKit/UI/Backend/EmbeddedResourcesDesktop.cpp")
    target_include_directories(RmlKit PRIVATE "${output_dir}")
    target_link_libraries(RmlKit INTERFACE "$<TARGET_OBJECTS:${target}>")
endfunction()
