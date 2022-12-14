##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions
## are met:
##  * Redistributions of source code must retain the above copyright
##    notice, this list of conditions and the following disclaimer.
##  * Redistributions in binary form must reproduce the above copyright
##    notice, this list of conditions and the following disclaimer in the
##    documentation and/or other materials provided with the distribution.
##  * Neither the name of NVIDIA CORPORATION nor the names of its
##    contributors may be used to endorse or promote products derived
##    from this software without specific prior written permission.
##
## THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
## EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
## IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
## PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
## CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
## EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
## PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
## PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
## OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
## OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##
## Copyright (c) 2008-2021 NVIDIA Corporation. All rights reserved.

IF(NOT DEFINED PHYSX_LINUX_COMPILE_DEFS)
	MESSAGE(FATAL ERROR "Samples uses the PhysX compile defs, and they're not defined when they need to be.")
ENDIF()

IF (NOT DEFINED PHYSX_CXX_FLAGS)
	MESSAGE(FATAL ERROR "Samples uses the PhysX CXX flags, and they're not defined when they need to be.")
ENDIF()

SET(SAMPLES_ADDITIONAL_WARNINGS "-Wno-missing-field-initializers -Wno-macro-redefined -Wno-unused-parameter -Wno-header-hygiene -Wno-double-promotion -Wno-format-pedantic -Wno-switch-enum -Wno-unused-variable -Wno-sign-compare -Wno-shadow -Wno-sometimes-uninitialized -Wno-conditional-uninitialized -Wno-vla -Wno-vla-extension -Wno-reorder -Wno-duplicate-enum -Wno-unknown-pragmas -Wno-shadow-field -Wno-comma -Wno-cast-qual -Wno-null-pointer-arithmetic")

# Get the CXX Flags from the Cached variables set by the PhysX CMakeLists
SET(CMAKE_CXX_FLAGS "${PHYSX_CXX_FLAGS} ${SAMPLES_ADDITIONAL_WARNINGS}")

SET(CMAKE_CXX_FLAGS_DEBUG   ${PHYSX_CXX_FLAGS_DEBUG})
SET(CMAKE_CXX_FLAGS_CHECKED ${PHYSX_CXX_FLAGS_CHECKED})
SET(CMAKE_CXX_FLAGS_PROFILE ${PHYSX_CXX_FLAGS_PROFILE})
SET(CMAKE_CXX_FLAGS_RELEASE ${PHYSX_CXX_FLAGS_RELEASE})

SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath='$ORIGIN'")

#Include projects
INCLUDE(SampleBase.cmake)
INCLUDE(SampleFramework.cmake)
INCLUDE(SampleRenderer.cmake)
INCLUDE(SamplePlatform.cmake)
INCLUDE(SamplesToolkit.cmake)
SET_PROPERTY(TARGET SampleToolkit PROPERTY FOLDER "PhysX Samples/Libraries only")
SET_PROPERTY(TARGET SampleBase PROPERTY FOLDER "PhysX Samples/Libraries only")
SET_PROPERTY(TARGET SampleFramework PROPERTY FOLDER "PhysX Samples/Libraries only")
SET_PROPERTY(TARGET SamplePlatform PROPERTY FOLDER "PhysX Samples/Libraries only")
SET_PROPERTY(TARGET SampleRenderer PROPERTY FOLDER "PhysX Samples/Libraries only")

IF(PX_BUILDSAMPLES)
	INCLUDE(Samples.cmake)
	SET_PROPERTY(TARGET Samples PROPERTY FOLDER "PhysX Samples")
ENDIF()

IF(PX_BUILDPUBLICSAMPLES)
	INCLUDE(Samples.cmake)
	SET_PROPERTY(TARGET Samples PROPERTY FOLDER "PhysX Samples")
ENDIF()