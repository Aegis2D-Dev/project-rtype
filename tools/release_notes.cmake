# Writes the notes of a release from the changelog: one line for each pull request of
# CHANGELOG.md that the previous release's CHANGELOG.md did not list, newest first, with a
# link to its changelog file.
# Variables: CHANGELOG, the changelog of the release; PREVIOUS, the previous release's (empty
# or missing for the first release); BASE_URL, where the changelog's relative links point
# (https://github.com/<owner>/<repo>/blob/<tag>/); OUTPUT, the file the notes are appended to.
set(row_regex "^\\| \\[#([0-9]+)\\]\\(([^)]+)\\) \\| ([^|]+) \\|.*\\[[^]]+\\]\\(([^)]+)\\) \\|$")

set(known "")
if(PREVIOUS AND EXISTS "${PREVIOUS}")
    file(STRINGS "${PREVIOUS}" previous_rows ENCODING UTF-8 REGEX "^\\| \\[#[0-9]+\\]")
    foreach(row IN LISTS previous_rows)
        string(REGEX REPLACE "^\\| \\[#([0-9]+)\\].*" "\\1" number "${row}")
        list(APPEND known ${number})
    endforeach()
endif()

file(STRINGS "${CHANGELOG}" rows ENCODING UTF-8 REGEX "^\\| \\[#[0-9]+\\]")
set(lines "")
set(count 0)
foreach(row IN LISTS rows)
    if(NOT row MATCHES "${row_regex}")
        message(FATAL_ERROR "unexpected changelog row: ${row}")
    endif()
    if(NOT CMAKE_MATCH_1 IN_LIST known)
        string(STRIP "${CMAKE_MATCH_3}" title)
        string(APPEND lines "- [#${CMAKE_MATCH_1}](${CMAKE_MATCH_2}) ${title} "
            "([details](${BASE_URL}${CMAKE_MATCH_4}))\n")
        math(EXPR count "${count} + 1")
    endif()
endforeach()
if(count EQUAL 0)
    message(FATAL_ERROR "${CHANGELOG} lists no pull request since the previous release")
endif()

file(APPEND "${OUTPUT}" "## Pull requests\n\n${lines}")
message(STATUS "${count} pull requests in the release notes")
