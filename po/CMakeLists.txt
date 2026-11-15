include(FindGettext)
find_program(GETTEXT_XGETTEXT_EXECUTABLE xgettext REQUIRED)
find_program(GETTEXT_MSGFMT_EXECUTABLE msgfmt REQUIRED)

set(DOMAIN batteryinspector.sprainbrains)
set(POT_NAME ${DOMAIN}.pot)
set(POT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${POT_NAME})

file(GLOB_RECURSE QML_SOURCES RELATIVE ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/qml/*.qml)

add_custom_target(${POT_NAME} ALL
    COMMAND ${GETTEXT_XGETTEXT_EXECUTABLE}
        --from-code=UTF-8
        --language=JavaScript
        --add-comments=TRANSLATORS
        --keyword=tr
        --keyword=tr:1,2
        --keyword=i18n.tr
        --keyword=i18n.tr:1,2
        --keyword=_
        --package-name=${DOMAIN}
        --sort-by-file
        -o ${POT_FILE}
        ${QML_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating ${POT_NAME}"
    VERBATIM)

file(GLOB PO_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.po)
foreach(PO_FILE ${PO_FILES})
    get_filename_component(LANG ${PO_FILE} NAME_WE)
    set(GMO ${CMAKE_CURRENT_BINARY_DIR}/${LANG}.gmo)
    add_custom_command(OUTPUT ${GMO}
        COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -c -o ${GMO} ${PO_FILE}
        DEPENDS ${PO_FILE}
        COMMENT "Compiling ${LANG} translation")
    add_custom_target(${LANG}_mo ALL DEPENDS ${GMO})
    install(FILES ${GMO}
            DESTINATION share/locale/${LANG}/LC_MESSAGES
            RENAME ${DOMAIN}.mo)
endforeach()
