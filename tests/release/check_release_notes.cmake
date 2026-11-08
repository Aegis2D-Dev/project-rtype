# Checks tools/release_notes.cmake on a changelog of 4 pull requests: since a release that
# listed 2 of them, the notes hold the other 2, newest first; for a first release, all 4.
# Variables: SOURCE_DIR, the game's sources; OUTPUT, a scratch directory.
set(here "${SOURCE_DIR}/tests/release")
file(MAKE_DIRECTORY "${OUTPUT}")

function(notes previous result)
    file(REMOVE "${OUTPUT}/notes.md")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DCHANGELOG=${here}/current.md"
            "-DPREVIOUS=${previous}" "-DBASE_URL=https://github.com/o/r/blob/v1/"
            "-DOUTPUT=${OUTPUT}/notes.md" -P "${SOURCE_DIR}/tools/release_notes.cmake"
        RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "release_notes.cmake failed with previous '${previous}'")
    endif()
    file(READ "${OUTPUT}/notes.md" text)
    set(${result} "${text}" PARENT_SCOPE)
endfunction()

set(fourth "- [#4](https://github.com/o/r/pull/4) Fourth change, stacked ([details](https://github.com/o/r/blob/v1/changelogs/2026-11-02-fourth.md))\n")
set(third "- [#3](https://github.com/o/r/pull/3) Third change ([details](https://github.com/o/r/blob/v1/changelogs/2026-11-01-third.md))\n")
set(second "- [#2](https://github.com/o/r/pull/2) Second change ([details](https://github.com/o/r/blob/v1/changelogs/2026-10-02-second.md))\n")
set(first "- [#1](https://github.com/o/r/pull/1) First change ([details](https://github.com/o/r/blob/v1/changelogs/2026-10-01-first.md))\n")

notes("${here}/previous.md" since)
if(NOT since STREQUAL "## Pull requests\n\n${fourth}${third}")
    message(FATAL_ERROR "notes since the previous release:\n${since}")
endif()
notes("" all)
if(NOT all STREQUAL "## Pull requests\n\n${fourth}${third}${second}${first}")
    message(FATAL_ERROR "notes of a first release:\n${all}")
endif()

# A changelog with nothing new since the previous release stops the release.
execute_process(COMMAND "${CMAKE_COMMAND}" "-DCHANGELOG=${here}/current.md"
        "-DPREVIOUS=${here}/current.md" "-DBASE_URL=x/" "-DOUTPUT=${OUTPUT}/empty.md"
        -P "${SOURCE_DIR}/tools/release_notes.cmake"
    RESULT_VARIABLE status OUTPUT_QUIET ERROR_QUIET)
if(status EQUAL 0)
    message(FATAL_ERROR "release_notes.cmake accepted a release with no new pull request")
endif()
message(STATUS "release notes: 2 pull requests since the previous release, 4 for the first")
