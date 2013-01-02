LOCAL_PATH := $(call my-dir)

KLAYGE_PLUGIN_OCTREE_PATH := $(LOCAL_PATH)
KLAYGE_PLUGIN_OCTREE_SRC_PATH := $(LOCAL_PATH)/../../../../../../Plugins/Src/Scene/OCTree

include $(CLEAR_VARS)

LOCAL_C_INCLUDES := $(KLAYGE_PLUGIN_OCTREE_SRC_PATH)/../../../../../External/boost \
		$(KLAYGE_PLUGIN_OCTREE_SRC_PATH)/../../../../../KFL/include \
		$(KLAYGE_PLUGIN_OCTREE_SRC_PATH)/../../../../Core/Include \
		$(KLAYGE_PLUGIN_OCTREE_SRC_PATH)/../../../Include \
		$(KLAYGE_PLUGIN_OCTREE_SRC_PATH)/../../../../../External/android_native_app_glue
		
LOCAL_MODULE := KlayGE_Scene_OCTree
LOCAL_PATH := $(KLAYGE_PLUGIN_OCTREE_SRC_PATH)
LOCAL_SRC_FILES := \
		OCTree.cpp \
		OCTreeFactory.cpp \

		
LOCAL_CFLAGS := -DKLAYGE_BUILD_DLL -DKLAYGE_OCTREE_SM_SOURCE

include $(BUILD_STATIC_LIBRARY)
