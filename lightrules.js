#!/usr/bin/env node

var OPC = new require('./opc');
var fc = new OPC('localhost', 7890);
fs = require('fs');

var SENSORTEST = false;
var POSITIONTEST = false;
var USESENSOR = true;

var sensor = {};

if ( USESENSOR ) {
	//sensor = new ultrasonicSensors();
	sensor = new visualSensors();
} else {
	sensor = new testSensors();
}

var context = { runState: "down" };

var BRIGHTEN = 100;
var DARKEN = 101;

var BRIGHTEN_S = 200;
var DARKEN_S = 201;

var SWEEP = 300;


var firstCapture = true;

var sensorCount = 0;

var pixelLength = 300;
var particleCount = 700;

var defaultScale = 13;

var environmentLength = defaultScale * pixelLength;


var pixels = [];
var particles = [];

var lookup = {};
var coordT = [];
var dist_v = [];
var dist_back = [];
var dist_recent = [];

var timerIdle = true;


///////////
// MAIN COMPUTER PROGRAM!
///////////

initialize();
setInterval(evaluate, 30);

function initialize() {

	if ( process.pid ) {
		fs.writeFile( 'light_pid.txt', process.pid );
	}

	if ( process.argv.length > 2 ) {
			for ( var i=2; i<process.argv.length; i++ ) {
				var current = process.argv[i];
				if ( current == "sensorTest" ) { SENSORTEST = true; }
				else if ( current == "positionTest" ) { POSITIONTEST = true; }
				else if ( current == "noSensors") { USESENSOR = false; }
			}
	}

	sensorCount = sensor.initialize( dist_v, context );

	for ( var i=0; i < sensorCount; i++ ) { dist_v[i] = 0; }

	// Init coord lookup
	var coordRatio = ( pixelLength  / environmentLength );
	for ( var t=0; t<environmentLength+1; t++ ){ coordT[t] = Math.floor( t * coordRatio ); }

	// Init scaling lookup

	lookup = { 'environmentLength': defaultScale * pixelLength, 'positions': [] };

	for ( var p=0; p < lookup.environmentLength; p++ ){
		var div = p / defaultScale;
		var calc = {
			'left': Math.floor( div ),
			'right': Math.floor( div ) + 1,
			'wleft': 1 - ( div ) % 1,
			'wright': ( div ) % 1
		};

		lookup.positions.push( calc );
	}

	// Init pixel array
	for ( var i=0; i<pixelLength + 3; i++ ) { pixels.push({ 'r':0, 'g':0, 'b':0 }); }
}


function evaluate() {
	if ( context.runState == "open") {
		evaluateEnvironment();
  	updateParticles();
  	draw();
	} else {
		if ( timerIdle ) {
			setTimeout( function() {
				sensor.connect( dist_v, context );
				timerIdle = true;
			}, 5000);
			timerIdle = false;
		}
	}
}


function updateParticles() {
  for ( var i=0; i<particles.length; i++ ){
		particles[i].update( i );
  }
	for ( var i=particles.length - 1; i >= 0; i-- ){
		if ( particles[i].life < 1 ) {
			particles.splice(i,1);
		}
	}
}

function evaluateEnvironment() {
	// fill particle list
	if ( particles.length < particleCount ) {
		addParticle( getRandParticle() );
	}

	for ( var s=0; s < sensorCount; s++ ) {
			if ( dist_v[s] > 0 && dist_v[s] < 150 ) {
				particles.push( getProximateParticle( s, dist_v[s] ));
			}
	}

	sensor.update( dist_v );

	if ( POSITIONTEST ) {
		var out = "";
		for ( var s=0; s < sensorCount; s++ ) {
			var p = getProximateParticle( s, dist_v[s] );
			out += s + "=" + dist_v[s] + "-" + p.position + "  :  ";
		}
		console.log( out );
	}

}

function draw() {
  if ( context.runState == "open" ) {

		// Initialize
		initAllPixels( 20, 20, 40 );

	  // Compose
	  for ( var i=0; i<particles.length; i++ ){ particles[i].method(); }

	  // Render
	//	try {
	    for ( var i=0; i<pixels.length; i++ ){
	      fc.setPixel( pixels.length-i, pixels[i].red, pixels[i].green, pixels[i].blue );
	    }
	    fc.writePixels();
	//	} catch( err ) {
	//		console.log('fc write err: ' + error);
	//		context.runState = "fc write err";
	//	}

  } else {
    // display runstate graphics
  }
}




///////////
// particle methods
///////////

function update_Discrete( i ) {
	this.position += this.vel;

	// recycle rules
	if ( this.position > environmentLength-1 ) {
		this.life = 0;
	}
}

function update_Smooth( i ) {
	this.position += this.vel;

	// recycle rules
	if ( this.position > environmentLength-1 ) {
		this.life = 0;
	}
}

function update_Glower( i ) {
	this.position += this.vel;

	// recycle rules
	if ( this.position > environmentLength-1 ) {
		this.life = 0;
	}
}

function update_React( i ) {
	this.position += this.vel;
	this.life--;
	this.intensity -= .01;
	if ( this.intensity < 0 ) { this.intensity = 0; }

	// recycle rules
	if ( this.position < 1 || this.position > environmentLength-1 || this.life < 1 ) {
		this.life = 0;
	}
}




// draw methods

function method_Brighten() {
	var dest = coordT[this.position];

	pixels[dest].red += this.color.r * this.intensity;
	pixels[dest].green += this.color.g * this.intensity;
	pixels[dest].blue += this.color.b * this.intensity;
}

function method_Darken() {
	var dest = coordT[this.position];

	pixels[dest].red -= this.color.r * this.intensity;
	pixels[dest].green -= this.color.g * this.intensity;
	pixels[dest].blue -= this.color.b * this.intensity;
}

function method_BrightenSmooth() {
	var pcalc;
	try {
		pcalc = lookup.positions[this.position];
		methodValueSmooth( this, pcalc );
	} catch( err ) {
			console.log('Brighten Smooth: ' + err);
			console.log( this.position );
			process.exit();
	//		context.runState = "fc write err";
		}
}

function method_DarkenSmooth() {
	var pcalc = lookup.positions[this.position];
	if ( this.method == DARKEN_S ) {
		pcalc.wleft *= -1;
		pcalc.wright *= -1;
	}
	methodValueSmooth( this, pcalc );
}

function methodValueSmooth( tgt, pcalc ){
	pixels[pcalc.left].red += tgt.color.r * pcalc.wleft * tgt.intensity;
	pixels[pcalc.left].green += tgt.color.g * pcalc.wleft * tgt.intensity;
	pixels[pcalc.left].blue += tgt.color.b * pcalc.wleft * tgt.intensity;

	pixels[pcalc.right].red += tgt.color.r * pcalc.wright * tgt.intensity;
	pixels[pcalc.right].green += tgt.color.g * pcalc.wright * tgt.intensity;
	pixels[pcalc.right].blue += tgt.color.b * pcalc.wright * tgt.intensity;
}


function method_Sweep() {
	var pcalc = lookup.positions[this.position];

	var taillen = 6;
	var fraction = 1/taillen;

	for ( var t=0; t < taillen; t++ ) {
		try {
		var tail = pcalc.right - t;
	} catch ( err ) {
		console.log( i + " | " + this.position );
	}
		if ( tail > 0 ) {
			var factor = ( 6 - t ) * ( fraction );
			pixels[tail].red += this.color.r * factor;
			pixels[tail].green += this.color.g * factor;
			pixels[tail].blue += this.color.b * factor;
		}
	}
	var frontedge = pcalc.right + 1;
	if ( frontedge < pixelLength - 2 ) {
		pixels[frontedge].red = 0;
		pixels[frontedge].green = 0;
		pixels[frontedge].blue = 0;
	}
}













function setPixel( i, r, g, b ) {
	pixels[i].red = r;
	pixels[i].green = g;
	pixels[i].blue = b;
}

function initAllPixels( r, g, b ){
	for ( var i=0; i<pixels.length; i++ ) {
		pixels[i].red = r;
		pixels[i].green = g;
		pixels[i].blue = b;
	}
}

function addParticle( p ) {
	particles.push( p );
}

function getProximateParticle( s, dist ) {
	var pos = sensor.getPosition( s );
	//Math.floor( ( s + 1 ) * ( environmentLength / sensorCount ) );
	var life = 150 - dist;

	return getParticle( 4, pos, 1,  method_BrightenSmooth, update_React, defaultScale, 85, 55, 15, life );
}

function getRandParticle() {
	var r = Math.random();

	if ( r < .0125 ) {
		return getParticle( 7, 0, 1, method_BrightenSmooth, update_Smooth, defaultScale, 128, 10, 128, 1 );
	} else if ( r < .10 ) {
		return getParticle( 5, 0, 1, method_BrightenSmooth, update_Smooth, defaultScale, 10, 10, 85, 1 );
//	} else if ( r < .15 ) {
//		return getParticle( 3, 0, 1, method_Sweep, update_Smooth, defaultScale, 60, 10, 15, 1 );
//	} else if ( r < .30 ) {
//		return getParticle( 3, 0, 1, method_Sweep, update_Smooth, defaultScale, 30, 30, 95, 1 );
	} else if ( r < .50 ) {
		return getParticle( 4, 0, 1, method_BrightenSmooth, update_Smooth, defaultScale, 30, 25, 100, 1 )
	} else {
		return getParticle( 2, 0, 1, method_BrightenSmooth, update_Smooth, defaultScale, 5, 5, 10, 1 );
	}
}

function getParticle( v, p, r, g, b ) {
	return getParticle( v, p, 1, method_Brighten, update_Discrete, defaultScale, r, g, b, 1 );
}

function getParticle( v, p, i, m, u, s, r, g, b, l ) {
  var p = {
    'vel': v,
    'position': p,
    'intensity': i,
		'method': m,
		'update': u,
		'scale': s,
    'color': { 'r': r, 'g': g, 'b': b },
		'life': l
  };
  return p;
}







///////////
// Visual Sensor Code
///////////

function visualSensors(){
	var self = this;
	self.positions = [];
	self.delimiter = String.fromCharCode(254);
	self.readable = false;


	self.initialize = function( dist_v, context ) {

		process.stdin.setEncoding('utf8');
	  process.stdin.on('readable', function() {
	    self.readable = true;
	  });
	  process.stdin.on('end', function() {
	    self.readable = false;
	    process.stdout.write('end');
	    console.log('end');
	  });

		var scaling = environmentLength / 35;

		for ( var p=0; p < 35; p++ ){
			self.positions.push( Math.floor(p * scaling) )
		}

		return 35;
	}

	self.connect = function( dist_v, context ) {
				context.runState = "open"
	}

	self.update = function( dist_v ) {
		if ( self.readable ) {
	    var chunk = process.stdin.read();
	    if (chunk !== null) {
	      var arr = [];

	      var sub = chunk.substring(0,71);
	      var segments = sub.split( self.delimiter );

	      var rebuilt = segments[0];

	      for ( var t=0; t < 35; t++ ){
	          dist_v[35-t] = rebuilt[t*2].charCodeAt();
	      }

				//console.log('\033[0;0H');
	      self.writeGrid( dist_v );
	    }
	  }
	}

	self.getPosition = function( p ) {
		if ( p < 0 || p >= self.positions.length ) {
			return 0;
		}
		return self.positions[p];
	}

	self.writeGrid = function( arr ){
	  for ( var y = 0; y < 40; y++ ){
	    out = "";
	    for ( var x = 0; x < 35; x++ ){
	      if ( arr[x] > 0 && arr[x] == y ){
	        out += "#";
	      } else {
	        out += ".";
	      }
	    }
	    console.log(out);
	  }
	}

}



///////////
// Ultrasonic Sensor Code
///////////



function getPosition( p ) {

	var positions = [
		100, 974, 1461, 1948, 2435, 2922, 3409, 3896
	];

	if ( p >= 0 && p < positions.length ) {
		return positions[ p ];
	} else {
		return 487;
	}
}


function ultrasonicSensors() {
	var self = this;

	var sp = require('serialport');
	var SerialPort = require('serialport').SerialPort;
	var portName = "/dev/ttyArduino";

	var serialPort = {};

	self.initialize = function( dist_v, context ) {
		serialport = new SerialPort(portName , { baudrate : 115200, parser: sp.parsers.readline("|") }, false);
		self.connect( dist_v, context );
		return 8;
	}

	self.listPorts = function() {
		serialport.list(function (err, ports) {
	    ports.forEach(function(port) {
	      console.log(port.comName);
	    });
	  });
	  return ports;
	}

	self.connect = function( dist_v, context ) {
		serialport.on('error', function(err){
			console.log('serialport general error' + err);
		});

	  serialport.open(function (error) {
	    if ( error ) {
	      console.log('open err: ' + error);
	//			context.runState = "conn open err";
				context.runState = "open";
	    } else {
	      console.log('open');
				context.runState = "open";

	      serialport.on('data', function(data) {
	    		var c = data.trim();
					try{

		    		dist_v = JSON.parse(c);

	//					if ( firstCapture ){
	//						dist_back = dist_v.slice();
	//						firstCapture = false;
	//					}

						if ( SENSORTEST ) {
							var out = "";
							for ( var s=0; s < sensorCount; s++ ) {
								out += s + '=' + dist_v[s] + '  :  ';
							}
							console.log( out );
						}
					} catch( err ){
						console.log('parse err ' + err);
					}
		    });
	    }
	  });
	}

	self.update = function( dist_v ) {}

	self.getPosition = getPosition;

};


function testSensors() {
	var self = this;

	self.initialize = function( dist_v, context ) {
		dist_v = [ 0,0,0,0,0,0,0,0 ];
		return 8;
	}

	self.connect = function( dist_v, context ) {
		/*
		var keypress = require('keypress');

		console.log('assigning keyhandler');

		// listen for the "keypress" event
		process.stdin.on('keypress', function (ch, key) {
		  console.log('got "keypress"', key);
		  //if (key.ctrl && key.name == 'c') {
		  //  process.stdin.pause();
		  //}
			if ( key.name == '1' ) {
				dist_v[0] = 60;
			}
		});
		//process.stdin.resume();
		*/

		var t = 6000;
		var tfunc = function(){
			t = ( Math.random() * 4 ) * 1000;
			t += 2000;
			dist_v[0] = 20;
		}

		setTimeout( tfunc, t);


		setTimeout( function() {
			dist_v[0] = 20;
		}, 10 );


		context.runState = "open";

	}

	self.update = function( dist_v ) {
		dist_v = [ 20,0,0,0,0,0,20,0 ];
	}

	self.getPosition = getPosition;

};
