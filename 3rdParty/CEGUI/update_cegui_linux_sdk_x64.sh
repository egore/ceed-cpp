#!/bin/sh
export src_lib_folder=../../../cegui/build/lib

# CEGUI Includes
rm -rf include
mkdir include
cp -a ../../../cegui/cegui/include .
cp ../../../cegui/build/cegui/include/CEGUI/*.h include/CEGUI

# CEGUI DLLs
rm -rf ./bin
mkdir -p ./bin/release
cp -a ${src_lib_folder}/libCEGUIBase-9999.so* ./bin/release/
cp -a ${src_lib_folder}/libCEGUICommonDialogs-9999.so* ./bin/release/
cp -a ${src_lib_folder}/libCEGUICoreWindowRendererSet.so* ./bin/release/
cp -a ${src_lib_folder}/libCEGUIPugiXMLParser.so* ./bin/release/
cp -a ${src_lib_folder}/libCEGUIOpenGLRenderer-9999.so* ./bin/release/
cp -a ${src_lib_folder}/libCEGUIFreeImageImageCodec.so* ./bin/release/

mkdir -p ./bin/debug
cp -a ${src_lib_folder}/libCEGUIBase-9999.so* ./bin/debug/
cp -a ${src_lib_folder}/libCEGUICommonDialogs-9999.so* ./bin/debug/
cp -a ${src_lib_folder}/libCEGUICoreWindowRendererSet.so* ./bin/debug/
cp -a ${src_lib_folder}/libCEGUIPugiXMLParser.so* ./bin/debug/
cp -a ${src_lib_folder}/libCEGUIOpenGLRenderer-9999.so* ./bin/debug/
cp -a ${src_lib_folder}/libCEGUIFreeImageImageCodec.so* ./bin/debug/