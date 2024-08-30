-- premake5.lua
workspace "RayTracer"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "RayTracer"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

include "WalnutExternal.lua"
include "RayTracer"