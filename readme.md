[![Build Status](https://api.travis-ci.org/rvt/statemachine.svg?branch=master)](https://www.travis-ci.org/rvt/scriptrunner)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

# Small library to create a simply wau to run arbitary 'scripts' on arduino

Why?!
I needed to automate my gaggia coffee machine and I needed to have a way to run several 'scripts' for specific functions.
Scripts like open valve, close valse after XX milliseconds. Display a message etc.. etc..
If I needed to program that with specific functions/classes then I would need for each function modify and compile the code.
With this scripts I can give the coffee machine commands and it would 'execute' these commands by calling c++ function that are binded with a small command interpreter.
I can now make commands to purge water from the machine, or with the same script make cappucino. The idea is that the script can be extended, by itself the scripting language cannot do anything but it is relative simple to add functions to it.


## Example
