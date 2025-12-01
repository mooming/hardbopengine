# HardBop Engine

<p>
 It's not a trivial work to produce a multiplatform software that works with Unix/Linux, Windows, and Mac OS.
Many game engines multiple platforms more than that though, they are very complex and huge to use for casual applications.
</p>

<p>
 This project is created for my personal software development and testing bed for study and experiments. Therefore, it
should be simple and easy to learn and use if users are familiar with standard C/C++ library.
</p>

# How to build
C++ 23 and Vulkan SDK are required to build the engine.

## Mac OS X
Latest xcode and Apple clang are required to build C++ 23 features.

## Linux
g++ 15 is required to build C++ 23 features

# Directory Structure

Hardbop Engine<br>
|<br>
|- Applications: Base directory for applications based on the hardbop engine.<br>
|- Source: Base engine directory<br>
|- External: Base directory for external solutions<br>


# Modules

<p>
Basically, each module is an individual library.
</p>

## Config

This module provides a simple key-value database for all tweakable config values.
ConfigParam could be thread-safe if IsAtomic=true.
ConfigFile represents a config file. It can have only string values.


