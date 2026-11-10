find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

function(add_Resetter_test target)
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} PRIVATE Resetter_core Qt6::Test)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        TIMEOUT 120)
endfunction()

add_Resetter_test(test_checksum)
add_Resetter_test(test_dump)
add_Resetter_test(test_profile)
add_Resetter_test(test_validation)
add_Resetter_test(test_protocol)

# GUI-тест: hex-редактор требует QtWidgets
add_executable(test_hexeditor test_hexeditor.cpp ${CMAKE_SOURCE_DIR}/src/gui/HexEditor.cpp)
target_link_libraries(test_hexeditor PRIVATE Resetter_core Qt6::Widgets Qt6::Test)
add_test(NAME test_hexeditor COMMAND test_hexeditor)
set_tests_properties(test_hexeditor PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 120)

# Каталог профилей известен на этапе сборки — используется в тестах
target_compile_definitions(test_profile PRIVATE
    RC_PROFILES_DIR="${CMAKE_SOURCE_DIR}/src/profiles")
