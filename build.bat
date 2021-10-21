@echo off

rem so that we always execute in the same directory every time.
cd %~dp0

if not exist bin mkdir bin
if not exist bin\obj mkdir bin\obj

set NAME=Blind
set code=win32_Blind.cpp
set objs=win32_Blind.obj
set libs=

if "%1"=="release" (
    set copts=-IC:\dev\include\ -O2 -MT -WX 
    set lopts=-OUT:..\%NAME%.exe -LIBPATH:C:\dev\lib\win64\ -SUBSYSTEM:WINDOWS
) else (
		set copts=-IC:\dev\include\ -Od -Zi -MDd -DDEBUG	
    set lopts=-OUT:..\%NAME%.exe -LIBPATH:C:\dev\lib\win64\ -SUBSYSTEM:WINDOWS -DEBUG
)

if not exist src\Generated mkdir src\Generated

pushd src\Generated
del ShaderByteCode_*.h
popd

pushd src
fxc -nologo -FhGenerated\ShaderByteCode_QuadSampleVertex.h -EQuadSampleVertex -VnShaderByteCode_QuadSampleVertex -Tvs_5_0 Shader.hlsl
fxc -nologo -FhGenerated\ShaderByteCode_QuadSamplePixel.h -EQuadSamplePixel -VnShaderByteCode_QuadSamplePixel -Tps_5_0 Shader.hlsl

fxc -nologo -FhGenerated\ShaderByteCode_QuadFillVertex.h -EQuadFillVertex -VnShaderByteCode_QuadFillVertex -Tvs_5_0 ShaderFill.hlsl
fxc -nologo -FhGenerated\ShaderByteCode_QuadFillPixel.h -EQuadFillPixel -VnShaderByteCode_QuadFillPixel -Tps_5_0 ShaderFill.hlsl
popd

pushd src
cl %copts% %code% -Fo..\bin\obj\ -c -EHsc
popd
pushd bin\obj
link %lopts% %objs% %libs%
popd
