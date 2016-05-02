LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

# measurements show that the ARM version of ZLib is about x1.17 faster
# than the thumb one...
LOCAL_ARM_MODE := arm

zlib_files := \
	src/adler32.c \
	src/compress.c \
	src/crc32.c \
	src/deflate.c \
	src/gzclose.c \
	src/gzlib.c \
	src/gzread.c \
	src/gzwrite.c \
	src/infback.c \
	src/inflate.c \
	src/inftrees.c \
	src/inffast.c \
	src/trees.c \
	src/uncompr.c \
	src/zutil.c

LOCAL_MODULE := libz
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -O3 -DUSE_MMAP

# FIXME: ask keith about this
ifneq ($(HOST_OS),darwin)
LOCAL_CLANG := false
endif

# TODO: This is to work around b/19059885. Remove after root cause is fixed
LOCAL_LDFLAGS_arm := -Wl,--hash-style=both

LOCAL_SRC_FILES := $(zlib_files)
ifneq ($(TARGET_BUILD_APPS),)
  LOCAL_SDK_VERSION := 9
endif
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libz
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -O3 -DUSE_MMAP
LOCAL_SRC_FILES := $(zlib_files)
ifneq ($(TARGET_BUILD_APPS),)
  LOCAL_SDK_VERSION := 9
endif
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
ifneq ($(HOST_OS),darwin)
LOCAL_CLANG := false
endif

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libz
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -O3 -DUSE_MMAP
LOCAL_SRC_FILES := $(zlib_files)
LOCAL_MULTILIB := both
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
LOCAL_CLANG := false

include $(BUILD_HOST_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libz-host
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -O3 -DUSE_MMAP
LOCAL_SRC_FILES := $(zlib_files)
LOCAL_MULTILIB := both
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)

ifneq ($(HOST_OS),darwin)
LOCAL_CLANG := false
endif

include $(BUILD_HOST_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=        \
	src/test/minigzip.c

LOCAL_MODULE:= gzip

LOCAL_SHARED_LIBRARIES := libz

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=        \
	src/test/minigzip.c

LOCAL_MODULE:= minigzip

LOCAL_STATIC_LIBRARIES := libz
ifneq ($(HOST_OS),darwin)
LOCAL_CLANG := false
endif

include $(BUILD_HOST_EXECUTABLE)
