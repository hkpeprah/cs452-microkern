Real-time Micro-Kernel (CS452)
==============================

## Details  
To see details about how to operate the program, please see the non-markdown README file in the current directory.  This is a micro-kernel written for CS452 (Real-Time Programming) at the University of Waterloo.  The purpose of the kernel is to provide tools for driving trains on a track; routing them to destinations, determining where they are on the track, and writing programs that interact with the train set.  

### Servers
* **TrainController* - Routing of trains and synchronization of track resources.  
* **ClockServer* - Keeps track of time and provides wait functionality.  
* **SensorServer* - Listens for sensors being triggerd on teh track and provies tools to wait on them.  
* **NameServer* - Provides lookup/registration services for tasks to find one another.  
* **RPSServer* - An implementation of a RockPaperScissors game engine.  

## Authors  
Max Chen: http://github.com/maxqchen  
Ford Peprah: http://github.com/hkpeprah

## Makefile (Making)
### Uploading  
Run `make upload`  
**Note**: This also compiles.  

### Compiling  
Run `make`  
  
### Testing  
Run `make test TEST="testfilename"`  
  
### Debugging  
Run `make debug`  

### Flags  
* `SILENT` - Pass either `false` or `true` to disable `DEBUG` if making test, otherwise to disable Makefile linking output.  
* `PROFILING` - Pass additional profiling defines.  

### Documentation  
Run `doxygen Doxyfile`  

## Useful Links  
* Kernel Description: http://www.cgl.uwaterloo.ca/~wmcowan/teaching/cs452/pdf/kernel.pdf
