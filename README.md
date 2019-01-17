# sequential-environment
source for sequential environment project

This project drives a custom light sculpture designed for the Ithaca Art in the Heart exhibition in 2016.  It uses a raspberry pi to control a fadecandy device, which drives a linear sequence of LEDs.

To install this project, 
create a new raspbian filesystem.  
install node and npm
Copy the entire directory into a user home as sequential-environment.
Move the included fswebcam directory out to be a sibling of sequential-environment.

now, your home directory should have (something like) the following children:
/home/pi/sequential-environment
and
/home/pi/fswebcam

fswebcam should work, but if it must be built, just satisfy the library dependencies per the fswebcam project, and then sudo configure, sudo make, and sudo make install in the fswebcam directory.

To start the lights, invoke ./start-lights

To run the included process monitor ( for automatic restarts ), install ps-node and child_process globally with npm.  A restart may be needed.
modify rc.local to call the process monitor like this:
sudo /usr/bin/node /home/pi/sequential-environment/process-monitor.js
