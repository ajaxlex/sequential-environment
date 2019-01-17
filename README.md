# sequential-environment
source for sequential environment project

This project drives a custom light sculpture designed for the Ithaca Art in the Heart exhibition in 2016.  It uses a raspberry pi to control a fadecandy device, which drives a linear sequence of LEDs.

To install this project, 
create a new raspbian filesystem.  
Copy the entire directory into a user home as sequential-environment.
Move the included fswebcam directory out to be a sibling of sequential-environment.

fswebcam should work, but if it must be built, just satisfy the library dependencies per the fswebcam project, and then sudo configure, sudo make, and sudo make install in the fswebcam directory.

To start the lights, invoke ./start-lights

