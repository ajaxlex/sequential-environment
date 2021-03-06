# sequential-environment
source for sequential environment project

This project drives a custom light sculpture designed for the Ithaca Art in the Heart exhibition in 2016.  It uses a raspberry pi to control a fadecandy device, which drives a linear sequence of LEDs.

To install this project, 
* create a new raspbian filesystem.  
* install node and npm
* Copy the entire directory into a user home as sequential-environment.
* Move the included fswebcam directory out to be a sibling of sequential-environment.

now, your home directory should have (something like) the following children:
/home/pi/sequential-environment
and
/home/pi/fswebcam

The code for fswebcam is modified for this project.  the fswebcam executable should work, but if it must be built, just satisfy the library dependencies per the fswebcam project, and then

* cd /home/pi/fswebcam
* sudo configure
* sudo make
* sudo make install 

To start the lights, 
* cd /home/pi/sequential-environment
* ./start-lights

To run the included process monitor ( for automatic restarts ), navigate inside the sequential-environment directory and then install ps-node and child_process with npm.  

* cd /home/pi/sequential-environment
* sudo npm install ps-node
* sudo npm install child_process

finally, modify /etc/rc.local to call the process monitor by adding the following to the end:
sudo /usr/bin/node /home/pi/sequential-environment/process-monitor.js
