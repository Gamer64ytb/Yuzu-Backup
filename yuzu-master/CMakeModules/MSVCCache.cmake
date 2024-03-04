# SPDX-FileCopyrightText: 2022 yuzu Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# buildcache wrapper
OPTION(USE_CCACHE "Use buildcache for compilation" OFF)
IF(USE_CCACHE)
    FIND_PROGRAM(CCACHE buildcache)
    IF (CCACHE)
        MESSAGE(STATUS "Using buildcache found in PATH")
        SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE})
        SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE})
    ELSE(CCACHE)
        MESSAGE(WARNING "USE_CCACHE enabled, but no buildcache executable found")
    ENDIF(CCACHE)
ENDIF(USE_CCACHE)
