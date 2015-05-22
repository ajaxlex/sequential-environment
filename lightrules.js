#!/usr/bin/env node

var OPC = new require('./opc')

var fc = new OPC('localhost', 7890);

var sp = require('serialport');
var SerialPort = require('serialport').SerialPort;
var portName = "/dev/ttyArduino";

var runState = "down";

var BRIGHTEN = 100;
var DARKEN = 101;

var BRIGHTEN_S = 200;
var DARKEN_S = 201;

var SWEEP = 300;



var pixelLength = 300;
var particleCount = 200;

var environmentLength = 2000;

var defaultScale = 7;

var pixels = [];
var particles = [];

var lookup = [];

var coordT = [];

var dist_v = [];

var timerIdle = true;



// MAIN COMPUTER PROGRAM!

initialize();
setInterval(evaluate, 30);


function initialize() {
	serialport = new SerialPort(portName , { baudrate : 115200, parser: sp.parsers.readline("|") }, false);
	openPort();

	// Init coord lookup
	var coordRatio = ( ( pixelLength ) / environmentLength );
	for ( var t=0; t<environmentLength+1; t++ ){ coordT[t] = Math.floor( t * coordRatio ); }

	// Init scaling lookup

	lookup[defaultScale] = { 'environmentLength': defaultScale * pixelLength, 'positions': [] };

	for ( var p=0; p < lookup[defaultScale].environmentLength; p++ ){
		var div = p / defaultScale;
		var calc = {
			'left': Math.floor( div ),
			'right': Math.floor( div ) + 1,
			'wleft': 1 - ( div ) % 1,
			'wright': ( div ) % 1
		};

		lookup[defaultScale].positions.push( calc );
	}

	// Init pixel array
	for ( var i=0; i<pixelLength + 3; i++ ) { pixels.push({ 'r':0, 'g':0, 'b':0 }); }
}


function evaluate() {
	if ( runState == "open") {
  	updateParticles();
  	evaluateEnvironment();
  	draw();
	} else {
		if ( timerIdle ) {
			setTimeout( function() {
				openPort();
				timerIdle = true;
			}, 5000);
			timerIdle = false;
		}
	}
}


function updateParticles() {
  for ( var i=0; i<particles.length; i++ ){
		particles[i].update();
  }
}

function evaluateEnvironment() {
	// fill particle list
	if ( particles.length < particleCount ) {
		particles.push( getRandParticle() );
	}
}

function draw() {
  if ( runState == "open" ) {

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
	//		runState = "fc write err";
	//	}

  } else {
    // display runstate graphics

  }
}




// update methods

function update_Discrete() {
	particles[i].position += particles[i].vel;

	// recycle rules
	if ( this.position > environmentLength ) {
		this = getRandParticle();
	}
}

function update_Smooth() {
	particles[i].position += particles[i].vel;

	// recycle rules
	if ( this.position > lookup[this.scale].environmentLength-1 ) {
		this = getRandParticle();
	}
}

function update_Glower() {
	particles[i].position += particles[i].vel;

	// recycle rules
	if ( this.position > lookup[this.scale].environmentLength-1 ) {
		this = getRandParticle();
	}
}







// draw methods

function method_Brighten() {
	var dest = coordT[this.position];

	pixels[dest].red += this.color.r;
	pixels[dest].green += this.color.g;
	pixels[dest].blue += this.color.b;
}

function method_Darken() {
	var dest = coordT[this.position];

	pixels[dest].red -= this.color.r;
	pixels[dest].green -= this.color.g;
	pixels[dest].blue -= this.color.b;
}

function method_BrightenSmooth() {
	var pcalc = lookup[this.scale].positions[this.position];
	methodValueSmooth( pcalc );
}

function method_DarkenSmooth() {
	var pcalc = lookup[this.scale].positions[this.position];
	if ( this.method == DARKEN_S ) {
		pcalc.wleft *= -1;
		pcalc.wright *= -1;
	}
	methodValueSmooth( pcalc );
}

function methodValueSmooth( pcalc ){
	pixels[pcalc.left].red += this.color.r * pcalc.wleft;
	pixels[pcalc.left].green += this.color.g * pcalc.wleft;
	pixels[pcalc.left].blue += this.color.b * pcalc.wleft;

	pixels[pcalc.right].red += this.color.r * pcalc.wright;
	pixels[pcalc.right].green += this.color.g * pcalc.wright;
	pixels[pcalc.right].blue += this.color.b * pcalc.wright;
}


function method_Sweep() {
	var pcalc = lookup[this.scale].positions[this.position];

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



function getRandParticle() {
	var r = Math.random();

	if ( r < .10 ) {
		return getParticle( 4, 0, 1, method_BrightenSmooth, update_Smooth, defaultScale, 10, 10, 85 );
	} else if ( r < .15 ) {
		return getParticle( 3, 0, 1, method_Sweep, update_Smooth, defaultScale, 60, 10, 15 );
	} else {
		return getParticle( 2, 0, 1, method_BrightenSmooth, update_Smooth, defaultScale, 5, 5, 10 );
	}
}

function getParticle( v, p, r, g, b ) {
	return getParticle( v, p, 1, method_Brighten, update_Discrete, defaultScale, r, g, b );
}

function getParticle( v, p, i, m, u, s, r, g, b ) {
  var p = {
    'vel': v,
    'position': p,
    'intensity': i,
		'method': m,
		'update': u
		'scale': s,
    'color': { 'r': r, 'g': g, 'b': b }
  };
  return p;
}







// Serial Access

function listPorts() {
  serialport.list(function (err, ports) {
    ports.forEach(function(port) {
      console.log(port.comName);
    });
  });
  return ports;
}

function openPort() {
	serialport.on('error', function(err){
		console.log('serialport general error' + err);
	});

  serialport.open(function (error) {
    if ( error ) {
      console.log('open err: ' + error);
//			runState = "conn open err";
	runState = "open";
    } else {
      console.log('open');
			runState = "open";

      serialport.on('data', function(data) {
    		var c = data.trim();
				try{
					//console.log('rcv: ' + c);
	    		dist_v = JSON.parse(c);
					//dist_v = {};
	    		//console.log(' 12: ' + dist_v[12] );
				} catch( err ){
					console.log('parse err ' + err);
				}
	    });

    }
  });

}
